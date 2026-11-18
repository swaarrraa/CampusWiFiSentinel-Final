
#pragma once

#include "../networking/Socket.h"
#include <atomic>

class UdpServer
{
private:
    socket_t serverSocket;
    std::atomic<bool> running;
    int port;

public:
    explicit UdpServer(int p = 9090);

    bool start();
    void run();
    void stop();
};
