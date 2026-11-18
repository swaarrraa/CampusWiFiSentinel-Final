
// ============================================================
// CAMPUS WIFI
// Frontend JavaScript
// ============================================================


// ============================================================
// GLOBAL VARIABLES
// ============================================================

var map = null;
var zoneLayers = [];
var measurementLayers = [];

var defaultCenter = [18.4639, 73.8677];


// THEME - mirrors CSS :root palette

function getThemeColor(name, fallback) {
    try {
        var v = getComputedStyle(document.documentElement).getPropertyValue(name);
        v = (v || '').trim();
        return v || fallback;
    } catch (e) {
        return fallback;
    }
}

var THEME = {
    lightest: '#E3F2FD',
    light: '#90CAF9',
    primary: '#2196F3',
    dark: '#0D47A1',
    success: '#1B7A3D',
    warning: '#8A5D00',
    danger: '#B3261E',
    muted: '#33507A'
};

try {
    THEME.lightest = getThemeColor('--palette-50', THEME.lightest);
    THEME.light = getThemeColor('--palette-200', THEME.light);
    THEME.primary = getThemeColor('--palette-500', THEME.primary);
    THEME.dark = getThemeColor('--palette-900', THEME.dark);
    THEME.success = getThemeColor('--health-good', THEME.success);
    THEME.warning = getThemeColor('--health-moderate', THEME.warning);
    THEME.danger = getThemeColor('--health-poor', THEME.danger);
    THEME.muted = getThemeColor('--muted', THEME.muted);
} catch (e) {}


var currentLocation = null;
var searchedLocation = null;
var searchMarker = null;


// ============================================================
// CAMPUS ZONES
// ============================================================

var zones = [
    {
        name: "Main Academic Area",
        center: [18.4643, 73.8674]
    },
    {
        name: "Library Area",
        center: [18.4635, 73.8680]
    },
    {
        name: "Canteen Area",
        center: [18.4637, 73.8690]
    },
    {
        name: "Main Gate Area",
        center: [18.4628, 73.8681]
    },
    {
        name: "Ground/Open Area",
        center: [18.4642, 73.8663]
    },
    {
        name: "South Campus Area",
        center: [18.4626, 73.8668]
    }
];


// ============================================================
// HELPER FUNCTIONS
// ============================================================

function getElement(id) {
    return document.getElementById(id);
}


function setText(id, value) {
    var element = getElement(id);

    if (element) {
        element.textContent = value;
    }
}


function setProgress(value) {
    var progress = getElement("progress");

    if (!progress) {
        return;
    }

    var bar = progress.querySelector("div");

    if (bar) {
        bar.style.width = value + "%";
    }
}


// Safely convert measurements into valid non-negative numbers.

function validMeasurement(value) {
    if (
        value === null ||
        value === undefined ||
        value === ""
    ) {
        return null;
    }

    var number = Number(value);

    if (
        !Number.isFinite(number) ||
        number < 0
    ) {
        return null;
    }

    return number;
}


// Safely validate GPS coordinates.

function validCoordinate(value, min, max) {
    if (
        value === null ||
        value === undefined ||
        String(value).trim() === ""
    ) {
        return null;
    }

    var number = Number(value);

    if (
        !Number.isFinite(number) ||
        number < min ||
        number > max
    ) {
        return null;
    }

    return number;
}


// ============================================================
// MAP INITIALIZATION
// ============================================================

function initializeMap() {
    var mapCanvas = getElement("mapCanvas");

    if (!mapCanvas) {
        return;
    }

    if (typeof L === "undefined") {
        console.error("Leaflet library is not loaded.");
        return;
    }

    map = L.map("mapCanvas").setView(defaultCenter, 17);

    L.tileLayer(
        "https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png",
        {
            maxZoom: 19,
            attribution: "&copy; OpenStreetMap contributors"
        }
    ).addTo(map);


    // ADD CAMPUS ZONES

    for (var i = 0; i < zones.length; i++) {
        var zone = zones[i];

        var circle = L.circle(
            zone.center,
            {
                radius: 90,
                color: THEME.danger,
                fillColor: THEME.danger,
                fillOpacity: 0.12,
                weight: 1
            }
        ).addTo(map);

        circle.bindPopup(
            "<b>" +
            zone.name +
            "</b><br>" +
            "Waiting for measurements"
        );

        zoneLayers.push(circle);
    }


    // DEFAULT VIT MARKER

    L.marker(defaultCenter)
        .addTo(map)
        .bindPopup(
            "<b>VIT Pune</b><br>" +
            "Campus WiFi"
        );
}


