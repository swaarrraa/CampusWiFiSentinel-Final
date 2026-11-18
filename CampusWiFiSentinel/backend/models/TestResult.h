
#pragma once

#include <string>

struct TestResult
{
    std::string timestamp, locationName, device, latitude, longitude;

    double throughputMbps = 0;
    double latencyMs = 0;
    double packetLossPct = 0;
    double retransmissionProxyPct = 0;
    double jitterMs = 0;

    int latencySamples = 0;
    long long bytes = 0;
    double durationMs = 0;
};
