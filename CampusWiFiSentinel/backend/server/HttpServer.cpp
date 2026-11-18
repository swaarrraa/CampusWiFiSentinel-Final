
#include "HttpServer.h"
#include "../utils/Logger.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstring>
#include <thread>
#include <algorithm>
#include <string>
#include <stdexcept>
#include <map>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <locale>
#include <cctype>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <unistd.h>
#endif

// ------------------------------------------------------------
// JSON / validation helpers
// ------------------------------------------------------------

// Returns true when `s` is well-formed UTF-8 starting at s[i];
// `len` receives the number of bytes in that character.
static bool utf8Char(const std::string& s, size_t i, size_t& len)
{
    unsigned char c = (unsigned char)s[i];

    if (c < 0x80) { len = 1; return true; }

    size_t need;
    if      ((c & 0xE0) == 0xC0 && c >= 0xC2) need = 1;
    else if ((c & 0xF0) == 0xE0)              need = 2;
    else if ((c & 0xF8) == 0xF0 && c <= 0xF4) need = 3;
    else return false;

    if (i + need >= s.size())
        return false;

    for (size_t k = 1; k <= need; k++)
        if (((unsigned char)s[i + k] & 0xC0) != 0x80)
            return false;

    len = need + 1;
    return true;
}

// Escapes a string so it can be placed between JSON double quotes.
// Control characters and invalid UTF-8 can never break the output.
static std::string jsonEscape(const std::string& s)
{
    static const char* hex = "0123456789abcdef";
    std::string o;
    o.reserve(s.size() + 8);

    for (size_t i = 0; i < s.size();)
    {
        unsigned char c = (unsigned char)s[i];

        if (c == '"')       { o += "\\\""; i++; }
        else if (c == '\\') { o += "\\\\"; i++; }
        else if (c == '\n') { o += "\\n";  i++; }
        else if (c == '\r') { o += "\\r";  i++; }
        else if (c == '\t') { o += "\\t";  i++; }
        else if (c < 0x20 || c == 0x7F)
        {
            o += "\\u00";
            o += hex[c >> 4];
            o += hex[c & 15];
            i++;
        }
        else if (c < 0x80) { o += (char)c; i++; }
        else
        {
            size_t len = 0;

            if (utf8Char(s, i, len))
            {
                o.append(s, i, len);
                i += len;
            }
            else
            {
                o += '?';
                i++;
            }
        }
    }

    return o;
}

// Strict number parser: the WHOLE string must be a finite number.
static bool parseFinite(const std::string& text, double& out)
{
    size_t b = 0;
    size_t e = text.size();

    while (b < e && (text[b] == ' ' || text[b] == '\t' ||
                     text[b] == '\r' || text[b] == '\n'))
        b++;

    while (e > b && (text[e - 1] == ' ' || text[e - 1] == '\t' ||
                     text[e - 1] == '\r' || text[e - 1] == '\n'))
        e--;

    if (b >= e)
        return false;

    std::string t = text.substr(b, e - b);

    // Only plain decimal numbers: rejects hex floats, "inf", "nan".
    for (char c : t)
    {
        if (!((c >= '0' && c <= '9') || c == '.' || c == '-' ||
              c == '+' || c == 'e' || c == 'E'))
            return false;
    }

    char* end = nullptr;
    double v = std::strtod(t.c_str(), &end);

    if (end != t.c_str() + t.size() || !std::isfinite(v))
        return false;

    out = v;
    return true;
}

static std::string formatNumber(double v)
{
    std::ostringstream o;
    o.imbue(std::locale::classic());
    o.precision(15);
    o << v;
    return o.str();
}

// A double as a JSON number (non-finite values become 0).
static std::string jsonNumber(double v)
{
    return std::isfinite(v) ? formatNumber(v) : std::string("0");
}

// A stored coordinate string as a JSON number, or null if it is
// empty, malformed or outside [lo, hi].
static std::string jsonCoordinate(const std::string& text, double lo, double hi)
{
    double v = 0;

    if (!parseFinite(text, v) || v < lo || v > hi)
        return "null";

    return formatNumber(v);
}

