
#include "UdpServer.h"

#include <iostream>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using socklen_type = int;
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
using socklen_type = socklen_t;
#ifndef SOCKET_ERROR
#define SOCKET_ERROR (-1)   // Winsock name, portable to Linux/macOS
#endif
#endif

UdpServer::UdpServer(int p)
    : serverSocket(static_cast<socket_t>(-1)),
      running(false),
      port(p)
{
}

bool UdpServer::start()
{
    serverSocket = socket(
        AF_INET,
        SOCK_DGRAM,
        IPPROTO_UDP
    );

    if (serverSocket == static_cast<socket_t>(-1))
    {
        std::cout << "UDP socket creation failed\n";
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (bind(
            serverSocket,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)) == SOCKET_ERROR)
    {
        std::cout << "UDP bind failed. Port may be in use.\n";

#ifdef _WIN32
        closesocket(serverSocket);
#else
        close(serverSocket);
#endif

        serverSocket = static_cast<socket_t>(-1);
        return false;
    }

#ifdef _WIN32
    DWORD timeout = 500;

    setsockopt(
        serverSocket,
        SOL_SOCKET,
        SO_RCVTIMEO,
        reinterpret_cast<const char*>(&timeout),
        sizeof(timeout)
    );
#else
    timeval timeout{};
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;

    setsockopt(
        serverSocket,
        SOL_SOCKET,
        SO_RCVTIMEO,
        &timeout,
        sizeof(timeout)
    );
#endif

    running = true;

    std::cout << "UDP echo server listening on port "
              << port << "\n";

    return true;
}

void UdpServer::run()
{
    char buffer[2048];

    while (running)
    {
        sockaddr_in client{};
        socklen_type length = sizeof(client);

        int received = recvfrom(
            serverSocket,
            buffer,
            sizeof(buffer),
            0,
            reinterpret_cast<sockaddr*>(&client),
            &length
        );

        if (received == SOCKET_ERROR)
            continue;

        if (!running)
            break;

        if (received > 0)
        {
            sendto(
                serverSocket,
                buffer,
                received,
                0,
                reinterpret_cast<sockaddr*>(&client),
                length
            );
        }
    }

#ifdef _WIN32
    closesocket(serverSocket);
#else
    close(serverSocket);
#endif

    serverSocket = static_cast<socket_t>(-1);

    std::cout << "UDP echo server stopped\n";
}

void UdpServer::stop()
{
    running = false;
}
