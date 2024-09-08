#pragma once

#include "net_common.h"
#include "net_tsqueue.h"
#include "net_message.h"
#include "net_connection.h"

namespace netp
{
    namespace net
    {
        template <typename T>
        class server_interface
        {
        public:
            server_interface(uint16_t port)
                : m_asioAcceptor(m_asioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
            {
            }

            virtual ~server_interface()
            {
                Stop();
            }

            bool Start()
            {
                try
                {
                    WaitForClientConnection();

                    m_threadContext = std::thread([this]()
                                                  { m_asioContext.run(); });
                }
                catch (const std::exception &e)
                {
                    std::cerr << "[SERVER] Exception: " << e.what() << '\n';
                    return false;
                }

                std::cout << "[SERVER] Started!\n";
                return true;
            }

            void Stop()
            {
                // request the context to close
                m_asioContext.stop();

                // tidy up context thread
                if (m_threadContext.joinable())
                    m_threadContext.join();

                std::cout << "[SERVER] Stopped!\n";
            }

            // ASYNC - instruct asio to wait for connection
            void WaitForClientConnection()
            {
                m_asioAcceptor.async_accept([this](std::error_code ec, asio::ip::tcp::socket socket)
                                            {

                    if (!ec)
                    {
                        std::cout << "[SERVER] New Connection: " << socket.remote_endpoint() << "\n";

                        std::shared_ptr<connection<T>> newconn =
                            std::make_shared<connection<T>>(connection<T>::owner::server,
                                m_asioContext, std::move(socket), m_qMessagesIn);

                        // give user chance to deny connection
                        if (OnClientConnect(newconn))
                        {
                            //connection allowed, add to container of new connections
                            m_deqConnections.push_back(std::move(newconn));

                            m_deqConnections.back()->ConnectToClient(this, nIDCounter++);

                            std::cout << "[" << m_deqConnections.back()->GetID() << "] Connection Approved\n";
                        }
                        else
                        {
                            std::cout << "[----] Connection Denied\n";
                        }
                    }
                    else{
                        //error has occured during acceptance
                        std::cout << "[SERVER] New Connection Error: " << ec.message() << "\n";
                    }

                    //prime asio context with more work - simply wait again for another connection
                    WaitForClientConnection(); });
            }

            // send message to specific client
            void MessageClient(std::shared_ptr<connection<T>> client, const message<T> &msg)
            {
                if (client && client->IsConnected())
                {
                    client->Send(msg);
                }
                else
                {
                    OnClientDisconnect(client);
                    client.reset();
                    m_deqConnections.erase(
                        std::remove(m_deqConnections.begin(), m_deqConnections.end(), client), m_deqConnections.end());
                }
            }

            // send message to all clients
            void MessageAllClients(const message<T> &msg, std::shared_ptr<connection<T>> pIgnoreClient = nullptr)
            {
                bool bInvalidClientExists = false;
                for (auto &client : m_deqConnections)
                {
                    // check client is connected...
                    if (client && client->IsConnected())
                    {
                        if (client != pIgnoreClient)
                            client->Send(msg);
                    }
                    else
                    {
                        // The client couldnt be contacted, so assume it has disconnected
                        OnClientDisconnect(client);
                        client.reset();
                        bInvalidClientExists = true;
                    }
                }

                if (bInvalidClientExists)
                    m_deqConnections.erase(std::remove(m_deqConnections.begin(), m_deqConnections.end(), nullptr), m_deqConnections.end());
            }

            void Update(size_t nMaxMessages = -1, bool bWait = false)
            {
                // we dont need server to occupy 100% of cpu core, server only runs when messages are there
                if (bWait)
                    m_qMessagesIn.wait();

                size_t nMessageCount = 0;
                while (nMessageCount < nMaxMessages && !m_qMessagesIn.empty())
                {
                    // grab front message
                    auto msg = m_qMessagesIn.pop_front();

                    // pass to message handler
                    OnMessage(msg.remote, msg.msg);

                    nMessageCount++;
                }
            }

        protected:
            // when client connects, can veto connection by returning false
            virtual bool OnClientConnect(std::shared_ptr<connection<T>> client)
            {
                return false;
            }

            // called when a client has disconnected
            virtual void OnClientDisconnect(std::shared_ptr<connection<T>> client)
            {
            }

            // called when message arrives
            virtual void OnMessage(std::shared_ptr<connection<T>> client, message<T> &msg)
            {
            }

        public:
            // called when client is validated
            virtual void OnClientValidated(std::shared_ptr<connection<T>> client)
            {
            }

        protected:
            // Thread safe queue for incoming messages
            tsqueue<owned_message<T>> m_qMessagesIn;

            // container of active validated connections
            std::deque<std::shared_ptr<connection<T>>> m_deqConnections;

            // order of declaration is important -> order of initialization
            asio::io_context m_asioContext;
            std::thread m_threadContext;

            // need an asio context
            asio::ip::tcp::acceptor m_asioAcceptor;

            // clients need an identifier in the "wider system"
            uint32_t nIDCounter = 10000;
        };
    }
}