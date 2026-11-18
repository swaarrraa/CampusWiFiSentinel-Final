# Architecture

```
Browser (HTML/CSS/JS + Leaflet)
   |  HTTP over TCP :8080                      UDP :9090
   v                                              v
C++17 HttpServer  --enqueue-->  ThreadPool(12)   UdpServer (echo)
   |  per connection: parse request, route
   |-- GET  /api/ping        latency probe
   |-- GET  /api/throughput  10 MB TCP stream
   |-- POST /api/result      validate -> Database::insert
   |-- GET  /api/results     Database::all -> escaped JSON
   |-- GET  /api/stats       aggregates
   '-- GET  /*               static files from web/
                |
                v
        data/results.csv  (mutex-protected, append-only)
```

## Key design points

- **Concurrency:** the accept loop hands every connection to a 12-thread
  pool, so one student's 10 MB transfer never blocks others.
- **Storage:** a mutex-guarded CSV. Writes append one line; reads tolerate
  damaged cells and older 12-column rows (missing jitter reads as 0).
- **Trust boundary:** every value entering the system (POST body) is
  validated; every value leaving it (JSON) is escaped or range-checked.
  GPS is stored only as a complete, in-range pair, otherwise left empty.
- **Robustness:** full request bodies are read via `Content-Length` (not a
  single `recv`), static paths cannot leave `web/`, and a client that
  disconnects mid-download cannot terminate the server (SIGPIPE ignored).
- **UDP:** a separate echo server on 9090 and `UdpLab` client measure RTT,
  loss and jitter on the LAN and can submit the result to the HTTP API.
