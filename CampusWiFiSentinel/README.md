# Campus WiFi Sentinel

**Campus WiFi Dead-Zone Mapper** - a C++17 platform that measures real WiFi
performance at physical locations on campus and shows the results on an
interactive map, so weak or unreliable areas can be found with evidence
instead of complaints.

| Layer | Technology |
|---|---|
| Frontend | HTML, CSS, JavaScript, Leaflet.js, OpenStreetMap tiles |
| Backend | C++17, custom multithreaded HTTP server over TCP sockets |
| Networking | TCP (HTTP + 10 MB throughput stream), UDP echo server (port 9090) |
| Storage | CSV file (`data/results.csv`), no third-party database |
| Location | Browser Geolocation API, Nominatim search |

## What is measured

| Metric | How |
|---|---|
| Throughput (Mbps) | 10 MB TCP download from the server, timed in the browser |
| Latency (ms) | Average of 10 request/response round trips |
| Jitter (ms) | Mean absolute difference between consecutive RTT samples |
| Loss (%) | Browser-safe *proxy* (browsers cannot read TCP retransmission counters) |
| GPS + time + device | Browser Geolocation API, ISO timestamp, device type |

## Quick start (Windows)

1. Install MinGW-w64 g++ (the project was built with WinLibs via `winget`).
2. Double-click **`run.bat`** - it builds the server and starts it.
   (Or manually: `build.bat`, then `bin\campus_wifi_server.exe`.)
3. Open <http://localhost:8080>.
4. For a phone on the same WiFi use the laptop's IPv4 address from
   `ipconfig`, e.g. `http://192.168.1.10:8080` (allow ports 8080 TCP and
   9090 UDP in Windows Firewall).

## Quick start (Linux / macOS)

    ./build.sh && ./bin/campus_wifi_server

The first page load needs internet access for the map library and tiles
(Leaflet from unpkg, tiles from OpenStreetMap).

## Pages

- `/` - dashboard: totals, averages, recent measurements
- `/run-test.html` - take a measurement at your current location
- `/heatmap.html` - campus map with markers, grouped measurements, heat layer

## Project layout

    backend/server       HTTP server, JSON API, validation
    backend/database     CSV storage
    backend/networking   TCP helpers, UDP echo server, UDP lab client
    backend/threading    12-worker thread pool
    web/                 dashboard, run-test and heatmap pages
    data/                results.csv + backups (never reset)
    docs/                architecture, API, testing, demo guide, limitations
    archive/             earlier page versions kept for reference (not served)

More detail: `docs/architecture.md`, `docs/API.md`, `docs/TESTING.md`,
`docs/DEMO_GUIDE.md`, `docs/KNOWN_LIMITATIONS.md`.

## Data safety

`data/results.csv` is the live database. It is only ever appended to. Damaged
historical rows (for example a record with unreadable GPS) are kept and
returned with `lat`/`lng` = `null`; they are simply not drawn on the map.
