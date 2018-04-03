#ifndef SIGNAL_IMPL_H
#define SIGNAL_IMPL_H

#include "signal_fwd.h"

namespace QWebChannelPP
{

Signal::Signal(QObject *qObject, int signalIndex, const std::string &signalName, bool isPropertyNotifySignal)
    : qObject(qObject), signalIndex(signalIndex), signalName(signalName), isPropertyNotifySignal(isPropertyNotifySignal)
{
}

bool Signal::valid() const
{
    return *this;
}

QWebChannelPP::Signal::operator bool() const {
    return qObject;
}

template<class Callable, size_t... I>
unsigned int Signal::connect_impl(Callable callback, std::index_sequence<I...>) const
{
    QObject::Connection conn {
        QObject::Connection::next_id(),
        [callback](const std::vector<json> &args) {
            callback(json_unwrap(args.at(I))...);
        }
    };

    qObject->__objectSignals__.insert(std::make_pair(signalIndex, conn));

    if (!isPropertyNotifySignal && signalName != "destroyed") {
        // only required for "pure" signals, handled separately for properties in _propertyUpdate
        // also note that we always get notified about the destroyed signal
        json msg {
            { "type", QWebChannelMessageTypes::ConnectToSignal },
            { "object", qObject->__id__ },
            { "signal", signalIndex },
        };

        qObject->webChannel->exec(msg);
    }

    return conn.id;
}

}

#endif // SIGNAL_IMPL_H
