#ifndef QWEBCHANNEL_IMPL_H
#define QWEBCHANNEL_IMPL_H

#include "qwebchannel_fwd.h"
#include "qobject_fwd.h"

namespace QWebChannelPP
{

QWebChannel::QWebChannel(Transport &transport, InitCallbackHandler initCallback)
    : transport(transport), initCallback(initCallback)
{
    transport.register_message_handler(std::bind(&QWebChannel::message_handler, this, std::placeholders::_1));

    this->exec(json { { "type", QWebChannelMessageTypes::Init } },
               std::bind(&QWebChannel::connection_made, this, _1));
}


void QWebChannel::connection_made(const json &data)
{
    for (auto prop = data.begin(); prop != data.end(); ++prop) {
        new QObject(prop.key(), prop.value(), this);
    }

    // now unwrap properties, which might reference other registered objects
    for (const auto &kv : _objects) {
        kv.second->unwrapProperties();
    }
    if (initCallback) {
        initCallback(this);
    }

    this->exec(json { { "type", QWebChannelMessageTypes::Idle } });
}


void QWebChannel::send(const json &o)
{
    transport.send(o.dump());
}


void QWebChannel::message_handler(const std::string &msg)
{
    json data = json::parse(msg);

    switch (data["type"].get<int>())
    {
    case QWebChannelMessageTypes::QSignal:
        this->handle_signal(data);
        break;
    case QWebChannelMessageTypes::Response:
        this->handle_response(data);
        break;
    case QWebChannelMessageTypes::PropertyUpdate:
        this->handle_property_update(data);
        break;
    default:
        std::cerr << "invalid message received: " << data << std::endl;;
        break;
    }
}


void QWebChannel::exec(json data, CallbackHandler callback)
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
    this->execCallbacks[data["id"].get<int>()] = callback;
    this->send(data);
}


void QWebChannel::handle_signal(const json &message)
{
    auto it = this->_objects.find(message["object"].get<std::string>());
    if (it != this->_objects.end()) {
        it->second->signalEmitted(message["signal"].get<int>(), message["args"]);
    } else {
        std::cerr << "Unhandled signal: " << message["object"] << "::" << message["signal"] << std::endl;
    }
}


void QWebChannel::handle_response(const json &message)
{
    if (!message.count("id")) {
        std::cerr << "Invalid response message received: " << message << std::endl;
        return;
    }

    this->execCallbacks[message["id"].get<int>()](message["data"]);
    this->execCallbacks.erase(message["id"].get<int>());
}


void QWebChannel::handle_property_update(const json &message)
{
    for (const json &data : message["data"]) {
        auto it = this->_objects.find(data["object"].get<std::string>());
        if (it != this->_objects.end()) {
            it->second->propertyUpdate(data["signals"], data["properties"]);
        } else {
            std::cerr << "Unhandled property update: " << data["object"] << "::" << data["signal"] << std::endl;
        }
    }

    this->exec(json { { "type", QWebChannelMessageTypes::Idle } } );
}

}

#endif // QWEBCHANNEL_IMPL_H