// Clean free text from a client: drop control characters, collapse
// whitespace, replace invalid UTF-8 and limit the length.
static std::string cleanText(const std::string& in, size_t maxLen,
                             const std::string& fallback)
{
    std::string o;
    bool space = false;

    for (size_t i = 0; i < in.size() && o.size() < maxLen;)
    {
        unsigned char c = (unsigned char)in[i];

        if (c < 0x20 || c == 0x7F || c == ' ')
        {
            space = !o.empty();
            i++;
            continue;
        }

        size_t len = 1;

        if (c >= 0x80 && !utf8Char(in, i, len))
        {
            i++;
            continue;
        }

        if (space) { o += ' '; space = false; }

        o.append(in, i, len);
        i += len;
    }

    return o.empty() ? fallback : o;
}

static std::string nowIso()
{
    std::time_t t = std::time(nullptr);
    std::tm tmv{};

#ifdef _WIN32
    gmtime_s(&tmv, &t);
#else
    gmtime_r(&t, &tmv);
#endif

    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmv);
    return buf;
}

static void appendUtf8(std::string& o, unsigned cp)
{
    if (cp < 0x80)
        o += (char)cp;
    else if (cp < 0x800)
    {
        o += (char)(0xC0 | (cp >> 6));
        o += (char)(0x80 | (cp & 0x3F));
    }
    else if (cp < 0x10000)
    {
        o += (char)(0xE0 | (cp >> 12));
        o += (char)(0x80 | ((cp >> 6) & 0x3F));
        o += (char)(0x80 | (cp & 0x3F));
    }
    else
    {
        o += (char)(0xF0 | (cp >> 18));
        o += (char)(0x80 | ((cp >> 12) & 0x3F));
        o += (char)(0x80 | ((cp >> 6) & 0x3F));
        o += (char)(0x80 | (cp & 0x3F));
    }
}

static bool hex4(const std::string& s, size_t i, unsigned& out)
{
    if (i + 4 > s.size())
        return false;

    out = 0;

    for (size_t k = 0; k < 4; k++)
    {
        char c = s[i + k];
        out <<= 4;

        if (c >= '0' && c <= '9')      out |= c - '0';
        else if (c >= 'a' && c <= 'f') out |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') out |= c - 'A' + 10;
        else return false;
    }

    return true;
}

// Minimal parser for a FLAT JSON object ({"key": value, ...}).
// String values are unescaped; numbers/true/false/null are kept as
// raw text.  Returns false if the body is not a JSON object.
static bool parseFlatJson(const std::string& b,
                          std::map<std::string, std::string>& out)
{
    size_t i = 0;
    auto skip = [&] {
        while (i < b.size() && (b[i] == ' ' || b[i] == '\t' ||
                                b[i] == '\r' || b[i] == '\n'))
            i++;
    };
    auto readString = [&](std::string& r) -> bool {
        if (i >= b.size() || b[i] != '"') return false;
        i++;
        r.clear();

        while (i < b.size())
        {
            char c = b[i++];

            if (c == '"') return true;

            if (c != '\\') { r += c; continue; }

            if (i >= b.size()) return false;

            char e = b[i++];

            switch (e)
            {
                case '"':  r += '"';  break;
                case '\\': r += '\\'; break;
                case '/':  r += '/';  break;
                case 'b':  r += '\b'; break;
                case 'f':  r += '\f'; break;
                case 'n':  r += '\n'; break;
                case 'r':  r += '\r'; break;
                case 't':  r += '\t'; break;
                case 'u':
                {
                    unsigned cp = 0;
                    if (!hex4(b, i, cp)) return false;
                    i += 4;

                    if (cp >= 0xD800 && cp <= 0xDBFF &&
                        i + 6 <= b.size() && b[i] == '\\' && b[i + 1] == 'u')
                    {
                        unsigned lo = 0;
                        if (hex4(b, i + 2, lo) && lo >= 0xDC00 && lo <= 0xDFFF)
                        {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            i += 6;
                        }
                    }

                    appendUtf8(r, cp);
                    break;
                }
                default: return false;
            }
        }

        return false;
    };

    skip();
    if (i >= b.size() || b[i] != '{') return false;
    i++;
    skip();

    if (i < b.size() && b[i] == '}') return true;

    while (i < b.size())
    {
        std::string key, val;

        skip();
        if (!readString(key)) return false;
        skip();
        if (i >= b.size() || b[i] != ':') return false;
        i++;
        skip();
        if (i >= b.size()) return false;

        if (b[i] == '"')
        {
            if (!readString(val)) return false;
        }
        else
        {
            size_t st = i;

            while (i < b.size() && b[i] != ',' && b[i] != '}')
                i++;

            val = b.substr(st, i - st);

            while (!val.empty() && (val.back() == ' ' || val.back() == '\r' ||
                                    val.back() == '\n' || val.back() == '\t'))
                val.pop_back();
        }

        out[key] = val;

        skip();
        if (i >= b.size()) return false;
        if (b[i] == ',') { i++; continue; }
        if (b[i] == '}') return true;
        return false;
    }

    return false;
}

