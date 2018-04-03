#ifndef QOBJECT_IMPL_H
#define QOBJECT_IMPL_H

#include "qobject_fwd.h"

namespace QWebChannelPP
{

QObject::QObject(std::string name, const json &data, QWebChannel *channel)
    : __id__(name), webChannel(channel)
{
    created_objects().insert(this);

    webChannel->objects[name] = this;

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
        json eObject = data["enums"];
        for (auto enumPair = eObject.begin(); enumPair != eObject.end(); ++enumPair) {
            enums[enumPair.key()] = enumPair.value().get<int>();
        }
    }
}

QObject::~QObject()
{
    created_objects().erase(this);
    for (auto it = qsignals.begin(); it != qsignals.end(); ++it) {
        delete it->second;
    }
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
    methods[method[0].get<std::string>()] = method[1].get<int>();
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

    properties[propertyName] = propertyIndex;
}

void QObject::addSignal(const json &signalData, bool isPropertyNotifySignal)
{
    std::string signalName = signalData[0];
    int signalIndex = signalData[1];

    qsignals[signalName] = new Signal(this, signalIndex, signalName, isPropertyNotifySignal);
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

    auto it = webChannel->objects.find(objectId);
    if (it != webChannel->objects.end()) {
        return it->second;
    }

    if (!response.count("data")) {
        std::cerr << "Cannot unwrap unknown QObject " << objectId << " without data.";
        return json();
    }

    QObject *qObject = new QObject( objectId, response["data"], webChannel );

    signal("destroyed").connect<0>([this, objectId]() {
        auto it = webChannel->objects.find(objectId);
        if (it != webChannel->objects.end()) {
            delete it->second;
            webChannel->objects.erase(it);
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

bool QObject::invoke(const std::string &name, const std::vector<json> &args, std::function<void (const json &)> callback)
{
    auto it = methods.find(name);
    if (it == methods.end()) {
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

    webChannel->exec(msg, [this, callback](const json &response) {
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
        std::cerr << "Property update: " << it.key() << std::endl;
        const int key = std::stoi(it.key());
        __propertyCache__[key] = it.value();
    }

    for (auto it = sigs.begin(); it != sigs.end(); ++it) {
        const int key = std::stoi(it.key());
        std::cerr << "signal: " << it.value() << std::endl;
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

json_unwrap QObject::property(const std::__cxx11::string &name) const
{
    auto it = properties.find(name);
    if (it == properties.end()) {
        std::cerr << "Property " << __id__ << "::" << name << " not found.";
        return json_unwrap(json());
    }

    auto cacheIt = __propertyCache__.find(it->second);
    if (cacheIt == __propertyCache__.end()) {
        return json_unwrap(json());
    }

    return json_unwrap(cacheIt->second);
}

void QObject::set_property(const std::__cxx11::string &name, const json &value)
{
    auto it = properties.find(name);
    if (it == properties.end()) {
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

    webChannel->exec(msg);
}

const Signal &QObject::signal(const std::__cxx11::string &name)
{
    static Signal Invalid;

    auto it = qsignals.find(name);
    if (it == qsignals.end()) {
        std::cerr << "Signal " << __id__ << "::" << name << " not found";
        return Invalid;
    }

    return *it->second;
}

void QObject::signalEmitted(int signalName, const json &signalArgs)
{
    json unwrapped = unwrapQObject(signalArgs);
    invokeSignalCallbacks(signalName, unwrapped);
}

}

#endif // QOBJECT_IMPL_H