// ============================================================
// HEALTH SCORE
// ============================================================

function scoreFor(throughput, latency, loss) {
    throughput = Number(throughput) || 0;
    latency = Number(latency) || 0;
    loss = Number(loss) || 0;

    var throughputScore = Math.min(
        100,
        (throughput / 500) * 100
    );

    var latencyScore = Math.max(
        0,
        100 - (latency / 2)
    );

    var reliabilityScore = Math.max(
        0,
        100 - (loss * 10)
    );

    var score =
        (throughputScore * 0.50) +
        (latencyScore * 0.30) +
        (reliabilityScore * 0.20);

    return Math.max(
        0,
        Math.min(100, score)
    );
}


// ============================================================
// HEALTH LABEL
// ============================================================

function healthLabel(score) {
    if (score >= 75) {
        return "Healthy";
    }

    if (score >= 45) {
        return "Moderate";
    }

    return "Poor";
}


// ============================================================
// DEAD ZONE CHECK
// ============================================================

function isDeadZone(score) {
    return score < 45;
}


// ============================================================
// GPS LOCATION
// ============================================================

function getLocation() {
    if (!navigator.geolocation) {
        setText("locationLabel", "GPS not supported");
        setText(
            "locationSub",
            "Your browser does not support GPS."
        );
        return;
    }

    setText("locationLabel", "Requesting location...");
    setText(
        "locationSub",
        "Please allow location access."
    );

    navigator.geolocation.getCurrentPosition(
        function(position) {
            var lat = position.coords.latitude;
            var lng = position.coords.longitude;

            currentLocation = {
                latitude: lat,
                longitude: lng
            };

            window.currentLocation = currentLocation;

            setText(
                "locationLabel",
                "GPS location selected"
            );

            setText(
                "locationSub",
                lat.toFixed(6) + ", " + lng.toFixed(6)
            );

            if (map) {
                map.setView([lat, lng], 18);

                L.marker([lat, lng])
                    .addTo(map)
                    .bindPopup(
                        "<b>Your current location</b><br>" +
                        lat.toFixed(6) +
                        ", " +
                        lng.toFixed(6)
                    )
                    .openPopup();
            }
        },

        function(error) {
            console.error("GPS error:", error);

            setText(
                "locationLabel",
                "Unable to get GPS location"
            );

            setText(
                "locationSub",
                "Allow location access and try again."
            );
        }
    );
}


// ============================================================
// LOCATION SEARCH
// SEARCH IS ONLY FOR MAP NAVIGATION.
// IT IS NOT USED FOR WIFI TEST COORDINATES.
// ============================================================

async function searchLocation() {
    if (!map) {
        console.error("Map is not available.");
        return;
    }

    var input = getElement("locationSearch");
    var message = getElement("searchMessage");

    if (!input) {
        return;
    }

    var query = input.value.trim();

    if (query === "") {
        if (message) {
            message.textContent = "Please enter a location.";
            message.style.color = THEME.danger;
        }
        return;
    }

    if (message) {
        message.textContent = "Searching...";
        message.style.color = THEME.muted;
    }

    try {
        var url =
            "https://nominatim.openstreetmap.org/search" +
            "?format=json" +
            "&limit=5" +
            "&q=" +
            encodeURIComponent(query);

        var response = await fetch(url);

        if (!response.ok) {
            throw new Error("Location search failed.");
        }

        var results = await response.json();

        if (!results || results.length === 0) {
            if (message) {
                message.textContent = "Location not found.";
                message.style.color = THEME.danger;
            }
            return;
        }

        var place = results[0];

        var lat = parseFloat(place.lat);
        var lng = parseFloat(place.lon);

        if (
            !Number.isFinite(lat) ||
            !Number.isFinite(lng) ||
            lat < -90 ||
            lat > 90 ||
            lng < -180 ||
            lng > 180
        ) {
            throw new Error("Invalid coordinates.");
        }

        map.setView([lat, lng], 17);

        searchedLocation = {
            name: place.display_name,
            latitude: lat,
            longitude: lng
        };

        window.searchedLocation = searchedLocation;

        if (searchMarker) {
            map.removeLayer(searchMarker);
        }

        searchMarker = L.marker([lat, lng]).addTo(map);

        searchMarker.bindPopup(
            "<b>" +
            place.display_name +
            "</b><br>" +
            "Latitude: " +
            lat.toFixed(6) +
            "<br>" +
            "Longitude: " +
            lng.toFixed(6) +
            "<br><br>" +
            "<span style='color:" + THEME.danger + ";'>" +
            "Search result only - not used for WiFi testing." +
            "</span>"
        );

        searchMarker.openPopup();

        if (message) {
            message.textContent = "Showing: " + place.display_name;
            message.style.color = THEME.success;
        }

    } catch (error) {
        console.error("Location search error:", error);

        if (message) {
            message.textContent = "Could not search location.";
            message.style.color = THEME.danger;
        }
    }
}


