#ifndef QWEBCHANNEL_FWD_H
#define QWEBCHANNEL_FWD_H

#include <string>
#include <functional>
#include <iostream>
#include <map>

#include "json.hpp"

namespace QWebChannelPP
{

using json = nlohmann::json;
using namespace std::placeholders;

class Transport
{
public:
    typedef std::function<void(const std::string &)> message_handler;

    virtual void send(const std::string &s) = 0;
    virtual void register_message_handler(message_handler handler) = 0;
};

struct json_unwrap
{
    json _json;

    explicit json_unwrap(json &&j)
        : _json(j)
    {
    }

    explicit json_unwrap(const json &j)
        : _json(j)
    {
    }

    template<class T>
    operator T()
    {
        return _json.get<T>();
    }

    template<class T>
    operator T*()
    {
        if (_json.is_null()) {
            return nullptr;
        }

        if (!_json.is_object() || !_json.count("__ptr__")) {
            std::cerr << "JSON object does not point to a native object!" << std::endl;
            return nullptr;
        }

        return dynamic_cast<T*>(T::convert(_json["__ptr__"].get<std::uintptr_t>()));
    }
};

enum QWebChannelMessageTypes {
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

class QObject;

class QWebChannel
{
public:
    typedef std::function<void(QWebChannel*)> InitCallbackHandler;
    typedef std::function<void(const json &)> CallbackHandler;

    QWebChannel(Transport &transport, InitCallbackHandler initCallback = InitCallbackHandler());

    const std::map<std::string, QObject*> &objects() const { return _objects; }

private:
    void connection_made(const json &data);
    void message_handler(const std::string &msg);

    void send(const json &o);
    void exec(json data, CallbackHandler callback = CallbackHandler());

    void handle_signal(const json &message);
    void handle_response(const json &message);
    void handle_property_update(const json &message);

    void debug(const json &message)
    {
        this->send(json { { "type", QWebChannelMessageTypes::Debug }, { "data", message } });
    }

    friend class QObject;
    friend class QSignal;

    Transport &transport;

    std::map<std::string, QObject*> _objects;

    InitCallbackHandler initCallback;
    std::map<unsigned int, CallbackHandler> execCallbacks;
    unsigned int execId = 0;
};

}

#endif // QWEBCHANNEL_FWD_H
