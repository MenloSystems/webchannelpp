# WebChannel++

## What is WebChannel++?

WebChannel++ is an implementation of [Qt's WebChannel](https://doc.qt.io/qt-5/qtwebchannel-index.html) protocol in C++14.
WebChannel++ is header-only and depends only on standard library features and [Niels Lohmann's excellent JSON Library for C++](https://github.com/nlohmann/json).

## Usage

To use it, you will have to define your own `Transport` subclass to handle the network related tasks (sending and receiving messages). An examplary implementation
based on the standalone [`asio` library](https://think-async.com) is included in `asio_transport.h`. This implementation communicates via TCP/IP and expectes messages to be newline-delimited.

## Caveats
### QObject marshalling

Due to some restrictions in nlohmann::json, plain pointers to objects can't be marshalled to or from json. To circumvent this,
WebChannel++ defines the transparent pointer class QObject::Ptr. It acts and behaves just like a QObject* (and can implicitly be converted to and from
QObject*), but is a separate type which can be marshalled to and from nlohmann::json.

In practice, you only have to care about this when you retrieve a property or a get the return value from a method:

```c++
// WebChannelPP::QObject *value = obj->property("value");  // WRONG!
WebChannelPP::QObject::Ptr value = obj->property("value");  // correct
```
