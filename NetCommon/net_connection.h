#pragma once

#include "net_common.h"
#include "net_tsqueue.h"
#include "net_message.h"

namespace netp
{
    namespace net
    {
        // forward declare
        template <typename T>
        class server_interface;

        template <typename T>
        class connection : public std::enable_shared_from_this<connection<T>>
        {
        public:
            enum class owner
            {
                server,
                client
            };

            connection(owner parent, asio::io_context &asioContext, asio::ip::tcp::socket socket, tsqueue<owned_message<T>> &qIn)
                : m_asioContext(asioContext), m_socket(std::move(socket)), m_qMessagesIn(qIn)
            {
                m_nOwnerType = parent;

                // construct validatoin check data
                if (m_nOwnerType == owner::server)
                {
                    // connection server -> client, construct "random" data for client to transform and send back to validate
                    m_nHandshakeOut = uint64_t(std::chrono::system_clock::now().time_since_epoch().count());

                    // pre-calculate result for checking
                    m_nHandshakeCheck = scramble(m_nHandshakeOut);
                }
                else
                {
                    // connection client -> server, nothing to define
                    m_nHandshakeIn = 0;
                    m_nHandshakeOut = 0;
                }
            }

            virtual ~connection()
            {
            }

            uint32_t GetID() const
            {
                return id;
            }

        public:
            void ConnectToClient(netp::net::server_interface<T> *server, uint32_t uid = 0)
            {
                if (m_nOwnerType == owner::server)
                {
                    if (m_socket.is_open())
                    {
                        id = uid;
                        // ReadHeader();
                        // client attempt to connect to server, validate client, write out handshake data
                        WriteValidation();

                        // read the validation data sent back
                        ReadValidation(server);
                    }
                }
            }
            void ConnectToServer(const asio::ip::tcp::resolver::results_type &endpoints)
            {
                // only cilents can connect to servers
                if (m_nOwnerType == owner::client)
                {
                    // request asio to connect to endpoint
                    asio::async_connect(m_socket, endpoints,
                                        [this](std::error_code ec, asio::ip::tcp::endpoint endpoint)
                                        {
                                            if (!ec)
                                            {
                                                // ReadHeader();
                                                // First validate a packet, wait and respond
                                                ReadValidation();
                                            }
                                        });
                }
            }
            void Disconnect()
            {
                if (IsConnected())
                    asio::post(m_asioContext, [this]()
                               { m_socket.close(); });
            }
            bool IsConnected() const
            {
                return m_socket.is_open();
            }

        public:
            void Send(const message<T> &msg)
            {
                asio::post(m_asioContext,
                           [this, msg]()
                           {
                               bool bWritingMessage = !m_qMessagesOut.empty();
                               m_qMessagesOut.push_back(msg);
                               if (!bWritingMessage)
                               {
                                   WriteHeader();
                               }
                           });
            }

        private:
            // ASYNC - prime context ready to read message header
            void ReadHeader()
            {
                asio::async_read(m_socket, asio::buffer(&m_msgTemporaryIn.header, sizeof(message_header<T>)),
                                 [this](std::error_code ec, std::size_t length)
                                 {
                                     if (!ec)
                                     {

                                         if (m_msgTemporaryIn.header.size > 0)
                                         {
                                             m_msgTemporaryIn.body.resize(m_msgTemporaryIn.header.size);
                                             ReadBody();
                                         }
                                         else
                                         {
                                             AddToIncomingMessageQueue();
                                         }
                                     }
                                     else
                                     {
                                         std::cout << "[" << id << "] Read Header Fail.\n";
                                         m_socket.close();
                                     }
                                 });
            }

            void ReadBody()
            {
                asio::async_read(m_socket, asio::buffer(m_msgTemporaryIn.body.data(), m_msgTemporaryIn.body.size()),
                                 [this](std::error_code ec, std::size_t length)
                                 {
                                     if (!ec)
                                     {
                                         AddToIncomingMessageQueue();
                                     }
                                     else
                                     {
                                         std::cout << "[" << id << "] Read Body Fail.\n";
                                         m_socket.close();
                                     }
                                 });
            }

