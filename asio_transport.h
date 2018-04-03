#ifndef ASIO_TRANSPORT_H
#define ASIO_TRANSPORT_H

#include <functional>
#include <vector>
#include <asio.hpp>

#include "qwebchannel_fwd.h"

namespace QWebChannelPP
{

using namespace std::placeholders;

class AsioTransport : public Transport
{
    asio::ip::tcp::socket &m_socket;
    std::vector<char> m_buffer;
    message_handler m_handler;
public:
    explicit AsioTransport(asio::ip::tcp::socket &socket)
        : m_socket(socket)
    {
        std::cout << "Created Asio Adapter" << std::endl;
        async_read_more();
    }

    void async_read_more()
    {
        m_socket.async_read_some(asio::null_buffers(), std::bind(&AsioTransport::read_some, this, _1, _2));
    }

    void read_some(const asio::error_code &/*err*/, std::size_t /* nybtes */)
    {
        std::vector<char> temp(m_socket.available());
        m_socket.read_some(asio::buffer(temp));
        m_buffer.insert(m_buffer.end(), temp.begin(), temp.end());

        process_messages();

        async_read_more();
    }

    void send(const std::string &s) override
    {
        m_socket.write_some(asio::buffer(s + "\n"));
    }

    void register_message_handler(message_handler handler) override
    {
        m_handler = std::move(handler);
    }

    void process_messages()
    {
        for (auto it = std::find(m_buffer.begin(), m_buffer.end(), '\n');
             it != m_buffer.end();
             it = std::find(m_buffer.begin(), m_buffer.end(), '\n'))
        {
            std::string msg(m_buffer.data(), std::distance(m_buffer.begin(), it));
            it++;
            m_buffer.erase(m_buffer.begin(), it);

            if (m_handler) {
                m_handler(msg);
            }
        }
    }
};

}

#endif // ASIO_TRANSPORT_H
