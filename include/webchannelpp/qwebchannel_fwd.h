/*
 * This file is part of WebChannel++.
 * Copyright (C) 2016 - 2018, Menlo Systems GmbH
 * Copyright (C) 2016 The Qt Company Ltd.
 * Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company
 * License: Dual-licensed under LGPLv3 and GPLv2+
 */

#ifndef QWEBCHANNEL_FWD_H
#define QWEBCHANNEL_FWD_H

#include <functional>
#include <iostream>
#include <map>

#ifndef WEBCHANNELPP_USE_GLOBAL_JSON
#include "nlohmann/json.hpp"
#else
#include <json.hpp>
#endif

namespace WebChannelPP
{

/// @brief Abstract transport class.
///
/// The transport calls the registered message handle when a new message arives.
/// `send` sends a message over the transport.
template<class String>
class BasicTransport
{
public:
    typedef std::function<void(const String &)> message_handler;

    virtual void send(const String &s) = 0;
    virtual void register_message_handler(message_handler handler) = 0;
};

using Transport = BasicTransport<std::string>;

/// @brief Thin helper class for implicitly converting json data types
template<class Json>
struct json_unwrap
{
    const Json &_json;

    explicit json_unwrap(Json &&j) : _json(j) {}
    explicit json_unwrap(const Json &j) : _json(j) {}

    const Json &json() const { return _json; }

    template<class T>
    operator T() { return _json.template get<T>(); }
};

enum BasicQWebChannelMessageTypes {
    QSignal = 1,
    PropertyUpdate = 2,
    Init = 3,
    Idle = 4,
    Debug = 5,
    InvokeMethod = 6,
    ConnectToSignal = 7,
    DisconnectFromSignal = 8,
    SetProperty = 9,
    Response = 10,
};

template<class Json>
class BasicQObject;

template<class Json = nlohmann::json>
class BasicQWebChannel
{
public:
    using json_t = Json;
    using string_t = typename json_t::string_t;

    typedef std::function<void(BasicQWebChannel*)> InitCallbackHandler;
    typedef std::function<void(const json_t &)> CallbackHandler;

    /// @brief Initializes the webchannel with the given `transport`. Optionally, an `initCallback`
    ///        can be invoked when the webchannel has successfully been initialized.
    BasicQWebChannel(BasicTransport<string_t> &transport, InitCallbackHandler initCallback = InitCallbackHandler());

    /// @brief Returns a map of all objects exported by the webchannel
    const std::map<string_t, BasicQObject<json_t>*> &objects() const { return _objects; }

    /// @brief Returns the object with @p name or nullptr if it doesn't exist
    BasicQObject<json_t> *object(const string_t &name) const;

    /// @brief Returns whether property caching is enabled
    bool property_caching() const { return propertyCachingEnabled; }

    /// @brief Enables or disables property caching
    void set_property_caching(bool enabled) { propertyCachingEnabled = enabled; }

    /// @brief Returns whether auto-idling after processing property updates is enabled
    bool auto_idle() const { return _autoIdle; }
    /// @brief Enable or disable auto-idling after processing property updates
    void set_auto_idle(bool enabled);
    /// @brief Explicitly notify the host that the client is idle
    void idle();

private:
    void connection_made(const json_t &data);
    void message_handler(const string_t &msg);

    void send(const json_t &o);
    void exec(json_t data, CallbackHandler callback = CallbackHandler());

    void handle_signal(const json_t &message);
    void handle_response(const json_t &message);
    void handle_property_update(const json_t &message);

    void debug(const json_t &message)
    {
        this->send(json_t { { "type", BasicQWebChannelMessageTypes::Debug }, { "data", message } });
    }

    friend class BasicQObject<json_t>;

    BasicTransport<string_t> &transport;

    std::map<string_t, BasicQObject<json_t>*> _objects;

    InitCallbackHandler initCallback;
    std::map<unsigned int, CallbackHandler> execCallbacks;
    unsigned int execId = 0;
    bool propertyCachingEnabled = true;
    bool _autoIdle = true;
};

using QWebChannel = BasicQWebChannel<>;

}

#endif // QWEBCHANNEL_FWD_H
