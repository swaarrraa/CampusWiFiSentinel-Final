# Demonstration guide

1. Run `run.bat`; the dashboard opens at http://localhost:8080.
2. **Dashboard:** show total tests, averages and the recent measurements log
   (real data collected since September).
3. **Run Test:** click *Use GPS*, optionally type an indoor tag, press
   *Run Full Test*. Explain: 10 MB TCP transfer -> Mbps, 10 round trips ->
   latency + jitter, then the health score and dead-zone warning.
4. **Heatmap:** open the page; the new measurement appears at your location.
   Green = healthy, red = potential dead zone; nearby tests are grouped.
5. **Phone on the same WiFi:** open `http://<laptop-ip>:8080`, repeat at a
   different location and show both devices in the log.
6. **Resilience:** stop and restart the server - all history is still there.
7. **UDP:** run `build_udp_lab.bat`, then (server already running) `bin\UdpLab.exe client <server-ip>` to show
   loss/RTT/jitter over the LAN; the result is saved to the dashboard.
8. **Concurrency:** `build_load_test.bat` and run the stress client against
   `/api/ping` (`bin\stress_client.exe <ip> 8080 50`) to show the thread pool at work.

Talking points are in `docs/presentation_points.md`; limitations in
`docs/KNOWN_LIMITATIONS.md`.
