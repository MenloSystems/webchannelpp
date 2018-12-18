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
#include <string>

#ifndef WEBCHANNELPP_USE_GLOBAL_JSON
#include "nlohmann/json.hpp"
#else
#include <json.hpp>
#endif
#include "qwebchannel_fwd.h"

namespace WebChannelPP
{

using json = nlohmann::json;

class QObject
{
    struct Signal {
        int signalIndex;
        std::string signalName;
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

        std::string signalName;
        unsigned int id;
        std::function<void(const std::vector<json> &args)> callback;
    };

    std::string __id__;

    std::map<std::string, std::map<std::string, int>> _enums;
    std::map<std::string, int> _methods;
    std::map<std::string, int> _properties;
    std::map<std::string, Signal> _qsignals;

    std::map<int, json> __propertyCache__;
    std::multimap<int, Connection> __objectSignals__;

    QWebChannel *_webChannel;

public:
    /// @brief Transparent pointer class for use in QObject* (de-)serialization
    struct Ptr
    {
        QObject *ptr;

        Ptr() : ptr(nullptr) {}
        Ptr(QObject *ptr) : ptr(ptr) {}
        Ptr(const Ptr &) = default;
        Ptr(Ptr &&) = default;
        Ptr &operator=(const Ptr& other) = default;

        QObject *get() const { return *this; }
        QObject *operator->() const { return ptr; }
        QObject &operator*() const { return *ptr; }
        operator QObject*() const { return ptr; }
        operator bool() const { return bool(ptr); }
    };

    ~QObject();

    QWebChannel *webChannel() const { return _webChannel; }

    /// @brief Returns a mapping of defined enums
    const decltype(_enums) & enums() const { return _enums; }

    /// @brief Returns the set of method names of this object
    std::set<std::string> methods() const;

    /// @brief Returns the set of property names of this object
    std::set<std::string> properties() const;

    /// @brief Returns the set of signal names of this object
    std::set<std::string> signalNames() const;

    /// @brief Invokes a method `name` with specified arguments `args`. If one argument is callable,
    ///        it is used as the callback when the method call has finished.
    template<class... Args>
    bool invoke(const std::string &name, Args&& ...args);
    /// @brief Invokes a method `name` with specified arguments `args`. `callback` is invoked when the method call has finished.
    bool invoke(const std::string &name, std::vector<json> args, std::function<void(const json&)> callback = std::function<void(const json&)>());

    /// @brief Connects `callback` to the signal `name`. `N` is the number of arguments.
    /// @return The connection id.
    template<size_t N, class T>
    unsigned int connect(const std::string &name, T &&callback);
    /// @brief Connects `callback` to the signal `name`.
    /// @return The connection id.
    template<class T>
    unsigned int connect(const std::string &name, T &&callback);

    /// @brief Breaks the connection with identifier `id`.
    bool disconnect(unsigned int id);

    /// @brief Gets the value of property `name`
    json_unwrap property(const std::string &name) const;
    /// @brief Sets the value of property `name` to `value`
    void set_property(const std::string &name, const json &value);

    std::string id() const { return __id__; }

private:
    QObject(const std::string &name, const json &data, QWebChannel *channel);

    inline static std::set<QObject*> &created_objects();

    inline static QObject *convert(std::uintptr_t ptr);

    template<class Callable, size_t... I>
    unsigned int connect_impl(const std::string &signal, Callable &&callable, std::index_sequence<I...>);

    void addMethod(const json &method);
    void bindGetterSetter(const json &propertyInfo);
    void addSignal(const json &signalData, bool isPropertyNotifySignal);

    json unwrapQObject(const json &response);
    void unwrapProperties();
    void propertyUpdate(const json &sigs, const json &propertyMap);

    /**
    * Invokes all callbacks for the given signalname. Also works for property notify callbacks.
    */
    void invokeSignalCallbacks(int signalName, const std::vector<json> &args);
    void signalEmitted(int signalName, const json &signalArgs);

    friend void from_json(const json &j, QObject *&o);
    friend class QWebChannel;
};


void to_json(json &j, QObject *qobj)
{
    j = json {
        { "__ptr__", std::uintptr_t(qobj) },
    };
}

void to_json(json &j, QObject::Ptr qobj)
{
    to_json(j, qobj.ptr);
}

void from_json(const json &j, QObject *&o)
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

    o = QObject::convert(j["__ptr__"].get<std::uintptr_t>());
}

void from_json(const json &j, QObject::Ptr &o)
{
    from_json(j, o.ptr);
}


}

#endif // QOBJECT_FWD_H
