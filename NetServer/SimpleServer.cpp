
#include <iostream>
#include "../NetCommon/netp_net.h"

enum class CustomMsgTypes : uint32_t
{
    ServerAccept,
    ServerDeny,
    ServerPing,
    MessageAll,
    ServerMessage,
};

class CustomServer : public netp::net::server_interface<CustomMsgTypes>
{
public:
    CustomServer(uint16_t nPort) : netp::net::server_interface<CustomMsgTypes>(nPort)
    {
    }

protected:
    virtual bool OnClientConnect(std::shared_ptr<netp::net::connection<CustomMsgTypes>> client)
    {
        netp::net::message<CustomMsgTypes> msg;
        msg.header.id = CustomMsgTypes::ServerAccept;
        client->Send(msg);
        return true;
    }

    // when client disconnects
    virtual void OnClientDisconnect(std::shared_ptr<netp::net::connection<CustomMsgTypes>> client)
    {
        std::cout << "Removing client [" << client->GetID() << "]\n";
    }

    // when message arrives
    virtual void OnMessage(std::shared_ptr<netp::net::connection<CustomMsgTypes>> client, netp::net::message<CustomMsgTypes> &msg)
    {
        switch (msg.header.id)
        {
        case CustomMsgTypes::ServerPing:
        {
            std::cout << "[" << client->GetID() << "]: Server Ping\n";

            // send to client
            client->Send(msg);
        }
        break;

        case CustomMsgTypes::MessageAll:
        {
            std::cout << "[" << client->GetID() << "]: Message All\n";

            // construct & send to all clients
            netp::net::message<CustomMsgTypes> msg;
            msg.header.id = CustomMsgTypes::ServerMessage;
            msg << client->GetID();
            MessageAllClients(msg, client);
        }
        break;
        }
    }
};

int main()
{
    CustomServer server(60000);
    server.Start();

    while (1)
    {
        server.Update(-1, true);
    }

    return 0;
}