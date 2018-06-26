#ifndef QOBJECT_FWD_H
#define QOBJECT_FWD_H

#include <map>
#include <set>
#include <string>

#include "json.hpp"
#include "qwebchannel_fwd.h"

namespace QWebChannelPP
{

using json = nlohmann::json;
using namespace std::placeholders;

class Signal;

class QObject
{
public:
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

    std::map<std::string, std::map<std::string, int>> enums;
    std::map<std::string, int> methods;
    std::map<std::string, int> properties;
    std::map<std::string, Signal> qsignals;

    std::map<int, json> __propertyCache__;
    std::multimap<int, Connection> __objectSignals__;

    QWebChannel *webChannel;

    QObject(std::string name, const json &data, QWebChannel *channel);

    ~QObject();

    inline static std::set<QObject*> &created_objects();
    inline static QObject *convert(std::uintptr_t ptr);

    void addMethod(const json &method);
    void bindGetterSetter(const json &propertyInfo);
    void addSignal(const json &signalData, bool isPropertyNotifySignal);

    json unwrapQObject(const json &response);
    void unwrapProperties();

    template<class... Args>
    bool invoke(const std::string &name, Args&& ...args);
    bool invoke(const std::string &name, const std::vector<json> &args, std::function<void(const json&)> callback = std::function<void(const json&)>());

    void propertyUpdate(const json &sigs, const json &propertyMap);

    /**
    * Invokes all callbacks for the given signalname. Also works for property notify callbacks.
    */
    void invokeSignalCallbacks(int signalName, const std::vector<json> &args);

    template<size_t N, class T>
    unsigned int connect(const std::string &name, T &&callback);
    template<class T>
    unsigned int connect(const std::string &name, T &&callback);
    template<class Callable, size_t... I>
    unsigned int connect_impl(const std::string &signal, Callable &&callable, std::index_sequence<I...>);
    bool disconnect(unsigned int id);

    json_unwrap property(const std::string &name) const;
    void set_property(const std::string &name, const json &value);
    const Signal &signal(const std::string &name);

    void signalEmitted(int signalName, const json &signalArgs);
};


void to_json(json &j, QObject *qobj)
{
    j = json {
        { "id", qobj->__id__ }
};
}

}

#endif // QOBJECT_FWD_H