// ============================================================
// LOCATION SEARCH - ENTER KEY
// ============================================================

function setupSearch() {
    var input = getElement("locationSearch");

    if (!input) {
        return;
    }

    input.addEventListener(
        "keydown",
        function(event) {
            if (event.key === "Enter") {
                event.preventDefault();
                searchLocation();
            }
        }
    );
}


// ============================================================
// LATENCY TEST
// ============================================================

// Jitter (mean absolute difference between consecutive RTT samples)
// of the most recent latency test, in milliseconds.
var lastJitter = 0;

async function latencyTest() {
    var samples = [];
    lastJitter = 0;

    for (var i = 0; i < 10; i++) {
        var start = performance.now();

        try {
            var response = await fetch(
                "/api/ping?x=" + Date.now(),
                {
                    cache: "no-store"
                }
            );

            if (!response.ok) {
                samples.push(999);
                continue;
            }

            var end = performance.now();

            samples.push(end - start);

        } catch (error) {
            console.error("Latency request failed:", error);
            samples.push(999);
        }
    }

    if (samples.length === 0) {
        return 999;
    }

    var total = 0;

    for (var j = 0; j < samples.length; j++) {
        total += samples[j];
    }

    // Jitter from the successful samples only (999 marks a failure).
    var good = samples.filter(function (v) { return v < 999; });
    var diffTotal = 0;

    for (var k = 1; k < good.length; k++) {
        diffTotal += Math.abs(good[k] - good[k - 1]);
    }

    lastJitter = good.length > 1 ? diffTotal / (good.length - 1) : 0;

    return total / samples.length;
}


// ============================================================
// THROUGHPUT TEST
// ============================================================

async function throughputTest() {
    var start = performance.now();

    try {
        var response = await fetch(
            "/api/throughput?x=" + Date.now(),
            {
                cache: "no-store"
            }
        );

        if (!response.ok) {
            throw new Error("Throughput request failed.");
        }

        var data = await response.arrayBuffer();

        var end = performance.now();

        var durationMs = end - start;
        var duration = durationMs / 1000;
        var bytes = data.byteLength;

        if (duration <= 0) {
            return {
                mbps: 0,
                bytes: bytes,
                durationMs: durationMs
            };
        }

        var mbps =
            (bytes * 8) /
            duration /
            1000000;

        return {
            mbps: mbps,
            bytes: bytes,
            durationMs: durationMs
        };

    } catch (error) {
        console.error("Throughput test failed:", error);

        return {
            mbps: 0,
            bytes: 0,
            durationMs: 0
        };
    }
}


// ============================================================
// RUN WIFI TEST
// ============================================================