            // ASYNC - prime context to write message header
            void WriteHeader()
            {
                asio::async_write(m_socket, asio::buffer(&m_qMessagesOut.front().header, sizeof(message_header<T>)),
                                  [this](std::error_code ec, std::size_t length)
                                  {
                                      if (!ec)
                                      {
                                          if (m_qMessagesOut.front().body.size() > 0)
                                          {
                                              WriteBody();
                                          }
                                          else
                                          {
                                              m_qMessagesOut.pop_front();
                                              if (!m_qMessagesOut.empty())
                                              {
                                                  WriteHeader();
                                              }
                                          }
                                      }
                                      else
                                      {
                                          std::cout << "[" << id << "] Write Header Fail.\n";
                                          m_socket.close();
                                      }
                                  });
            }

            void WriteBody()
            {
                asio::async_write(m_socket, asio::buffer(m_qMessagesOut.front().body.data(), m_qMessagesOut.front().body.size()),
                                  [this](std::error_code ec, std::size_t length)
                                  {
                                      if (!ec)
                                      {
                                          m_qMessagesOut.pop_front();

                                          if (!m_qMessagesOut.empty())
                                          {
                                              WriteHeader();
                                          }
                                      }
                                      else
                                      {
                                          std::cout << "[" << id << "] Write Body Fail.\n";
                                          m_socket.close();
                                      }
                                  }

                );
            }

            void AddToIncomingMessageQueue()
            {
                if (m_nOwnerType == owner::server)
                    m_qMessagesIn.push_back({this->shared_from_this(), m_msgTemporaryIn});
                else
                    m_qMessagesIn.push_back({nullptr, m_msgTemporaryIn});

                ReadHeader();
            }

            // encrypt data
            uint64_t scramble(uint64_t nInput)
            {
                uint64_t out = nInput ^ 0xDEADBEEFC0DECAFE;
                out = (out & 0xF0F0F0F0F0F0F0) >> 4 | (out & 0x0F0F0F0F0F0F0F) << 4;
                return out ^ 0xC0DEFACE12345678;
            }

            // ASYNC
            void WriteValidation()
            {
                asio::async_write(m_socket, asio::buffer(&m_nHandshakeOut, sizeof(uint64_t)),
                                  [this](std::error_code ec, std::size_t length)
                                  {
                                      if (!ec)
                                      {
                                          // validation data sent, client wait for response
                                          if (m_nOwnerType == owner::client)
                                              ReadHeader();
                                      }
                                      else
                                      {
                                          m_socket.close();
                                      }
                                  }

                );
            }

            void ReadValidation(netp::net::server_interface<T> *server = nullptr)
            {
                asio::async_read(m_socket, asio::buffer(&m_nHandshakeIn, sizeof(uint64_t)),
                                 [this, server](std::error_code ec, std::size_t length)
                                 {
                                     if (!ec)
                                     {
                                         if (m_nOwnerType == owner::server)
                                         {
                                             if (m_nHandshakeIn == m_nHandshakeCheck)
                                             {
                                                 // client provided valid solution, allow connection
                                                 std::cout << "Client Validated" << std::endl;
                                                 server->OnClientValidated(this->shared_from_this());

                                                 // sit and receive data
                                                 ReadHeader();
                                             }
                                             else
                                             {
                                                 // Client gave incorrect data, disconnect
                                                 std::cout << "Client Disconnected (Fail Validation)" << std::endl;
                                                 m_socket.close();
                                             }
                                         }
                                         else
                                         {
                                             // connection is a client, solve puzzle
                                             m_nHandshakeOut = scramble(m_nHandshakeIn);

                                             // write result
                                             WriteValidation();
                                         }
                                     }
                                     else
                                     {
                                         // some big failure occured
                                         std::cout << "Client Disconnected (ReadValidation)" << std::endl;
                                         m_socket.close();
                                     }
                                 });
            }

        protected:
            // each connection will have a unique socket to remote
            asio::ip::tcp::socket m_socket;

            // this contet is shared with entire asio instance
            asio::io_context &m_asioContext;

            // queue holding all messages to be sent to remote site
            tsqueue<message<T>> m_qMessagesOut;

            // queue holding all messages sent in from remote site,
            // note it is a reference as "owner" of this connection is expected to provide a queue
            tsqueue<owned_message<T>> &m_qMessagesIn;
            message<T> m_msgTemporaryIn;

            // "owner" decides how connection behaves
            owner m_nOwnerType = owner::server;
            uint32_t id = 0;

            // handshake validation
            uint64_t m_nHandshakeOut = 0;
            uint64_t m_nHandshakeIn = 0;
            uint64_t m_nHandshakeCheck = 0;
        };

    }

}