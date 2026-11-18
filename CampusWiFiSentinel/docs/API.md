# HTTP API (port 8080)

All JSON responses are UTF-8 and valid even when stored data is damaged.

## GET /api/ping
Latency probe. `{"ok":true,"server_time_ms":0}`

## GET /api/throughput
Streams 10 MiB (`application/octet-stream`) for throughput timing.

## POST /api/result
Saves one measurement. Body (JSON object):

| Field | Type | Required | Rule |
|---|---|---|---|
| `throughput` | number | yes | 0 - 1,000,000 Mbps |
| `latency` | number | yes | 0 - 10,000,000 ms |
| `loss` | number | yes | 0 - 100 % |
| `jitter` | number | no (default 0) | >= 0 ms |
| `bytes`, `duration_ms` | number | no | >= 0 |
| `lat`, `lng` | number or null | no | stored only if both valid (-90..90, -180..180) |
| `timestamp` | string | no | defaults to server UTC time |
| `location`, `device` | string | no | cleaned, length-limited, defaults applied |

Responses: `201 {"saved":true}` or `400 {"saved":false,"error":"..."}`
(invalid JSON, non-numeric / out-of-range / non-finite values).

## GET /api/results
Array of all stored measurements:

    {"timestamp":"...","location":"...","device":"...","lat":18.46|null,
     "lng":73.86|null,"throughput":0,"latency":0,"loss":0,
     "bytes":0,"duration_ms":0,"jitter":0}

## GET /api/stats
    {"tests":N,"avgThroughput":..,"avgLatency":..,"avgLoss":..,
     "avgJitter":..,"withGps":N}
Throughput average ignores 0 Mbps rows (failed / UDP-only tests), matching
the dashboard. `withGps` counts records with a valid coordinate pair.

## UDP echo (port 9090)
Every datagram is echoed back unchanged.
