# UniversalMMOServer

Networking Server/Client framework

Created with ASIO and ncurses libraries. Please ensure you have these installed before compiling.

You can run this program by compiling and running the SimpleServer.cpp and SimpleClient.cpp files.

Please compile the SimpleClient Server with the -lncurses and -pthread tags.

```
g++ SimpleClient.cpp -lncurses -pthread
```
The Server file must be run first for the Client to attach to it.

This program was initially created on a Rocky-Linux image.
This program followed a tutorial created by [Javidx9](https://github.com/OneLoneCoder) with minor changes and a revamped input system with the ncurses library which works on non-Windows systems.

Sample of Server running with two clients.
![image](https://github.com/user-attachments/assets/1e7e3214-5e4b-4da7-b98e-f1b9e0f20f16)


