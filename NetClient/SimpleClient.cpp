#include <iostream>
#include "../NetCommon/netp_net.h"
#include <ncurses.h>

enum class CustomMsgTypes : uint32_t
{
    ServerAccept,
    ServerDeny,
    ServerPing,
    MessageAll,
    ServerMessage,
};

class CustomClient : public netp::net::client_interface<CustomMsgTypes>
{
public:
    void PingServer()
    {
        netp::net::message<CustomMsgTypes> msg;
        msg.header.id = CustomMsgTypes::ServerPing;

        // based on system time, care when using with client/server. measures round trip time from client->server->client
        std::chrono::system_clock::time_point timeNow = std::chrono::system_clock::now();

        msg << timeNow;
        m_connection->Send(msg);
    }

    void MessageAll()
    {
        netp::net::message<CustomMsgTypes> msg;
        msg.header.id = CustomMsgTypes::MessageAll;
        m_connection->Send(msg);
    }
};

int kbhit(void)
{
    int ch = getch();

    if (ch != ERR)
    {
        ungetch(ch);
        return 1;
    }
    else
    {
        return 0;
    }
}

int main()
{
    initscr();

    cbreak();
    noecho();
    nodelay(stdscr, TRUE);

    scrollok(stdscr, TRUE);

    CustomClient c;
    c.Connect("127.0.0.1", 60000);

    //  bool key[3] = {false, false, false};
    // bool old_key[3] = {false, false, false};
    bool bQuit = false;
    while (!bQuit)
    {

        // if (GetForegroundWindow() == GetConsoleWindow()) // also windows lib
        // {
        //     // windows: GetAsyncKeyState, Linux: todo, ncurses library
        //     // checking for inputs '1','2','3'. 3 quits client, 1 pings server
        // }
        if (c.IsConnected())
        {
            // if 1 pressed{
            // c.PingServer();
            //}
            // if 3 pressed{
            // bQuit = true;
            //}
            if (kbhit())
            {
                int inp = getch();
                // printw("Key pressed! It was: %d\n", inp);
                refresh();

                switch (inp)
                {
                case 49:
                {
                    c.PingServer();
                    break;
                }
                case 50:
                {
                    c.MessageAll();
                    break;
                }
                case 51:
                {
                    bQuit = true;
                    break;
                }
                default:
                {
                    break;
                }
                }
            }
            else
            {
                // printw("No key pressed yet...\n");
                // refresh();
            }
            if (!c.Incoming().empty())
            {
                auto msg = c.Incoming().pop_front().msg;

                switch (msg.header.id)
                {
                case CustomMsgTypes::ServerAccept:
                {
                    // server has responded to ping request
                    std::cout << "Server Accepted Connection\n\r";
                }
                break;

                case CustomMsgTypes::ServerPing:
                {
                    std::chrono::system_clock::time_point timeNow = std::chrono::system_clock::now();
                    std::chrono::system_clock::time_point timeThen;
                    msg >> timeThen;
                    std::cout << "Ping: " << std::chrono::duration<double>(timeNow - timeThen).count() << "\n\r";
                }
                break;

                case CustomMsgTypes::ServerMessage:
                {
                    // server responded to ping request
                    uint32_t clientID;
                    msg >> clientID;
                    std::cout << "Hello from [" << clientID << "]\n\r";
                }
                break;
                }
            }
        }
        else
        {
            std::cout << "\rServer Down\n";
            bQuit = true;
        }
    }

    return 0;
}