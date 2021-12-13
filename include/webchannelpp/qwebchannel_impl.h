/*
 * This file is part of WebChannel++.
 * Copyright (C) 2016 - 2018, Menlo Systems GmbH
 * Copyright (C) 2016 The Qt Company Ltd.
 * Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company
 * License: Dual-licensed under LGPLv3 and GPLv2+
 */

#ifndef QWEBCHANNEL_IMPL_H
#define QWEBCHANNEL_IMPL_H

#include "qwebchannel_fwd.h"
#include "qobject_fwd.h"

namespace WebChannelPP
{

template<class Json>
inline BasicQWebChannel<Json>::BasicQWebChannel(BasicTransport<string_t> &transport, InitCallbackHandler initCallback)
    : transport(transport), initCallback(initCallback)
{
    transport.register_message_handler(std::bind(&BasicQWebChannel::message_handler, this, std::placeholders::_1));

    this->exec(json_t { { "type", BasicQWebChannelMessageTypes::Init } },
               std::bind(&BasicQWebChannel::connection_made, this, std::placeholders::_1));
}

template<class Json>
inline void BasicQWebChannel<Json>::set_auto_idle(bool enabled)
{
    _autoIdle = enabled;
    if (_autoIdle) {
        idle();
    }
}

template<class Json>
inline void BasicQWebChannel<Json>::idle()
{
    this->exec(json_t { { "type", BasicQWebChannelMessageTypes::Idle } } );
}

template<class Json>
inline void BasicQWebChannel<Json>::connection_made(const json_t &data)
{
    for (auto prop = data.begin(); prop != data.end(); ++prop) {
        new BasicQObject<json_t>(prop.key(), prop.value(), this);
    }

    // now unwrap properties, which might reference other registered objects
    for (const auto &kv : _objects) {
        kv.second->unwrapProperties();
    }
    if (initCallback) {
        initCallback(this);
    }

    this->exec(json_t { { "type", BasicQWebChannelMessageTypes::Idle } });
}


template<class Json>
inline BasicQObject<Json> *BasicQWebChannel<Json>::object(const string_t &name) const
{
    auto it = _objects.find(name);
    if (it == _objects.end()) {
        return nullptr;
    }
    return it->second;
}


template<class Json>
inline void BasicQWebChannel<Json>::send(const json_t &o)
{
    transport.send(o.dump());
}


template<class Json>
inline void BasicQWebChannel<Json>::message_handler(const string_t &msg)
{
    json_t data = json_t::parse(msg);

    switch (data["type"].template get<int>())
    {
    case BasicQWebChannelMessageTypes::QSignal:
        this->handle_signal(data);
        break;
    case BasicQWebChannelMessageTypes::Response:
        this->handle_response(data);
        break;
    case BasicQWebChannelMessageTypes::PropertyUpdate:
        this->handle_property_update(data);
        break;
    default:
        std::cerr << "invalid message received: " << data << std::endl;;
        break;
    }
}


template<class Json>
inline void BasicQWebChannel<Json>::exec(json_t data, CallbackHandler callback)
{
    if (!callback) {
        // if no callback is given, send directly
        this->send(data);
        return;
    }

    if (data.count("id")) {
        std::cerr << "Cannot exec message with property \"id\": " << data << std::endl;
        return;
    }

    data["id"] = this->execId++;
    this->execCallbacks[data["id"].template get<int>()] = callback;
    this->send(data);
}


template<class Json>
inline void BasicQWebChannel<Json>::handle_signal(const json_t &message)
{
    auto it = this->_objects.find(message["object"].template get<string_t>());
    if (it != this->_objects.end()) {
        it->second->signalEmitted(message["signal"].template get<int>(), message.value("args", typename Json::array_t{}));
    } else {
        std::cerr << "Unhandled signal: " << message["object"] << "::" << message["signal"] << std::endl;
    }
}


template<class Json>
inline void BasicQWebChannel<Json>::handle_response(const json_t &message)
{
    if (!message.count("id")) {
        std::cerr << "Invalid response message received: " << message << std::endl;
        return;
    }

    this->execCallbacks[message["id"].template get<int>()](message["data"]);
    this->execCallbacks.erase(message["id"].template get<int>());
}


template<class Json>
inline void BasicQWebChannel<Json>::handle_property_update(const json_t &message)
{
    for (const json_t &data : message["data"]) {
        auto it = this->_objects.find(data["object"].template get<string_t>());
        if (it != this->_objects.end()) {
            it->second->propertyUpdate(data["signals"], data["properties"]);
        } else {
            std::cerr << "Unhandled property updates: " << data["object"] << "::" << data["properties"] << std::endl;
        }
    }

    if (_autoIdle) {
        idle();
    }
}

}

#endif // QWEBCHANNEL_IMPL_H
