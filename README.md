# UniversalMMOServer

Networking Server/Client framework

Created with ASIO and ncurses libraries. Please ensure you have these installed before compiling.

You can run this program by compiling and running the SimpleServer.cpp and SimpleClient.cpp files.

Please compile the SimpleClient Server with the -lncurses and -pthread tags.

```
g++ SimpleClient.cpp -lncurses -pthread
```
The Server file must be run first for the Client to attach to it.
