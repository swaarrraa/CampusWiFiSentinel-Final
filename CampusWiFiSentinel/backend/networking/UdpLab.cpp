
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <cstring>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <ctime>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using socklen_type = int;
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
using SOCKET = int;
using socklen_type = socklen_t;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
#endif

using Clock = std::chrono::steady_clock;

bool initialize()
{
#ifdef _WIN32
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
#else
    return true;
#endif
}

void cleanup()
{
#ifdef _WIN32
    WSACleanup();
#endif
}

std::string getTimestamp()
{
    std::time_t now = std::time(nullptr);
    std::tm utc{};

#ifdef _WIN32
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif

    char buffer[30];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return buffer;
}

bool sendAll(SOCKET socket, const std::string& data)
{
    size_t totalSent = 0;

    while (totalSent < data.size())
    {
        int sent = send(
            socket,
            data.c_str() + totalSent,
            static_cast<int>(data.size() - totalSent),
            0
        );

        if (sent <= 0)
            return false;

        totalSent += sent;
    }

    return true;
}

void saveResult(
    const std::string& ip,
    double averageRtt,
    double loss,
    double averageJitter,
    double durationMs
)
{
    SOCKET httpSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (httpSocket == INVALID_SOCKET)
    {
        std::cout << "Could not create HTTP socket. Result not saved.\n";
        return;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(8080);

    if (inet_pton(AF_INET, ip.c_str(), &address.sin_addr) != 1)
    {
        std::cout << "Invalid HTTP server IP. Result not saved.\n";
        closesocket(httpSocket);
        return;
    }

    if (connect(
        httpSocket,
        (sockaddr*)&address,
        sizeof(address)
    ) == SOCKET_ERROR)
    {
        std::cout << "Could not connect to HTTP API at "
                  << ip << ":8080. Result not saved.\n";
        closesocket(httpSocket);
        return;
    }

    std::ostringstream body;

    body << std::fixed << std::setprecision(3)
         << "{"
         << "\"timestamp\":\"" << getTimestamp() << "\","
         << "\"location\":\"UDP test - GPS unavailable\","
         << "\"device\":\"UDP Client\","
         << "\"lat\":\"\","
         << "\"lng\":\"\","
         << "\"throughput\":0,"
         << "\"latency\":" << averageRtt << ","
         << "\"loss\":" << loss << ","
         << "\"jitter\":" << averageJitter << ","
         << "\"bytes\":0,"
         << "\"duration_ms\":" << durationMs
         << "}";

    std::string json = body.str();

    std::ostringstream request;

    request << "POST /api/result HTTP/1.1\r\n"
            << "Host: " << ip << ":8080\r\n"
            << "Content-Type: application/json\r\n"
            << "Content-Length: " << json.size() << "\r\n"
            << "Connection: close\r\n"
            << "\r\n"
            << json;

    bool sent = sendAll(httpSocket, request.str());

    if (!sent)
    {
        std::cout << "Failed to send result to HTTP API.\n";
        closesocket(httpSocket);
        return;
    }

    char response[2048]{};

    int received = recv(
        httpSocket,
        response,
        sizeof(response) - 1,
        0
    );

    if (received > 0)
    {
        response[received] = '\0';

        if (std::strstr(response, "200") != nullptr)
            std::cout << "UDP result successfully saved to database.\n";
        else
            std::cout << "HTTP API response:\n" << response << "\n";
    }
    else
    {
        std::cout << "No response received from HTTP API.\n";
    }

    closesocket(httpSocket);
}

void runServer(int port)
{
    SOCKET server = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (server == INVALID_SOCKET)
    {
        std::cout << "UDP socket creation failed\n";
        return;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server, (sockaddr*)&address, sizeof(address)) == SOCKET_ERROR)
    {
        std::cout << "UDP bind failed. Port may be in use.\n";
        closesocket(server);
        return;
    }

    std::cout << "UDP echo server listening on port "
              << port << "\n";

    char buffer[1024];

    while (true)
    {
        sockaddr_in client{};
        socklen_type length = sizeof(client);

        int received = recvfrom(
            server,
            buffer,
            sizeof(buffer),
            0,
            (sockaddr*)&client,
            &length
        );

        if (received <= 0)
            continue;

        sendto(
            server,
            buffer,
            received,
            0,
            (sockaddr*)&client,
            length
        );
    }

    closesocket(server);
}

void runClient(const std::string& ip, int port)
{
    SOCKET client = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (client == INVALID_SOCKET)
    {
        std::cout << "UDP socket creation failed\n";
        return;
    }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &server.sin_addr) != 1)
    {
        std::cout << "Invalid IPv4 address\n";
        closesocket(client);
        return;
    }

