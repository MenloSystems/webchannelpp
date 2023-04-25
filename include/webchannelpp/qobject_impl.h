/*
 * This file is part of WebChannel++.
 * Copyright (C) 2016 - 2018, Menlo Systems GmbH
 * Copyright (C) 2016 The Qt Company Ltd.
 * Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company
 * License: Dual-licensed under LGPLv3 and GPLv2+
 */

#ifndef QOBJECT_IMPL_H
#define QOBJECT_IMPL_H

#include <list>
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

template<class T>
bool isDestroyedSignal(const T &signalName)
{
    return signalName == "destroyed" || signalName == "destroyed()" ||
            signalName == "destroyed(QObject*)";
}
}

template<class Json>
inline BasicQObject<Json>::BasicQObject(const string_t &name, const json_t &data, BasicQWebChannel<json_t> *channel)
    : __id__(name), _webChannel(channel)
{
    created_objects().insert(this);

    _webChannel->_objects[name] = this;

    if (data.count("methods")) {
        for (const json_t &method : data["methods"]) {
            addMethod(method);
        }
    }

    if (data.count("properties")) {
        for (const json_t &property : data["properties"]) {
            bindGetterSetter(property);
        }
    }

    if (data.count("signals")) {
        for (const json_t &signal : data["signals"]) {
            addSignal(signal, false);
        }
    }

    if (data.count("enums")) {
        _enums = data["enums"].template get<decltype(_enums)>();
    }
}

template<class Json>
inline BasicQObject<Json>::~BasicQObject()
{
    created_objects().erase(this);
}

template<class Json>
inline std::set<typename BasicQObject<Json>::string_t> BasicQObject<Json>::methods() const {
    std::set<string_t> methNames;
    for (auto &kv : _methods) methNames.insert(kv.first);
    return methNames;
}

template<class Json>
inline std::set<typename BasicQObject<Json>::string_t> BasicQObject<Json>::properties() const {
    std::set<string_t> propNames;
    for (auto &kv : _properties) propNames.insert(kv.first);
    return propNames;
}

template<class Json>
inline std::set<typename BasicQObject<Json>::string_t> BasicQObject<Json>::signalNames() const {
    std::set<string_t> sigNames;
    for (auto &kv : _qsignals) sigNames.insert(kv.first);
    return sigNames;
}

template<class Json>
inline bool BasicQObject<Json>::isNotifySignal(const string_t &signal) const {
    auto it = _qsignals.find(signal);
    if (it == _qsignals.end()) {
        return false;
    }
    return it->second.isPropertyNotifySignal;
}

template<class Json>
inline typename BasicQObject<Json>::string_t BasicQObject<Json>::notifySignalForProperty(const string_t &property) const {
    auto idIterator = _properties.find(property);
    if (idIterator == _properties.cend()) {
        return string_t();
    }

    auto signalIterator = _propertyNotifySignalMap.find(idIterator->second);
    if (signalIterator == _propertyNotifySignalMap.cend()) {
        return string_t();
    }
    return signalIterator->second;
}

template<class Json>
inline std::set<BasicQObject<Json> *> &BasicQObject<Json>::created_objects() {
    static std::set<BasicQObject*> set;
    return set;
}

template<class Json>
inline BasicQObject<Json> *BasicQObject<Json>::convert(uintptr_t ptr) {
    BasicQObject *obj = reinterpret_cast<BasicQObject*>(ptr);
    if (created_objects().find(obj) == created_objects().end()) {
        return nullptr;
    }
    return obj;
}

template<class Json>
inline void BasicQObject<Json>::addMethod(const json_t &method)
{
    _methods[method[0].template get<string_t>()] = method[1].template get<int>();
}

template<class Json>
inline void BasicQObject<Json>::bindGetterSetter(const json_t &propertyInfo)
{
    int propertyIndex = propertyInfo[0];
    string_t propertyName = propertyInfo[1];
    json_t notifySignalData = propertyInfo[2];

    // initialize property cache with current value
    // NOTE: if this is an object, it is not directly unwrapped as it might
    // reference other BasicQObject that we do not know yet
    __propertyCache__[propertyIndex] = propertyInfo[3];

    if (notifySignalData.size() > 0) {
        if (notifySignalData[0].is_number() && notifySignalData[0].template get<int>() == 1) {
            // signal name is optimized away, reconstruct the actual name
            notifySignalData[0] = propertyName + "Changed";
        }
        addSignal(notifySignalData, true);
        _propertyNotifySignalMap[propertyIndex] = notifySignalData[0];
    }

    _properties[propertyName] = propertyIndex;
}

template<class Json>
inline void BasicQObject<Json>::addSignal(const json_t &signalData, bool isPropertyNotifySignal)
{
    string_t signalName = signalData[0];
    int signalIndex = signalData[1];

    // If a signal already exists, only allow replacing it with a signal
    // of the same "kind". Otherwise, we might replace a property notify signal
    // with a pure signal, preventing the user from reacting to property updates.
    const bool exists = _qsignals.find(signalName) != _qsignals.end();
    Signal &signal = _qsignals[signalName];
    if (exists && signal.isPropertyNotifySignal != isPropertyNotifySignal) {
        return;
    }
    signal = Signal{signalIndex, signalName, isPropertyNotifySignal};
}