static std::string mime(const std::string& p)
{
    auto ends = [&](const char* x) {
        std::string e(x);
        return p.size() >= e.size() &&
               p.compare(p.size() - e.size(), e.size(), e) == 0;
    };

    if (ends(".css"))  return "text/css";
    if (ends(".js"))   return "application/javascript";
    if (ends(".json")) return "application/json";
    if (ends(".svg"))  return "image/svg+xml";
    if (ends(".png"))  return "image/png";
    if (ends(".ico"))  return "image/x-icon";

    return "text/html";
}

std::string HttpServer::file(const std::string& path)
{
    std::string p = path;

    if (p == "/")
        p = "/index.html";

    // Never serve anything outside the web/ folder.
    if (p.empty() || p[0] != '/' ||
        p.find("..") != std::string::npos ||
        p.find('\\') != std::string::npos ||
        p.find('\0') != std::string::npos)
        return "";

    std::ifstream in("web" + p, std::ios::binary);

    if (!in)
        return "";

    std::ostringstream ss;
    ss << in.rdbuf();

    return ss.str();
}

void HttpServer::response(
    socket_t s,
    const std::string& status,
    const std::string& type,
    const std::string& body)
{
    std::ostringstream h;

    h << "HTTP/1.1 " << status << "\r\n"
      << "Content-Type: " << type << "; charset=utf-8\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "Connection: close\r\n"
      << "Cache-Control: no-store\r\n"
      << "\r\n";

    std::string header = h.str();

    net::send_all(
        s,
        header.c_str(),
        header.size()
    );

    net::send_all(
        s,
        body.data(),
        body.size()
    );
}

void HttpServer::throughput(socket_t s)
{
    const size_t total = 10 * 1024 * 1024;
    const size_t chunk = 64 * 1024;

    std::string data(chunk, 'W');

    std::ostringstream h;

    h << "HTTP/1.1 200 OK\r\n"
      << "Content-Type: application/octet-stream\r\n"
      << "Content-Length: " << total << "\r\n"
      << "Connection: close\r\n"
      << "Cache-Control: no-store\r\n"
      << "\r\n";

    auto header = h.str();

    if (!net::send_all(
            s,
            header.data(),
            header.size()))
    {
        return;
    }

    size_t sent = 0;

    while (sent < total)
    {
        size_t n = std::min(
            chunk,
            total - sent
        );

        if (!net::send_all(
                s,
                data.data(),
                n))
        {
            break;
        }

        sent += n;
    }
}