async function runTest() {
    var button = getElement("runBtn");
    var progress = getElement("progress");
    var result = getElement("result");
    var locationInput = getElement("manualLocation");
    var device = getElement("device");

    if (!button) {
        console.error("Run button not found.");
        return;
    }


    // GPS IS REQUIRED

    var latitude = null;
    var longitude = null;

    if (
        currentLocation &&
        typeof currentLocation.latitude === "number" &&
        typeof currentLocation.longitude === "number"
    ) {
        latitude = currentLocation.latitude;
        longitude = currentLocation.longitude;

    } else if (
        window.currentLocation &&
        typeof window.currentLocation.latitude === "number" &&
        typeof window.currentLocation.longitude === "number"
    ) {
        latitude = window.currentLocation.latitude;
        longitude = window.currentLocation.longitude;
    }


    // DO NOT USE SEARCHED LOCATION

    if (
        latitude === null ||
        longitude === null
    ) {
        if (result) {
            result.innerHTML =
                "<p>Please select your GPS location first.</p>";

            result.classList.remove("hidden");
        }

        setText("locationLabel", "Location required");

        setText(
            "locationSub",
            "Click Use GPS before running the test."
        );

        return;
    }


    // DISABLE BUTTON

    button.disabled = true;

    if (result) {
        result.classList.add("hidden");
    }

    if (progress) {
        progress.classList.remove("hidden");
    }

    setProgress(5);


    // LOCATION NAME

    var locationName = "";

    if (locationInput) {
        locationName = locationInput.value.trim();
    }

    if (locationName === "") {
        locationName = "GPS Test Location";
    }


    // DEVICE

    var deviceName = "Unknown";

    if (device) {
        if (device.value) {
            deviceName = device.value;
        } else if (device.textContent) {
            deviceName = device.textContent;
        }
    }


    // LATENCY

    setProgress(20);

    var latency = await latencyTest();

    setProgress(55);


    // THROUGHPUT

    var throughputResult = await throughputTest();

    var throughput = throughputResult.mbps;
    var bytes = throughputResult.bytes;
    var durationMs = throughputResult.durationMs;

    setProgress(80);


    // LOSS / DEGRADATION PROXY

    var loss = 1.5;

    if (latency > 200) {
        loss = (latency - 200) / 50;
    }

    loss = Math.min(
        10,
        Math.max(0, loss)
    );


    // HEALTH SCORE

    var score = scoreFor(
        throughput,
        latency,
        loss
    );


    // SAVE RESULT

    var payload = {
        timestamp: new Date().toISOString(),
        location: locationName,
        device: deviceName,
        lat: latitude,
        lng: longitude,
        throughput: throughput,
        latency: latency,
        loss: loss,
        jitter: lastJitter,
        bytes: bytes,
        duration_ms: durationMs
    };

    console.log("Sending test result:", payload);

    try {
        var saveResponse = await fetch(
            "/api/result",
            {
                method: "POST",

                headers: {
                    "Content-Type": "application/json"
                },

                body: JSON.stringify(payload)
            }
        );

        if (!saveResponse.ok) {
            console.error(
                "Server rejected result:",
                saveResponse.status
            );
        }

    } catch (error) {
        console.error("Could not save result:", error);
    }


    // SHOW RESULT

    setProgress(100);

    if (result) {
        var resultStatus = healthLabel(score);
        var warning = "";

        if (isDeadZone(score)) {
            warning =
                "<p style='color:" + THEME.danger + ";font-weight:bold;'>" +
                "WARNING: POTENTIAL WIFI DEAD ZONE DETECTED" +
                "</p>";

        } else if (score < 75) {
            warning =
                "<p style='color:" + THEME.warning + ";font-weight:bold;'>" +
                "WARNING: MODERATE WIFI QUALITY" +
                "</p>";

        } else {
            warning =
                "<p style='color:" + THEME.success + ";font-weight:bold;'>" +
                "HEALTHY WIFI ZONE" +
                "</p>";
        }

        result.innerHTML =
            warning +

            "<div class=\"result-grid\">" +

            "<div>" +
            "<small>THROUGHPUT</small>" +
            "<strong>" +
            throughput.toFixed(2) +
            " Mbps" +
            "</strong>" +
            "</div>" +

            "<div>" +
            "<small>LATENCY</small>" +
            "<strong>" +
            latency.toFixed(2) +
            " ms" +
            "</strong>" +
            "</div>" +

            "<div>" +
            "<small>DEGRADATION</small>" +
            "<strong>" +
            loss.toFixed(2) +
            " %" +
            "</strong>" +
            "</div>" +

            "<div>" +
            "<small>HEALTH</small>" +
            "<strong>" +
            score.toFixed(0) +
            " / 100" +
            "</strong>" +
            "</div>" +

            "</div>" +

            "<p>" +
            "Test completed at: " +
            "<b>" +
            locationName +
            "</b>" +
            "</p>" +

            "<p>" +
            "GPS Coordinates: " +
            latitude.toFixed(6) +
            ", " +
            longitude.toFixed(6) +
            "</p>" +

            "<p>" +
            "Status: <b>" +
            resultStatus +
            "</b>" +
            "</p>";

        result.classList.remove("hidden");
    }

    button.disabled = false;

    setTimeout(
        function() {
            if (progress) {
                progress.classList.add("hidden");
            }
        },
        800
    );


    // REFRESH DASHBOARD + HEATMAP

    await refresh();
}