template<class Json>
inline Json BasicQObject<Json>::unwrapQObject(const json_t &response) {
    if (response.is_array()) {
        // support list of objects
        json_t copy = response;
        for (json_t &j : copy) {
            j = unwrapQObject(j);
        }
        return copy;
    }

    if (!response.is_object() || response.is_null()) {
        return response;
    } else if (!response.count("__QObject*__") || !response.count("id")) {
        json_t obj;
        for (auto it = response.begin(); it != response.end(); ++it) {
            obj[it.key()] = unwrapQObject(it.value());
        }
        return obj;
    }

    const string_t objectId = response["id"];

    auto it = _webChannel->_objects.find(objectId);
    if (it != _webChannel->_objects.end()) {
        return it->second;
    }

    if (!response.count("data")) {
        std::cerr << "Cannot unwrap unknown QObject " << objectId << " without data.";
        return json_t();
    }

    BasicQObject *qObject = new BasicQObject( objectId, response["data"], _webChannel );

    qObject->connect("destroyed", [qObject]() {
        auto it = qObject->_webChannel->_objects.find(qObject->id());
        if (it != qObject->_webChannel->_objects.end()) {
            qObject->_webChannel->_objects.erase(it);

            // Toggle this flag to ensure that all signal handlers have been run
            // before we destroy the instance. Actual destruction happens in
            // invokeSignalCallbacks()
            qObject->_destroyAfterSignal = true;
        }
    });

    // here we are already initialized, and thus must directly unwrap the properties
    qObject->unwrapProperties();

    return qObject;
}

template<class Json>
inline void BasicQObject<Json>::unwrapProperties()
{
    for (auto it = __propertyCache__.begin(); it != __propertyCache__.end(); ++it) {
        it->second = unwrapQObject(it->second);
    }
}


namespace detail {

template<unsigned int N> struct priority_tag : priority_tag <N - 1> {};
template<> struct priority_tag<0> {};

template<class Json, class T>
auto handle_arg(std::vector<Json> &, std::function<void(const Json &)> &callback, T&& callable, priority_tag<1>) -> decltype(callable(json_unwrap<Json>(Json())), void())
{
    callback = [callable](const Json &response) {
        callable(json_unwrap<Json>(response));
    };
}

template<class Json, class T>
void handle_arg(std::vector<Json> &args, std::function<void(const Json &)> &, T &&t, priority_tag<0>)
{
    args.push_back(std::forward<T>(t));
}

}


template<class Json>
template<class... Args>
inline bool BasicQObject<Json>::invoke(const string_t &name, Args&& ...args)
{
    std::vector<json_t> jargs;
    jargs.reserve(sizeof...(Args));

    std::function<void(const json_t &)> callback;

    using expander = int[];
    (void) expander { (detail::handle_arg(jargs, callback, args, detail::priority_tag<2> {}), 0)... };

    return invoke(name, static_cast<const std::vector<json_t>&>(jargs), callback);
}

template<class Json>
inline bool BasicQObject<Json>::invoke(const string_t &name, std::vector<json_t> args, std::function<void (const json_t &)> callback)
{
    auto it = _methods.find(name);
    if (it == _methods.end()) {
        std::cerr << "Unknown method " << __id__ << "::" << name << std::endl;
        return false;
    }

    const int methodIdx = it->second;

    for (json_t &j : args) {
        if (j.count("__ptr__")) {
            j = { { "id", j.template get<BasicQObject::Ptr>()->id() } };
        }
    }

    json_t msg {
        { "type", BasicQWebChannelMessageTypes::InvokeMethod },
        { "method", methodIdx },
        { "args", args },
        { "object", __id__ },
    };

    _webChannel->exec(msg, [this, callback](const json_t &response) {
        json_t result = unwrapQObject(response);
        if (callback) {
            callback(result);
        }
    });

    return true;
}

template<class Json>
inline void BasicQObject<Json>::propertyUpdate(const json_t &sigs, const json_t &propertyMap)
{
    using std::stoi;

    // update property cache
    for (auto it = propertyMap.begin(); it != propertyMap.end(); ++it) {
        const int key = stoi(it.key());
        __propertyCache__[key] = unwrapQObject(it.value());
    }

    for (auto it = sigs.begin(); it != sigs.end(); ++it) {
        const int key = stoi(it.key());
        // Invoke all callbacks, as signalEmitted() does not. This ensures the
        // property cache is updated before the callbacks are invoked.
        invokeSignalCallbacks(key, it.value());
    }
}

template<class Json>
inline void BasicQObject<Json>::invokeSignalCallbacks(int signalName, const std::vector<json_t> &args)
{
    const auto values = __objectSignals__.equal_range(signalName);

    // Copy the connections. The signal handler itself might connect/disconnect
    // things and thus invalidate the iterators.
    std::list<Connection> connections;
    for (auto it = values.first; it != values.second; ++it) {
        connections.push_back(it->second);
    }

    for (const auto &conn : connections) {
        conn.callback(args);
    }

    if (_destroyAfterSignal) {
        delete this;
    }
}

