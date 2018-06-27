/*
 * This file is part of WebChannel++.
 * Copyright (C) 2016 - 2018, Menlo Systems GmbH
 * Copyright (C) 2016 The Qt Company Ltd.
 * Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company
 * License: Dual-licensed under LGPLv3 and GPLv2+
 */

#ifndef QOBJECT_IMPL_H
#define QOBJECT_IMPL_H

#include <type_traits>

#include "qobject_fwd.h"

namespace WebChannelPP
{

// https://stackoverflow.com/questions/27866909/get-function-arity-from-template-parameter
namespace detail
{
template <typename T>
struct get_arity : get_arity<decltype(&T::operator())> {};
template <typename R, typename... Args>
struct get_arity<R(*)(Args...)> : std::integral_constant<unsigned, sizeof...(Args)> {};
template <typename R, typename C, typename... Args>
struct get_arity<R(C::*)(Args...)> : std::integral_constant<unsigned, sizeof...(Args)> {};
template <typename R, typename C, typename... Args>
struct get_arity<R(C::*)(Args...) const> : std::integral_constant<unsigned, sizeof...(Args)> {};
}


QObject::QObject(const std::string &name, const json &data, QWebChannel *channel)
    : __id__(name), _webChannel(channel)
{
    created_objects().insert(this);

    _webChannel->_objects[name] = this;

    if (data.count("methods")) {
        for (const json &method : data["methods"]) {
            addMethod(method);
        }
    }

    if (data.count("properties")) {
        for (const json &property : data["properties"]) {
            bindGetterSetter(property);
        }
    }

    if (data.count("signals")) {
        for (const json &signal : data["signals"]) {
            addSignal(signal, false);
        }
    }

    if (data.count("enums")) {
        _enums = data["enums"].get<decltype(_enums)>();
    }
}

QObject::~QObject()
{
    created_objects().erase(this);
}

std::set<std::string> QObject::methods() const {
    std::set<std::string> methNames;
    for (auto &kv : _methods) methNames.insert(kv.first);
    return methNames;
}

std::set<std::string> QObject::properties() const {
    std::set<std::string> propNames;
    for (auto &kv : _properties) propNames.insert(kv.first);
    return propNames;
}

std::set<std::string> QObject::signalNames() const {
    std::set<std::string> sigNames;
    for (auto &kv : _qsignals) sigNames.insert(kv.first);
    return sigNames;
}

std::set<QObject *> &QObject::created_objects() {
    static std::set<QObject*> set;
    return set;
}

QObject *QObject::convert(uintptr_t ptr) {
    QObject *obj = reinterpret_cast<QObject*>(ptr);
    if (created_objects().find(obj) == created_objects().end()) {
        return nullptr;
    }
    return obj;
}

void QObject::addMethod(const json &method)
{
    _methods[method[0].get<std::string>()] = method[1].get<int>();
}

void QObject::bindGetterSetter(const json &propertyInfo)
{
    int propertyIndex = propertyInfo[0];
    std::string propertyName = propertyInfo[1];
    json notifySignalData = propertyInfo[2];

    // initialize property cache with current value
    // NOTE: if this is an object, it is not directly unwrapped as it might
    // reference other QObject that we do not know yet
    __propertyCache__[propertyIndex] = propertyInfo[3];

    if (notifySignalData.size() > 0) {
        if (notifySignalData[0].is_number() && notifySignalData[0].get<int>() == 1) {
            // signal name is optimized away, reconstruct the actual name
            notifySignalData[0] = propertyName + "Changed";
        }
        addSignal(notifySignalData, true);
    }

    _properties[propertyName] = propertyIndex;
}

void QObject::addSignal(const json &signalData, bool isPropertyNotifySignal)
{
    std::string signalName = signalData[0];
    int signalIndex = signalData[1];

    _qsignals.emplace(signalName, Signal { signalIndex, signalName, isPropertyNotifySignal });
}

json QObject::unwrapQObject(const json &response) {
    if (response.is_array()) {
        // support list of objects
        json copy = response;
        for (json &j : copy) {
            j = unwrapQObject(j);
        }
        return copy;
    }

    if (!(response.is_object())
            || response.is_null()
            || !response.count("__QObject*__")
            || !response.count("id"))
    {
        return response;
    }

    const std::string objectId = response["id"];

    auto it = _webChannel->_objects.find(objectId);
    if (it != _webChannel->_objects.end()) {
        return it->second;
    }

    if (!response.count("data")) {
        std::cerr << "Cannot unwrap unknown QObject " << objectId << " without data.";
        return json();
    }

    QObject *qObject = new QObject( objectId, response["data"], _webChannel );

    connect("destroyed", [this, objectId]() {
        auto it = _webChannel->_objects.find(objectId);
        if (it != _webChannel->_objects.end()) {
            delete it->second;
            _webChannel->_objects.erase(it);
        }
    });

    // here we are already initialized, and thus must directly unwrap the properties
    qObject->unwrapProperties();

    return json {
        { "__ptr__", std::uintptr_t(qObject) }
    };
}

void QObject::unwrapProperties()
{
    for (auto it = __propertyCache__.begin(); it != __propertyCache__.end(); ++it) {
        it->second = unwrapQObject(it->second);
    }
}


namespace detail {

template<unsigned int N> struct priority_tag : priority_tag <N - 1> {};
template<> struct priority_tag<0> {};

template<class T>
auto handle_arg(std::vector<json> &, std::function<void(const json &)> &callback, T&& callable, priority_tag<1>) -> decltype(callable(json_unwrap(json())), void())
{
    callback = [callable](const json &response) {
        callable(json_unwrap(response));
    };
}

template<class T>
void handle_arg(std::vector<json> &args, std::function<void(const json &)> &, T &&t, priority_tag<0>)
{
    args.push_back(std::forward<T>(t));
}

}


template<class... Args>
bool QObject::invoke(const std::string &name, Args&& ...args)
{
    std::vector<json> jargs;
    jargs.reserve(sizeof...(Args));

    std::function<void(const json &)> callback;

    using expander = int[];
    (void) expander { (detail::handle_arg(jargs, callback, args, detail::priority_tag<2> {}), 0)... };

    return invoke(name, static_cast<const std::vector<json>&>(jargs), callback);
}

bool QObject::invoke(const std::string &name, const std::vector<json> &args, std::function<void (const json &)> callback)
{
    auto it = _methods.find(name);
    if (it == _methods.end()) {
        std::cerr << "Unknown method " << __id__ << "::" << name << std::endl;
        return false;
    }

    const int methodIdx = it->second;

    json msg {
        { "type", QWebChannelMessageTypes::InvokeMethod },
        { "method", methodIdx },
        { "args", args },
        { "object", __id__ },
    };

    _webChannel->exec(msg, [this, callback](const json &response) {
        if (!response.is_null()) {
            json result = unwrapQObject(response);
            if (callback) {
                callback(result);
            }
        }
    });

    return true;
}

void QObject::propertyUpdate(const json &sigs, const json &propertyMap)
{
    // update property cache
    for (auto it = propertyMap.begin(); it != propertyMap.end(); ++it) {
        const int key = std::stoi(it.key());
        __propertyCache__[key] = it.value();
    }

    for (auto it = sigs.begin(); it != sigs.end(); ++it) {
        const int key = std::stoi(it.key());
        // Invoke all callbacks, as signalEmitted() does not. This ensures the
        // property cache is updated before the callbacks are invoked.
        invokeSignalCallbacks(key, it.value());
    }
}

void QObject::invokeSignalCallbacks(int signalName, const std::vector<json> &args)
{
    auto values = __objectSignals__.equal_range(signalName);

    for (auto it = values.first; it != values.second; ++it) {
        it->second.callback(args);
    }
}

json_unwrap QObject::property(const std::string &name) const
{
    auto it = _properties.find(name);
    if (it == _properties.end()) {
        std::cerr << "Property " << __id__ << "::" << name << " not found.";
        return json_unwrap(json());
    }

    auto cacheIt = __propertyCache__.find(it->second);
    if (cacheIt == __propertyCache__.end()) {
        return json_unwrap(json());
    }

    return json_unwrap(cacheIt->second);
}

void QObject::set_property(const std::string &name, const json &value)
{
    auto it = _properties.find(name);
    if (it == _properties.end()) {
        std::cerr << "Property " << __id__ << "::" << name << " not found.";
        return;
    }

    __propertyCache__[it->second] = value;

    json msg {
        { "type", QWebChannelMessageTypes::SetProperty },
        { "property", it->second },
        { "value", value },
        { "object", __id__ },
    };

    _webChannel->exec(msg);
}

template<size_t N, class T>
unsigned int QObject::connect(const std::string &name, T &&callback)
{
    return connect_impl(name, std::forward<T>(callback), std::make_index_sequence<N>());
}

template<class T>
unsigned int QObject::connect(const std::string &name, T &&callback)
{
    return connect<detail::get_arity<std::decay_t<T>>::value>(name, callback);
}

template<class Callable, size_t... I>
unsigned int QObject::connect_impl(const std::string &signalName, Callable &&callback, std::index_sequence<I...>)
{
    auto it = _qsignals.find(signalName);
    if (it == _qsignals.end()) {
        std::cerr << "Signal " << __id__ << "::" << signalName << " not found";
        return 0;
    }

    const int signalIndex = it->second.signalIndex;
    const bool isPropertyNotifySignal = it->second.isPropertyNotifySignal;

    QObject::Connection conn {
        signalName,
        QObject::Connection::next_id(),
        [callback](const std::vector<json> &args) {
            callback(json_unwrap(args.at(I))...);
        }
    };

    __objectSignals__.insert(std::make_pair(signalIndex, conn));

    if (!isPropertyNotifySignal && signalName != "destroyed") {
        // only required for "pure" signals, handled separately for properties in _propertyUpdate
        // also note that we always get notified about the destroyed signal
        json msg {
            { "type", QWebChannelMessageTypes::ConnectToSignal },
            { "object", __id__ },
            { "signal", signalIndex },
        };

        _webChannel->exec(msg);
    }

    return conn.id;
}


bool QObject::disconnect(unsigned int id)
{
    auto it = std::find_if(__objectSignals__.begin(), __objectSignals__.end(), [id](const std::pair<int, Connection> &s) {
        return s.second.id == id;
    });

    if (it == __objectSignals__.end()) {
        std::cerr << "QObject::disconnect: No connection with id " << id << std::endl;
        return false;
    }

    Connection conn = it->second;
    __objectSignals__.erase(it);

    auto sigIt = _qsignals.find(conn.signalName);

    if (sigIt == _qsignals.end()) {
        std::cerr << "QObject::disconnect: Don't know signal name " << conn.signalName << ". This should not happen!" << std::endl;
        return false;
    }

    const Signal &sig = sigIt->second;

    if (!sig.isPropertyNotifySignal && __objectSignals__.count(sig.signalIndex) == 0) {
        // only required for "pure" signals, handled separately for properties in propertyUpdate
        _webChannel->exec(json {
            { "type", QWebChannelMessageTypes::DisconnectFromSignal },
            { "object", __id__ },
            { "signal", sig.signalIndex },
        });
    }

    return true;
}


void QObject::signalEmitted(int signalName, const json &signalArgs)
{
    json unwrapped = unwrapQObject(signalArgs);
    invokeSignalCallbacks(signalName, unwrapped);
}

}

#endif // QOBJECT_IMPL_H