// ============================================================
// REFRESH DATA
// ============================================================

async function refresh() {
    try {
        var response = await fetch(
            "/api/results?x=" + Date.now(),
            {
                cache: "no-store"
            }
        );

        if (!response.ok) {
            throw new Error("Could not load results.");
        }

        var data = await response.json();

        if (!Array.isArray(data)) {
            console.error("Invalid results received:", data);
            return;
        }


        // DASHBOARD TOTAL

        var total = getElement("tests");

        if (total) {
            total.textContent = data.length;
        }


        // DASHBOARD AVERAGES

        var avgThroughput = getElement("avgThroughput");
        var avgLatency = getElement("avgLatency");
        var avgLoss = getElement("avgLoss");

        var throughputTotal = 0;
        var throughputCount = 0;

        var latencyTotal = 0;
        var latencyCount = 0;

        var lossTotal = 0;
        var lossCount = 0;

        for (var i = 0; i < data.length; i++) {
            var throughput = validMeasurement(data[i].throughput);
            var latency = validMeasurement(data[i].latency);
            var loss = validMeasurement(data[i].loss);

            if (
                throughput !== null &&
                throughput > 0
            ) {
                throughputTotal += throughput;
                throughputCount++;
            }

            if (latency !== null) {
                latencyTotal += latency;
                latencyCount++;
            }

            if (loss !== null) {
                lossTotal += loss;
                lossCount++;
            }
        }

        if (avgThroughput) {
            avgThroughput.textContent =
                throughputCount > 0
                    ? (throughputTotal / throughputCount).toFixed(1)
                    : "0";
        }

        if (avgLatency) {
            avgLatency.textContent =
                latencyCount > 0
                    ? (latencyTotal / latencyCount).toFixed(1)
                    : "0";
        }

        if (avgLoss) {
            avgLoss.textContent =
                lossCount > 0
                    ? (lossTotal / lossCount).toFixed(1)
                    : "0";
        }


        // RECENT RESULTS TABLE

        var rows = getElement("recentRows");

        if (rows) {
            rows.innerHTML = "";

            var recent = data.slice()
                .reverse()
                .slice(0, 10);

            for (var r = 0; r < recent.length; r++) {
                var item = recent[r];

                var tr = document.createElement("tr");

                var timeText = "-";

                if (item.timestamp) {
                    try {
                        timeText = new Date(
                            item.timestamp
                        ).toLocaleTimeString();

                    } catch (e) {
                        timeText = item.timestamp;
                    }
                }

                var itemThroughput =
                    validMeasurement(item.throughput);

                var itemLatency =
                    validMeasurement(item.latency);

                var itemLoss =
                    validMeasurement(item.loss);

                var itemJitter =
                    validMeasurement(item.jitter);

                var throughputText =
                    itemThroughput !== null
                        ? itemThroughput.toFixed(1) + " Mbps"
                        : "-";

                var latencyText =
                    itemLatency !== null
                        ? itemLatency.toFixed(1) + " ms"
                        : "-";

                var jitterText =
                    itemJitter !== null
                        ? itemJitter.toFixed(3) + " ms"
                        : "-";

                var lossText =
                    itemLoss !== null
                        ? itemLoss.toFixed(1) + "%"
                        : "-";

                var timeCell = document.createElement("td");
                timeCell.textContent = timeText;

                var locationCell = document.createElement("td");
                locationCell.textContent = item.location || "-";

                var throughputCell = document.createElement("td");
                throughputCell.textContent = throughputText;

                var latencyCell = document.createElement("td");
                latencyCell.textContent = latencyText;

                var jitterLine = document.createElement("small");
                jitterLine.textContent = "Jitter: " + jitterText;
                jitterLine.style.display = "block";
                jitterLine.style.opacity = "0.75";

                latencyCell.appendChild(
                    document.createElement("br")
                );

                latencyCell.appendChild(jitterLine);

                var lossCell = document.createElement("td");
                lossCell.textContent = lossText;

                var deviceCell = document.createElement("td");
                deviceCell.textContent = item.device || "-";

                tr.appendChild(timeCell);
                tr.appendChild(locationCell);
                tr.appendChild(throughputCell);
                tr.appendChild(latencyCell);
                tr.appendChild(lossCell);
                tr.appendChild(deviceCell);

                rows.appendChild(tr);
            }
        }


        // MAP MEASUREMENTS

        if (
            map &&
            typeof L !== "undefined"
        ) {

            // REMOVE OLD MEASUREMENT MARKERS

            for (
                var m = 0;
                m < measurementLayers.length;
                m++
            ) {
                map.removeLayer(measurementLayers[m]);
            }

            measurementLayers = [];


            // GROUP NEARBY TESTS

            var groups = [];

            var GROUP_DISTANCE = 20;


            // DISTANCE FUNCTION

            function distanceInMeters(
                lat1,
                lng1,
                lat2,
                lng2
            ) {
                var R = 6371000;

                var dLat =
                    (lat2 - lat1) *
                    Math.PI / 180;

                var dLng =
                    (lng2 - lng1) *
                    Math.PI / 180;

                var a =
                    Math.sin(dLat / 2) *
                    Math.sin(dLat / 2) +

                    Math.cos(lat1 * Math.PI / 180) *
                    Math.cos(lat2 * Math.PI / 180) *

                    Math.sin(dLng / 2) *
                    Math.sin(dLng / 2);

                var c =
                    2 *
                    Math.atan2(
                        Math.sqrt(a),
                        Math.sqrt(1 - a)
                    );

                return R * c;
            }


            // CREATE LOCATION GROUPS

            for (var d = 0; d < data.length; d++) {
                var itemData = data[d];

                var lat = validCoordinate(
                    itemData.lat,
                    -90,
                    90
                );

                var lng = validCoordinate(
                    itemData.lng,
                    -180,
                    180
                );

                if (lat === null || lng === null) {
                    continue;
                }

                var addedToGroup = false;

                for (var g = 0; g < groups.length; g++) {
                    var group = groups[g];

                    var distance = distanceInMeters(
                        lat,
                        lng,
                        group.lat,
                        group.lng
                    );

                    if (distance <= GROUP_DISTANCE) {
                        group.tests.push(itemData);
                        addedToGroup = true;
                        break;
                    }
                }

                if (!addedToGroup) {
                    groups.push({
                        lat: lat,
                        lng: lng,
                        tests: [itemData]
                    });
                }
            }


            // CREATE ONE MARKER PER GROUP

            for (var g = 0; g < groups.length; g++) {
                var group = groups[g];
                var tests = group.tests;

                var totalThroughput = 0;
                var throughputCount = 0;

                var totalLatency = 0;
                var latencyCount = 0;

                var totalLoss = 0;
                var lossCount = 0;

                var totalLat = 0;
                var totalLng = 0;


                // CALCULATE VALID AVERAGES

                for (var t = 0; t < tests.length; t++) {
                    var testThroughput =
                        validMeasurement(tests[t].throughput);

                    var testLatency =
                        validMeasurement(tests[t].latency);

                    var testLoss =
                        validMeasurement(tests[t].loss);

                    if (
                        testThroughput !== null &&
                        testThroughput > 0
                    ) {
                        totalThroughput += testThroughput;
                        throughputCount++;
                    }

                    if (testLatency !== null) {
                        totalLatency += testLatency;
                        latencyCount++;
                    }

                    if (testLoss !== null) {
                        totalLoss += testLoss;
                        lossCount++;
                    }

                    totalLat += Number(tests[t].lat);
                    totalLng += Number(tests[t].lng);
                }


                var averageThroughput =
                    throughputCount > 0
                        ? totalThroughput / throughputCount
                        : 0;

                var averageLatency =
                    latencyCount > 0
                        ? totalLatency / latencyCount
                        : 0;

                var averageLoss =
                    lossCount > 0
                        ? totalLoss / lossCount
                        : 0;

                var averageLat =
                    totalLat / tests.length;

                var averageLng =
                    totalLng / tests.length;


                // GROUP HEALTH SCORE

                var groupScore = scoreFor(
                    averageThroughput,
                    averageLatency,
                    averageLoss
                );


                // HEALTH STATUS

                var groupHealth = healthLabel(groupScore);
                var deadZone = isDeadZone(groupScore);


                // MARKER COLOR

                var markerColor = THEME.danger;

                if (groupScore >= 75) {
                    markerColor = THEME.success;

                } else if (groupScore >= 45) {
                    markerColor = THEME.warning;
                }


                // CREATE MARKER

                var marker = L.circleMarker(
                    [
                        averageLat,
                        averageLng
                    ],
                    {
                        radius: deadZone ? 14 : 11,
                        color: markerColor,
                        fillColor: markerColor,
                        fillOpacity: 0.85,
                        weight: deadZone ? 3 : 2
                    }
                ).addTo(map);


                // LOCATION NAME

                var locationName =
                    tests[0].location ||
                    "WiFi Test Location";


                // DEAD ZONE MESSAGE

                var statusMessage = "";

                if (deadZone) {
                    statusMessage =
                        "<div style='" +
                        "color:" + THEME.danger + ";" +
                        "font-weight:bold;" +
                        "font-size:15px;" +
                        "margin-bottom:8px;" +
                        "'>" +
                        "WARNING: POTENTIAL WIFI DEAD ZONE" +
                        "</div>";

                } else if (groupScore < 75) {
                    statusMessage =
                        "<div style='" +
                        "color:" + THEME.warning + ";" +
                        "font-weight:bold;" +
                        "margin-bottom:8px;" +
                        "'>" +
                        "WARNING: MODERATE WIFI QUALITY" +
                        "</div>";

                } else {
                    statusMessage =
                        "<div style='" +
                        "color:" + THEME.success + ";" +
                        "font-weight:bold;" +
                        "margin-bottom:8px;" +
                        "'>" +
                        "HEALTHY WIFI ZONE" +
                        "</div>";
                }


                // POPUP

                var popup =
                    "<b>" +
                    locationName +
                    "</b><br><br>" +

                    statusMessage +

                    "<b>Tests at this location:</b> " +
                    tests.length +
                    "<br>" +

                    "<b>Health:</b> " +
                    groupHealth +
                    "<br><br>" +

                    "Average Throughput: " +
                    averageThroughput.toFixed(1) +
                    " Mbps<br>" +

                    "Average Latency: " +
                    averageLatency.toFixed(1) +
                    " ms<br>" +

                    "Average Degradation: " +
                    averageLoss.toFixed(1) +
                    "%<br>" +

                    "Average Health: " +
                    groupScore.toFixed(0) +
                    "/100<br><br>" +

                    "Coordinates: " +
                    averageLat.toFixed(6) +
                    ", " +
                    averageLng.toFixed(6);

                marker.bindPopup(popup);

                measurementLayers.push(marker);
            }
        }

    } catch (error) {
        console.error("Refresh failed:", error);
    }
}