void HttpServer::client(
    socket_t s,
    std::string ip)
{
    // Read the full request: headers first, then (for POST) the
    // complete body as announced by Content-Length.
    std::string req;
    {
        char buf[8192];
        const size_t maxRequest = 256 * 1024;
        size_t headerEnd = std::string::npos;

        while (headerEnd == std::string::npos && req.size() < maxRequest)
        {
            int n = recv(s, buf, sizeof(buf), 0);

            if (n <= 0)
                break;

            req.append(buf, (size_t)n);
            headerEnd = req.find("\r\n\r\n");
        }

        if (req.empty())
        {
            net::close_socket(s);
            return;
        }

        if (headerEnd != std::string::npos)
        {
            std::string head = req.substr(0, headerEnd);
            std::string lower = head;

            for (auto& ch : lower)
                ch = (char)std::tolower((unsigned char)ch);

            size_t want = 0;
            auto cl = lower.find("content-length:");

            if (cl != std::string::npos)
                want = (size_t)std::strtoull(
                    lower.c_str() + cl + 15, nullptr, 10);

            if (want > maxRequest)
                want = maxRequest;

            while (req.size() < headerEnd + 4 + want)
            {
                int n = recv(s, buf, sizeof(buf), 0);

                if (n <= 0)
                    break;

                req.append(buf, (size_t)n);
            }
        }
    }

    std::istringstream ss(req);

    std::string method;
    std::string path;
    std::string ver;

    ss >> method >> path >> ver;

    auto qpos = path.find('?');

    if (qpos != std::string::npos)
        path = path.substr(0, qpos);

    logx::info(
        ip + " " +
        method + " " +
        path
    );

    // LATENCY / PING API

    if (method == "GET" && path == "/api/ping")
    {
        response(
            s,
            "200 OK",
            "application/json",
            "{\"ok\":true,\"server_time_ms\":0}"
        );

        net::close_socket(s);
        return;
    }

    // THROUGHPUT API

    if (method == "GET" && path == "/api/throughput")
    {
        throughput(s);

        net::close_socket(s);
        return;
    }

    // RESULTS API

    if (method == "GET" && path == "/api/results")
    {
        auto v = db.all();

        std::ostringstream o;

        o << "[";

        for (size_t i = 0; i < v.size(); i++)
        {
            if (i)
                o << ",";

            auto& r = v[i];

            // Every field is validated / escaped, so one damaged
            // historical record can never invalidate the response.
            o << "{"
              << "\"timestamp\":\"" << jsonEscape(r.timestamp) << "\","
              << "\"location\":\""  << jsonEscape(r.locationName) << "\","
              << "\"device\":\""    << jsonEscape(r.device) << "\","
              << "\"lat\":"  << jsonCoordinate(r.latitude,  -90.0,  90.0) << ","
              << "\"lng\":"  << jsonCoordinate(r.longitude, -180.0, 180.0) << ","
              << "\"throughput\":"  << jsonNumber(r.throughputMbps) << ","
              << "\"latency\":"     << jsonNumber(r.latencyMs) << ","
              << "\"loss\":"        << jsonNumber(r.packetLossPct) << ","
              << "\"bytes\":"       << r.bytes << ","
              << "\"duration_ms\":" << jsonNumber(r.durationMs) << ","
              << "\"jitter\":"      << jsonNumber(r.jitterMs)
              << "}";
        }

        o << "]";

        response(s, "200 OK", "application/json", o.str());

        net::close_socket(s);
        return;
    }

    // STATISTICS API

    if (method == "GET" && path == "/api/stats")
    {
        auto v = db.all();

        double at = 0, al = 0, loss = 0, jit = 0;
        size_t tCount = 0, jCount = 0, gps = 0;

        for (auto& r : v)
        {
            // Same rules as the dashboard: a 0 Mbps row (failed or
            // UDP-only test) does not drag the throughput average down.
            if (std::isfinite(r.throughputMbps) && r.throughputMbps > 0)
            {
                at += r.throughputMbps;
                tCount++;
            }

            if (std::isfinite(r.latencyMs))    al += r.latencyMs;
            if (std::isfinite(r.packetLossPct)) loss += r.packetLossPct;

            if (std::isfinite(r.jitterMs) && r.jitterMs > 0)
            {
                jit += r.jitterMs;
                jCount++;
            }

            if (jsonCoordinate(r.latitude, -90, 90) != "null" &&
                jsonCoordinate(r.longitude, -180, 180) != "null")
                gps++;
        }

        double n = (double)v.size();

        std::ostringstream o;

        o << "{"
          << "\"tests\":" << v.size()
          << ",\"avgThroughput\":" << jsonNumber(tCount ? at / tCount : 0)
          << ",\"avgLatency\":"    << jsonNumber(n ? al / n : 0)
          << ",\"avgLoss\":"       << jsonNumber(n ? loss / n : 0)
          << ",\"avgJitter\":"     << jsonNumber(jCount ? jit / jCount : 0)
          << ",\"withGps\":"       << gps
          << "}";

        response(s, "200 OK", "application/json", o.str());

        net::close_socket(s);
        return;
    }

    // SAVE TEST RESULT API

    if (method == "POST" && path == "/api/result")
    {
        auto pos = req.find("\r\n\r\n");

        std::string body =
            pos == std::string::npos ? "" : req.substr(pos + 4);

        std::map<std::string, std::string> f;

        auto bad = [&](const std::string& why)
        {
            response(
                s,
                "400 Bad Request",
                "application/json",
                "{\"saved\":false,\"error\":\"" + jsonEscape(why) + "\"}");

            net::close_socket(s);
        };

        if (!parseFlatJson(body, f))
        {
            bad("Request body is not valid JSON");
            return;
        }

        auto field = [&](const char* k) -> std::string
        {
            auto it = f.find(k);
            return it == f.end() ? std::string() : it->second;
        };

        // Required numeric field: must be a finite number in range.
        auto number = [&](const char* k, double lo, double hi,
                          bool required, double& out) -> bool
        {
            std::string t = field(k);

            if (t.empty() || t == "null")
            {
                out = 0;
                return !required;
            }

            return parseFinite(t, out) && out >= lo && out <= hi;
        };

        TestResult r;

        double thr = 0, lat = 0, loss = 0, jit = 0, dur = 0, byt = 0;

        if (!number("throughput", 0, 1e6, true, thr) ||
            !number("latency", 0, 1e7, true, lat) ||
            !number("loss", 0, 100, true, loss) ||
            !number("jitter", 0, 1e7, false, jit) ||
            !number("duration_ms", 0, 1e9, false, dur) ||
            !number("bytes", 0, 1e12, false, byt))
        {
            bad("Invalid measurement data");
            return;
        }

        r.timestamp = cleanText(field("timestamp"), 40, nowIso());
        r.locationName = cleanText(field("location"), 120, "GPS Test Location");
        r.device = cleanText(field("device"), 60, "Unknown");

        // GPS: store only a complete, valid pair - otherwise store
        // nothing, so a bad value can never reach the map.
        double la = 0, lo = 0;

        if (parseFinite(field("lat"), la) &&
            parseFinite(field("lng"), lo) &&
            la >= -90 && la <= 90 && lo >= -180 && lo <= 180)
        {
            r.latitude = formatNumber(la);
            r.longitude = formatNumber(lo);
        }

        r.throughputMbps = thr;
        r.latencyMs = lat;
        r.packetLossPct = loss;
        r.retransmissionProxyPct = loss;
        r.jitterMs = jit;
        r.latencySamples = 10;
        r.bytes = (long long)byt;
        r.durationMs = dur;

        db.insert(r);

        response(s, "201 Created", "application/json", "{\"saved\":true}");

        net::close_socket(s);
        return;
    }

    // STATIC WEB FILES

    std::string body = file(path);

    if (body.empty())
    {
        response(
            s,
            "404 Not Found",
            "text/plain",
            "Not found"
        );
    }
    else
    {
        response(
            s,
            "200 OK",
            mime(path),
            body
        );
    }

    net::close_socket(s);
}

