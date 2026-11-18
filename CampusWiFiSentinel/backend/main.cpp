
#include "networking/Socket.h"
#include "networking/UdpServer.h"
#include "database/Database.h"
#include "server/HttpServer.h"
#include "utils/Logger.h"

#include <iostream>
#include <thread>

int main(int argc, char** argv)
{
    net::startup();

    Database db("data/results.csv");

    int port = argc > 1
        ? std::stoi(argv[1])
        : 8080;

    UdpServer udpServer(9090);

    if (!udpServer.start())
    {
        logx::error("Could not start UDP server.");
        net::cleanup();
        return 1;
    }

    std::thread udpThread(
        &UdpServer::run,
        &udpServer
    );

    HttpServer server(db, port, 12);

    logx::info("Starting Campus WiFi Sentinel");

    if (!server.start())
    {
        logx::error("Could not start HTTP server.");

        udpServer.stop();
        udpThread.join();

        net::cleanup();
        return 1;
    }

    udpServer.stop();
    udpThread.join();

    net::cleanup();

    return 0;
}
