# Known limitations (state these honestly in the viva)

- **Loss is a proxy.** Browsers cannot read TCP retransmissions, so loss is
  an estimate derived from latency behaviour. Real packet loss is measured
  only by the UDP lab on the LAN.
- **GPS indoors is imprecise** (often tens of metres). Use the manual
  location tag (e.g. "Library 2nd floor") and compare with the coordinates.
- **Map needs internet** for Leaflet and OpenStreetMap tiles; measurement
  and the dashboard numbers work on a LAN without it.
- **CSV storage** is ideal for a demo-size dataset; thousands of rows per
  second would call for a real database.
- **No authentication**: anyone on the network can submit a result.
- Older records (September) were taken before jitter/duration were
  captured and show 0 for those fields.
