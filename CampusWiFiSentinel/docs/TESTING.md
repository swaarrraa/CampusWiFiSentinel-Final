# Testing record

Automated checks were run on a Linux build of the final source against a copy
of the real `data/results.csv` (29 historical records). They did **not**
modify the project data.

| # | Check | Result |
|---|---|---|
| 1 | `GET /api/results` is valid JSON | PASS - all 29 records returned |
| 2 | Damaged Nov 12 record (GPS = control char + `O`) | PASS - returned with `lat`/`lng` null, other fields kept |
| 3 | UDP-only record without GPS | PASS - null coordinates, `jitter` 0.238 kept |
| 4 | `GET /api/stats` | PASS - 29 tests, 27 with valid GPS |
| 5 | POST with commas, quotes, backslash, accents in location | PASS - stored and returned intact |
| 6 | POST with invalid GPS | PASS - saved without coordinates, never reaches map |
| 7 | POST with text / `1e999` / non-JSON body | PASS - HTTP 400 |
| 8 | POST sent in separate packets (headers, then body) | PASS |
| 9 | Persistence across server restart | PASS - identical stats before and after |
| 10 | 200 parallel requests (40 threads) | PASS - 200/200 valid |
| 11 | Client aborting a 10 MB download | PASS - server keeps running |
| 12 | Path traversal (`/../data/results.csv`) | PASS - 404 |
| 13 | Browser run: dashboard shows 29 tests and averages, 0 console errors | PASS |
| 14 | Browser run: GPS -> test -> POST -> CSV, jitter stored | PASS |
| 15 | Map layer receives only finite coordinates (Leaflet stubbed) | PASS - 0 invalid |
| 16 | UDP echo on localhost | PASS - 100/100 replies |

## Still to do on site (cannot be done from a development machine)

- Windows build with `run.bat` (MinGW) and a start-to-finish check.
- Rendering of real map tiles (needs internet).
- Measurements from several phones/laptops at different campus locations,
  GPS accuracy indoors, and UDP over the LAN.

Localhost results do not prove campus-wide WiFi quality.