#ifdef _WIN32
    DWORD timeout = 500;
    setsockopt(
        client,
        SOL_SOCKET,
        SO_RCVTIMEO,
        (const char*)&timeout,
        sizeof(timeout)
    );
#else
    timeval timeout{};
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;

    setsockopt(
        client,
        SOL_SOCKET,
        SO_RCVTIMEO,
        &timeout,
        sizeof(timeout)
    );
#endif

    const int totalPackets = 100;
    int receivedPackets = 0;

    double totalRtt = 0;
    double previousRtt = -1;
    double totalJitter = 0;

    std::vector<double> rtts;

    auto testStart = Clock::now();

    std::cout << "\nUDP Network Measurement\n";
    std::cout << "Destination: " << ip << ":" << port << "\n";
    std::cout << "Packets: " << totalPackets << "\n\n";

    for (int seq = 1; seq <= totalPackets; seq++)
    {
        auto start = Clock::now();

        std::string message =
            std::to_string(seq) + "|" +
            std::to_string(
                std::chrono::duration_cast<
                    std::chrono::nanoseconds
                >(
                    start.time_since_epoch()
                ).count()
            );

        int sent = sendto(
            client,
            message.c_str(),
            (int)message.size(),
            0,
            (sockaddr*)&server,
            sizeof(server)
        );

        if (sent == SOCKET_ERROR)
        {
            std::cout << "Send failed for packet "
                      << seq << "\n";
            continue;
        }

        char buffer[1024];

        sockaddr_in from{};
        socklen_type fromLength = sizeof(from);

        int n = recvfrom(
            client,
            buffer,
            sizeof(buffer) - 1,
            0,
            (sockaddr*)&from,
            &fromLength
        );

        if (n <= 0)
        {
            std::cout << "Packet " << seq
                      << ": timeout\n";
        }
        else
        {
            auto end = Clock::now();

            double rtt =
                std::chrono::duration<double, std::milli>(
                    end - start
                ).count();

            receivedPackets++;
            totalRtt += rtt;
            rtts.push_back(rtt);

            if (previousRtt >= 0)
                totalJitter += std::abs(rtt - previousRtt);

            previousRtt = rtt;

            std::cout << "Packet " << seq
                      << ": RTT = "
                      << std::fixed
                      << std::setprecision(3)
                      << rtt << " ms\n";
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(20)
        );
    }

    auto testEnd = Clock::now();

    double durationMs =
        std::chrono::duration<double, std::milli>(
            testEnd - testStart
        ).count();

    int lostPackets = totalPackets - receivedPackets;

    double loss =
        100.0 * lostPackets / totalPackets;

    double averageRtt =
        receivedPackets
            ? totalRtt / receivedPackets
            : 0;

    double averageJitter =
        receivedPackets > 1
            ? totalJitter / (receivedPackets - 1)
            : 0;

    std::cout << "\n========== RESULTS ==========\n";
    std::cout << "Packets sent: " << totalPackets << "\n";
    std::cout << "Packets received: " << receivedPackets << "\n";
    std::cout << "Packets lost: " << lostPackets << "\n";
    std::cout << "Packet loss: " << loss << "%\n";
    std::cout << "Average RTT: " << averageRtt << " ms\n";
    std::cout << "RTT variation (jitter proxy): "
              << averageJitter << " ms\n";
    std::cout << "Test duration: " << durationMs << " ms\n";
    std::cout << "=============================\n";

    closesocket(client);

    std::cout << "\nSending results to database...\n";

    saveResult(
        ip,
        averageRtt,
        loss,
        averageJitter,
        durationMs
    );
}

int main(int argc, char* argv[])
{
    if (!initialize())
    {
        std::cout << "Network initialization failed\n";
        return 1;
    }

    if (argc < 2)
    {
        std::cout << "Usage:\n";
        std::cout << "UdpLab server [port]\n";
        std::cout << "UdpLab client [IP] [port]\n";
        cleanup();
        return 1;
    }

    std::string mode = argv[1];

    if (mode == "server")
    {
        int port = argc >= 3 ? std::stoi(argv[2]) : 9090;
        runServer(port);
    }
    else if (mode == "client")
    {
        std::string ip = argc >= 3 ? argv[2] : "127.0.0.1";
        int port = argc >= 4 ? std::stoi(argv[3]) : 9090;
        runClient(ip, port);
    }
    else
    {
        std::cout << "Unknown mode\n";
    }

    cleanup();
    return 0;
}