// ============================================================
// SMOOTH SCROLL REVEAL
// ============================================================

function setupReveals() {
    try {
        if (
            window.matchMedia &&
            window.matchMedia(
                "(prefers-reduced-motion: reduce)"
            ).matches
        ) {
            return;
        }

        if (typeof IntersectionObserver === "undefined") {
            return;
        }

        var targets = document.querySelectorAll(
            ".hero > div, " +
            ".health-strip, " +
            ".stats > div, " +
            ".panel, " +
            ".map-section, " +
            ".recent, " +
            ".section-title"
        );

        for (var i = 0; i < targets.length; i++) {
            var element = targets[i];

            if (element.classList.contains("reveal")) {
                continue;
            }

            element.classList.add("reveal");

            element.style.setProperty(
                "--reveal-delay",
                (i % 4) * 70 + "ms"
            );
        }

        var reveals = document.querySelectorAll(".reveal");

        if (reveals.length === 0) {
            return;
        }

        var observer = new IntersectionObserver(
            function(entries) {
                for (var e = 0; e < entries.length; e++) {
                    var entry = entries[e];

                    if (entry.isIntersecting) {
                        entry.target.classList.add("visible");
                        observer.unobserve(entry.target);
                    }
                }
            },
            {
                threshold: 0.12,
                rootMargin: "0px 0px -8% 0px"
            }
        );

        for (var r = 0; r < reveals.length; r++) {
            observer.observe(reveals[r]);
        }

    } catch (error) {
        console.error("Reveal setup failed:", error);
    }
}