bool HttpServer::start()
{
    listenSock = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (listenSock < 0)
        return false;

    int yes = 1;

    setsockopt(
        listenSock,
        SOL_SOCKET,
        SO_REUSEADDR,
        (char*)&yes,
        sizeof(yes)
    );

    sockaddr_in a{};

    a.sin_family = AF_INET;
    a.sin_addr.s_addr = INADDR_ANY;
    a.sin_port = htons(port);

    if (bind(
            listenSock,
            (sockaddr*)&a,
            sizeof(a)) < 0)
    {
        return false;
    }

    if (listen(listenSock, 128) < 0)
    {
        return false;
    }

    running = true;

    logx::info(
        "Campus WiFi Sentinel listening on port " +
        std::to_string(port)
    );

    while (running)
    {
        sockaddr_in c{};

#ifdef _WIN32
        int len = sizeof(c);
#else
        socklen_t len = sizeof(c);
#endif

        socket_t s = accept(
            listenSock,
            (sockaddr*)&c,
            &len
        );

        if (s < 0)
            continue;

        char ip[INET_ADDRSTRLEN];

        inet_ntop(
            AF_INET,
            &c.sin_addr,
            ip,
            sizeof(ip)
        );

        pool.enqueue(
            [
                this,
                s,
                ip = std::string(ip)
            ]
            {
                client(s, ip);
            }
        );
    }

    return true;
}

void HttpServer::stop()
{
    running = false;

    net::close_socket(listenSock);
}