template<class Json>
inline json_unwrap<Json> BasicQObject<Json>::property(const string_t &name) const
{
    auto it = _properties.find(name);
    if (it == _properties.end()) {
        std::cerr << "Property " << __id__ << "::" << name << " not found." << std::endl;
        return json_unwrap<json_t>{};
    }

    auto cacheIt = __propertyCache__.find(it->second);
    if (cacheIt == __propertyCache__.end()) {
        return json_unwrap<json_t>{};
    }

    return json_unwrap<json_t>(cacheIt->second);
}

template<class Json>
inline void BasicQObject<Json>::set_property(const string_t &name, const json_t &value)
{
    auto it = _properties.find(name);
    if (it == _properties.end()) {
        std::cerr << "Property " << __id__ << "::" << name << " not found.";
        return;
    }

    if (_webChannel->propertyCachingEnabled) {
        __propertyCache__[it->second] = value;
    }

    json_t sendval = value;
    if (sendval.count("__ptr__")) {
        sendval = { { "id", sendval.template get<BasicQObject::Ptr>()->id() } };
    }

    json_t msg {
        { "type", BasicQWebChannelMessageTypes::SetProperty },
        { "property", it->second },
        { "value", sendval },
        { "object", __id__ },
    };

    _webChannel->exec(msg);
}

template<class Json>
template<size_t N, class T>
unsigned int BasicQObject<Json>::connect(const string_t &name, T &&callback)
{
    return connect_impl(name, std::forward<T>(callback), std::make_index_sequence<N>());
}

template<class Json>
template<class T>
unsigned int BasicQObject<Json>::connect(const string_t &name, T &&callback)
{
    return connect<detail::get_arity<std::decay_t<T>>::value>(name, callback);
}

template<class Json>
template<class Callable, size_t... I>
unsigned int BasicQObject<Json>::connect_impl(const string_t &signalName, Callable &&callback, std::index_sequence<I...>)
{
    return connect(signalName, static_cast<std::function<void (const std::vector<json_t> &)>>(
                   [callback](const std::vector<json_t> &args)
    {
        callback(json_unwrap<json_t>(args.at(I))...);
    }));
}

template<class Json>
inline unsigned int BasicQObject<Json>::connect(const string_t &signalName, std::function<void (const std::vector<json_t> &)> callback)
{
    auto it = _qsignals.find(signalName);
    if (it == _qsignals.end()) {
        std::cerr << "Signal " << __id__ << "::" << signalName << " not found";
        return 0;
    }

    const int signalIndex = it->second.signalIndex;
    const bool isPropertyNotifySignal = it->second.isPropertyNotifySignal;

    BasicQObject<Json>::Connection conn {
        signalName,
        BasicQObject<Json>::Connection::next_id(),
        callback
    };

    __objectSignals__.insert(std::make_pair(signalIndex, conn));

    if (!isPropertyNotifySignal && !detail::isDestroyedSignal<string_t>(signalName)) {
        // only required for "pure" signals, handled separately for properties in _propertyUpdate
        // also note that we always get notified about the destroyed signal
        json_t msg {
            { "type", BasicQWebChannelMessageTypes::ConnectToSignal },
            { "object", __id__ },
            { "signal", signalIndex },
        };

        _webChannel->exec(msg);
    }

    return conn.id;
}

template<class Json>
inline bool BasicQObject<Json>::disconnect(unsigned int id)
{
    auto it = std::find_if(__objectSignals__.begin(), __objectSignals__.end(), [id](const std::pair<int, Connection> &s) {
        return s.second.id == id;
    });

    if (it == __objectSignals__.end()) {
        std::cerr << "BasicQObject::disconnect: No connection with id " << id << std::endl;
        return false;
    }

    Connection conn = it->second;
    __objectSignals__.erase(it);

    auto sigIt = _qsignals.find(conn.signalName);

    if (sigIt == _qsignals.end()) {
        std::cerr << "BasicQObject::disconnect: Don't know signal name " << conn.signalName << ". This should not happen!" << std::endl;
        return false;
    }

    const Signal &sig = sigIt->second;

    if (!sig.isPropertyNotifySignal && __objectSignals__.count(sig.signalIndex) == 0) {
        // only required for "pure" signals, handled separately for properties in propertyUpdate
        _webChannel->exec(json_t {
            { "type", BasicQWebChannelMessageTypes::DisconnectFromSignal },
            { "object", __id__ },
            { "signal", sig.signalIndex },
        });
    }

    return true;
}

template<class Json>
inline void BasicQObject<Json>::signalEmitted(int signalName, const json_t &signalArgs)
{
    json_t unwrapped = unwrapQObject(signalArgs);
    invokeSignalCallbacks(signalName, unwrapped);
}

}

#endif // QOBJECT_IMPL_H