// ============================================================
// WEBSPHERE BACKDROP
// ============================================================

function setupWebsphere() {
    try {
        if (
            window.matchMedia &&
            window.matchMedia(
                "(prefers-reduced-motion: reduce)"
            ).matches
        ) {
            return;
        }

        var canvas = document.getElementById("websphere-bg");

        if (!canvas) {
            canvas = document.createElement("canvas");
            canvas.id = "websphere-bg";
            canvas.setAttribute("aria-hidden", "true");

            var hero = document.querySelector(".hero");

            if (hero) {
                hero.appendChild(canvas);
            } else {
                document.body.prepend(canvas);
            }
        }

        var context = canvas.getContext("2d");

        if (!context) {
            return;
        }

        var NODE_COUNT = 90;
        var LINK_DISTANCE = 0.85;
        var ROTATION_SPEED = 0.0016;
        var TILT = 0.35;

        var nodes = [];
        var links = [];

        var goldenAngle =
            Math.PI * (3 - Math.sqrt(5));

        for (var i = 0; i < NODE_COUNT; i++) {
            var y =
                1 -
                (i / (NODE_COUNT - 1)) * 2;

            var radius = Math.sqrt(
                Math.max(0, 1 - y * y)
            );

            var theta = goldenAngle * i;

            nodes.push({
                x: Math.cos(theta) * radius,
                y: y,
                z: Math.sin(theta) * radius
            });
        }

        for (var a = 0; a < nodes.length; a++) {
            for (var b = a + 1; b < nodes.length; b++) {
                var dx = nodes[a].x - nodes[b].x;
                var dy = nodes[a].y - nodes[b].y;
                var dz = nodes[a].z - nodes[b].z;

                var distance = Math.sqrt(
                    dx * dx + dy * dy + dz * dz
                );

                if (distance < LINK_DISTANCE) {
                    links.push([a, b]);
                }
            }
        }

        var angle = 0;
        var running = true;

        function resize() {
            var size = canvas.clientWidth || 440;

            var dpr = Math.min(
                window.devicePixelRatio || 1,
                1.5
            );

            canvas.width = size * dpr;
            canvas.height = size * dpr;

            context.setTransform(
                dpr,
                0,
                0,
                dpr,
                0,
                0
            );
        }

        resize();

        window.addEventListener("resize", resize);

        document.addEventListener(
            "visibilitychange",
            function() {
                running = !document.hidden;
            }
        );

        function frame() {
            requestAnimationFrame(frame);

            if (!running) {
                return;
            }

            var size = canvas.clientWidth || 440;
            var center = size / 2;
            var sphereRadius = size * 0.36;

            angle += ROTATION_SPEED;

            var cosA = Math.cos(angle);
            var sinA = Math.sin(angle);
            var cosT = Math.cos(TILT);
            var sinT = Math.sin(TILT);

            context.clearRect(0, 0, size, size);

            var projected = [];

            for (var n = 0; n < nodes.length; n++) {
                var node = nodes[n];

                var x1 =
                    node.x * cosA -
                    node.z * sinA;

                var z1 =
                    node.x * sinA +
                    node.z * cosA;

                var y2 =
                    node.y * cosT -
                    z1 * sinT;

                var z2 =
                    node.y * sinT +
                    z1 * cosT;

                var perspective =
                    2.6 / (2.6 - z2 * 0.7);

                projected.push({
                    x:
                        center +
                        x1 * sphereRadius * perspective,

                    y:
                        center +
                        y2 * sphereRadius * perspective,

                    depth: (z2 + 1) / 2
                });
            }

            context.lineWidth = 1;
            context.strokeStyle = THEME.primary;
            context.globalAlpha = 0.28;
            context.beginPath();

            for (var l = 0; l < links.length; l++) {
                var p1 = projected[links[l][0]];
                var p2 = projected[links[l][1]];

                context.moveTo(p1.x, p1.y);
                context.lineTo(p2.x, p2.y);
            }

            context.stroke();
            context.globalAlpha = 1;

            for (var m = 0; m < projected.length; m++) {
                var point = projected[m];

                var dotRadius =
                    1 + point.depth * 1.8;

                context.beginPath();

                context.fillStyle =
                    point.depth > 0.55
                        ? THEME.dark
                        : THEME.primary;

                context.arc(
                    point.x,
                    point.y,
                    dotRadius,
                    0,
                    Math.PI * 2
                );

                context.fill();
            }
        }

        frame();

    } catch (error) {
        console.error("Websphere setup failed:", error);
    }
}


// ============================================================
// INITIALIZATION
// ============================================================

function initializeApp() {
    console.log("Campus WiFi JavaScript loaded.");

    setupReveals();
    setupWebsphere();
    initializeMap();
    setupSearch();
    refresh();
}


// ============================================================
// START APPLICATION
// ============================================================

if (document.readyState === "loading") {
    document.addEventListener(
        "DOMContentLoaded",
        initializeApp
    );

} else {
    initializeApp();
}
