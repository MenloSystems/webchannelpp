#ifndef SIGNAL_FWD_H
#define SIGNAL_FWD_H

#include "qobject_fwd.h"

namespace QWebChannelPP
{

class Signal
{
public:
    QObject *qObject;
    int signalIndex;
    std::string signalName;
    bool isPropertyNotifySignal;

    explicit Signal(QObject *qObject = nullptr, int signalIndex = 0, const std::string &signalName = std::string(), bool isPropertyNotifySignal = false);

    bool valid() const;
    operator bool() const;

    template<size_t N, class T>
    unsigned int connect(T &&callback) const
    {
        return connect_impl(std::forward<T>(callback), std::make_index_sequence<N>());
    }

    template<class Callable, size_t... I>
    unsigned int connect_impl(Callable callback, std::index_sequence<I...>) const;

//    void disconnect(Delegate callback) {
//        if (!qObject.__objectSignals__.ContainsKey(signalIndex)) {
//            qObject.__objectSignals__[signalIndex] = new List<Delegate>();
//        }

//        if (!qObject.__objectSignals__[signalIndex].Contains(callback)) {
//            Console.Error.WriteLine("Cannot find connection of signal " + signalName + " to " + callback);
//            return;
//        }

//        qObject.__objectSignals__[signalIndex].Remove(callback);

//        if (!isPropertyNotifySignal && qObject.__objectSignals__[signalIndex].Count == 0) {
//            // only required for "pure" signals, handled separately for properties in _propertyUpdate
//            var msg = new JObject();
//            msg["type"] = (int) QWebChannelMessageTypes.DisconnectFromSignal;
//            msg["object"] = qObject.__id__;
//            msg["signal"] = signalIndex;

//            qObject.webChannel.exec(msg);
//        }
//    }
};

}

#endif // SIGNAL_FWD_H
