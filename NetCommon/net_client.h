#pragma once

#include "net_common.h"
#include "net_message.h"
#include "net_tsqueue.h"
#include "net_connection.h"

namespace netp
{
    namespace net
    {
        template <typename T>
        class client_interface // sets up asio & connection, also acts as access point for application to talk to server
        {
        public:
            client_interface() //: m_socket(m_context)
            {
                // initialize socket with io context, so it can do stuff
            }

            virtual ~client_interface()
            {
                // if client is destroyed, always try to disconnect
                Disconnect();
            }

        public:
            // connect to server with hostname/ip-address and port
            bool Connect(const std::string &host, const uint16_t port)
            {
                try
                {
                    // Resolve hostname/ip_address into tangible physical address
                    asio::ip::tcp::resolver resolver(m_context);
                    asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port));

                    // create connection
                    m_connection = std::make_unique<connection<T>>(
                        connection<T>::owner::client,
                        m_context,
                        asio::ip::tcp::socket(m_context), m_qMessagesIn);

                    // tell connection object to connect to server
                    m_connection->ConnectToServer(endpoints);

                    // start context thread
                    thrContext = std::thread([this]()
                                             { m_context.run(); });
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Client Exception: " << e.what() << '\n';
                    return false;
                }

                return false;
            }

            void Disconnect()
            {
                // if connection exists
                if (IsConnected())
                {
                    // disconnect from server
                    m_connection->Disconnect();
                }

                // stop asio context
                m_context.stop();
                // stop thread
                if (thrContext.joinable())
                    thrContext.join();

                // destroy connection object
                m_connection.release();
            }

            // check if client is connected to server
            bool IsConnected()
            {
                if (m_connection)
                    return m_connection->IsConnected();
                return false;
            }

            // retrieve queue of messages from server
            tsqueue<owned_message<T>> &Incoming()
            {
                return m_qMessagesIn;
            }

        protected:
            // asio context handles data transfer
            asio::io_context m_context;
            // needs thread of its own to execute its work commands
            std::thread thrContext;
            // the client has a single instance of a "connection" object, which handels data transfer
            std::unique_ptr<connection<T>> m_connection;

        private:
            // thread safe queue of incoming messages from server
            tsqueue<owned_message<T>> m_qMessagesIn;
        };
    }
}