/*
 * This file is part of WebChannel++.
 * Copyright (C) 2016 - 2018, Menlo Systems GmbH
 * Copyright (C) 2016 The Qt Company Ltd.
 * Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company
 * License: Dual-licensed under LGPLv3 and GPLv2+
 */

#ifndef QOBJECT_FWD_H
#define QOBJECT_FWD_H

#include <map>
#include <set>
#include <iostream>

#ifndef WEBCHANNELPP_USE_GLOBAL_JSON
#include "nlohmann/json.hpp"
#else
#include <json.hpp>
#endif
#include "qwebchannel_fwd.h"

namespace WebChannelPP
{

template<class Json>
void from_json(const Json &j, BasicQObject<Json> *&o);

template<class Json = nlohmann::json>
class BasicQObject
{
    using json_t = Json;
    using string_t = typename json_t::string_t;

    struct Signal {
        int signalIndex;
        string_t signalName;
        bool isPropertyNotifySignal;
    };

    struct Connection {
        static unsigned int next_id()
        {
            static unsigned int gid = 0;
            gid++;

            if (gid == 0) {
                gid = 1;
            }

            return gid;
        }

        string_t signalName;
        unsigned int id;
        std::function<void(const std::vector<json_t> &args)> callback;
    };

    string_t __id__;

    std::map<string_t, std::map<string_t, int>> _enums;
    std::map<string_t, int> _methods;
    std::map<string_t, int> _properties;
    std::map<string_t, Signal> _qsignals;
    std::map<int, string_t> _propertyNotifySignalMap;

    std::map<int, json_t> __propertyCache__;
    std::multimap<int, Connection> __objectSignals__;

    BasicQWebChannel<json_t> *_webChannel;

public:
    /// @brief Transparent pointer class for use in BasicQObject* (de-)serialization
    struct Ptr
    {
        BasicQObject *ptr;

        Ptr() : ptr(nullptr) {}
        Ptr(BasicQObject *ptr) : ptr(ptr) {}
        Ptr(const Ptr &) = default;
        Ptr(Ptr &&) = default;
        Ptr &operator=(const Ptr& other) = default;

        BasicQObject *get() const { return *this; }
        BasicQObject *operator->() const { return ptr; }
        BasicQObject &operator*() const { return *ptr; }
        operator BasicQObject*() const { return ptr; }
        operator bool() const { return bool(ptr); }
    };

    ~BasicQObject();

    BasicQWebChannel<json_t> *webChannel() const { return _webChannel; }

    /// @brief Returns a mapping of defined enums
    const decltype(_enums) & enums() const { return _enums; }

    /// @brief Returns the set of method names of this object
    std::set<string_t> methods() const;

    /// @brief Returns the set of property names of this object
    std::set<string_t> properties() const;

    /// @brief Returns the set of signal names of this object
    std::set<string_t> signalNames() const;

    /// @brief Returns whether a signal is a property notification signal
    bool isNotifySignal(const string_t &signalName) const;

    /// @brief Returns the notify signal name for a given object
    string_t notifySignalForProperty(const string_t &property) const;

    /// @brief Invokes a method `name` with specified arguments `args`. If one argument is callable,
    ///        it is used as the callback when the method call has finished.
    template<class... Args>
    bool invoke(const string_t &name, Args&& ...args);
    /// @brief Invokes a method `name` with specified arguments `args`. `callback` is invoked when the method call has finished.
    bool invoke(const string_t &name, std::vector<json_t> args, std::function<void(const json_t&)> callback = std::function<void(const json_t&)>());

    /// @brief Connects `callback` to the signal `name`. `N` is the number of arguments.
    /// @return The connection id.
    template<size_t N, class T>
    unsigned int connect(const string_t &name, T &&callback);
    /// @brief Connects `callback` to the signal `name`.
    /// @return The connection id.
    template<class T>
    unsigned int connect(const string_t &name, T &&callback);
    /// @brief Connects `callback` to the signal `name`.
    /// @return The connection id.
    unsigned int connect(const string_t &signalName, std::function<void(const std::vector<json_t> &)> callback);

    /// @brief Breaks the connection with identifier `id`.
    bool disconnect(unsigned int id);

    /// @brief Gets the value of property `name`
    json_unwrap<json_t> property(const string_t &name) const;
    /// @brief Sets the value of property `name` to `value`
    void set_property(const string_t &name, const json_t &value);

    string_t id() const { return __id__; }

private:
    BasicQObject(const string_t &name, const json_t &data, BasicQWebChannel<json_t> *channel);

    static std::set<BasicQObject*> &created_objects();

    static BasicQObject *convert(std::uintptr_t ptr);

    template<class Callable, size_t... I>
    unsigned int connect_impl(const string_t &signal, Callable &&callable, std::index_sequence<I...>);

    void addMethod(const json_t &method);
    void bindGetterSetter(const json_t &propertyInfo);
    void addSignal(const json_t &signalData, bool isPropertyNotifySignal);

    json_t unwrapQObject(const json_t &response);
    void unwrapProperties();
    void propertyUpdate(const json_t &sigs, const json_t &propertyMap);

    /**
    * Invokes all callbacks for the given signalname. Also works for property notify callbacks.
    */
    void invokeSignalCallbacks(int signalName, const std::vector<json_t> &args);
    void signalEmitted(int signalName, const json_t &signalArgs);

    friend void from_json<>(const json_t &j, BasicQObject<json_t> *&o);
    friend class BasicQWebChannel<json_t>;
};

using QObject = BasicQObject<>;

template<class Json>
void to_json(Json &j, BasicQObject<Json> *qobj)
{
    j = Json {
        { "__ptr__", std::uintptr_t(qobj) },
    };
}

template<class Json>
void to_json(Json &j, typename BasicQObject<Json>::Ptr qobj)
{
    to_json(j, qobj.ptr);
}

template<class Json>
void from_json(const Json &j, BasicQObject<Json> *&o)
{
    if (j.is_null()) {
        o = nullptr;
        return;
    }

    if (!j.is_object() || !j.count("__ptr__")) {
        std::cerr << "JSON object " << j << " does not point to a native object!" << std::endl;
        o = nullptr;
        return;
    }

    auto ptr = j["__ptr__"].template get<std::uintptr_t>();
    o = BasicQObject<Json>::convert(ptr);
}

template<class Json>
void from_json(const Json &j, typename BasicQObject<Json>::Ptr &o)
{
    from_json(j, o.ptr);
}

}

#endif // QOBJECT_FWD_H
