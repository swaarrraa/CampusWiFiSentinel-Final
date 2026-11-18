
#include "Database.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>

Database::Database(std::string f)
    : file(std::move(f))
{
    init();
}

void Database::init()
{
    std::filesystem::path parent =
        std::filesystem::path(file).parent_path();

    if (!parent.empty())
        std::filesystem::create_directories(parent);

    std::lock_guard<std::mutex> l(m);

    std::ifstream in(file);

    if (!in.good())
    {
        std::ofstream out(file);

        out << "timestamp,location,device,latitude,longitude,"
            << "throughput_mbps,latency_ms,loss_pct,"
            << "retransmission_proxy_pct,latency_samples,"
            << "bytes,duration_ms,jitter_ms\n";

        return;
    }

    std::string header;
    std::getline(in, header);

    if (header.find("jitter_ms") != std::string::npos)
        return;

    // Migrate the old 12-column CSV without losing old records.
    std::filesystem::path tempFile = file + ".tmp";

    std::ofstream out(tempFile);

    if (!out.is_open())
        return;

    out << "timestamp,location,device,latitude,longitude,"
        << "throughput_mbps,latency_ms,loss_pct,"
        << "retransmission_proxy_pct,latency_samples,"
        << "bytes,duration_ms,jitter_ms\n";

    std::string line;

    while (std::getline(in, line))
    {
        if (!line.empty())
            out << line << ",0\n";
    }

    in.close();
    out.close();

    if (!out.good())
    {
        std::filesystem::remove(tempFile);
        return;
    }

    std::filesystem::copy_file(
        tempFile,
        file,
        std::filesystem::copy_options::overwrite_existing
    );

    std::filesystem::remove(tempFile);
}

// Makes a text value safe for one CSV cell: no commas, no line
// breaks, no control characters.
static std::string q(const std::string& s)
{
    std::string x;
    x.reserve(s.size());

    for (unsigned char c : s)
    {
        if (c == ',' || c < 0x20 || c == 0x7F)
            x += ' ';
        else
            x += (char)c;
    }

    return x;
}

void Database::insert(const TestResult& r)
{
    std::lock_guard<std::mutex> l(m);

    std::ofstream out(file, std::ios::app);

    out << q(r.timestamp) << ","
        << q(r.locationName) << ","
        << q(r.device) << ","
        << r.latitude << ","
        << r.longitude << ","
        << r.throughputMbps << ","
        << r.latencyMs << ","
        << r.packetLossPct << ","
        << r.retransmissionProxyPct << ","
        << r.latencySamples << ","
        << r.bytes << ","
        << r.durationMs << ","
        << r.jitterMs
        << "\n";
}

// Splits one CSV line on commas, keeping empty cells (the old
// std::getline approach silently dropped a trailing empty cell).
static std::vector<std::string> splitLine(std::string line)
{
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
        line.pop_back();

    std::vector<std::string> fields;
    size_t start = 0;

    while (true)
    {
        size_t c = line.find(',', start);

        if (c == std::string::npos)
        {
            fields.push_back(line.substr(start));
            break;
        }

        fields.push_back(line.substr(start, c - start));
        start = c + 1;
    }

    return fields;
}

// Tolerant number read: a damaged cell becomes 0 instead of causing
// the whole historical record to be thrown away.
static double num(const std::vector<std::string>& f, size_t i)
{
    if (i >= f.size() || f[i].empty())
        return 0.0;

    try
    {
        return std::stod(f[i]);
    }
    catch (const std::exception&)
    {
        return 0.0;
    }
}

std::vector<TestResult> Database::all()
{
    std::lock_guard<std::mutex> l(m);

    std::vector<TestResult> results;

    std::ifstream in(file);
    std::string line;

    std::getline(in, line);   // header

    while (std::getline(in, line))
    {
        if (line.empty() || line == "\r")
            continue;

        auto fields = splitLine(line);

        // Older files may have no jitter column (12 cells).
        if (fields.size() < 12)
            continue;

        TestResult r;

        r.timestamp = fields[0];
        r.locationName = fields[1];
        r.device = fields[2];
        r.latitude = fields[3];
        r.longitude = fields[4];

        r.throughputMbps = num(fields, 5);
        r.latencyMs = num(fields, 6);
        r.packetLossPct = num(fields, 7);
        r.retransmissionProxyPct = num(fields, 8);
        r.latencySamples = (int)num(fields, 9);
        r.bytes = (long long)num(fields, 10);
        r.durationMs = num(fields, 11);
        r.jitterMs = num(fields, 12);

        results.push_back(r);
    }

    return results;
}
