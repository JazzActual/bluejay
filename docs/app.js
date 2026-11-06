(function () {
  "use strict";

  // Fictional points for the "sample data" link, in the CSV format the device writes.
  const SAMPLE_CSV = [
    "id,utc,lat,lon,alt_m,hdop,sats,type",
    "1,2026-03-14T15:04:11Z,39.7392000,-104.9903000,1609.3,0.80,12,flock",
    "2,2026-03-14T15:21:47Z,39.7508000,-104.9966000,1594.7,1.60,9,vigilant",
    "3,2026-03-14T15:38:02Z,39.7617000,-105.0118000,nan,3.40,6,",
    "4,2026-03-15T16:12:55Z,39.7294000,-104.9719000,1621.2,0.90,11,flock",
    "5,2026-03-15T16:40:30Z,39.7178000,-105.0044000,1602.8,7.20,4,axon",
  ].join("\n");

  // Matches kSightingTypes in src/sighting.h. Other values are shown as they are.
  const TYPE_LABELS = {
    flock: "Flock",
    vigilant: "Vigilant",
    genetec: "Genetec",
    axon: "Axon",
    alpr: "Other ALPR",
    unknown: "Unknown",
  };
  const typeLabel = (type) => (type === null ? "No tag" : TYPE_LABELS[type] || type);

  const el = (id) => document.getElementById(id);
  const fileInput = el("file");
  const drop = el("drop");
  const message = el("message");
  const results = el("results");
  const list = el("list");

  const map = L.map("map", { worldCopyJump: true }).setView([20, 0], 2);
  L.tileLayer("https://tile.openstreetmap.org/{z}/{x}/{y}.png", {
    maxZoom: 19,
    attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors',
  }).addTo(map);
  const markerLayer = L.featureGroup().addTo(map);

  // HDOP is the GPS accuracy estimate, where lower is better.
  function quality(hdop) {
    if (hdop === null) return { key: "unknown", label: "Unknown", color: "#6b7683" };
    if (hdop <= 2) return { key: "good", label: "Good", color: "#1a7f4b" };
    if (hdop <= 5) return { key: "fair", label: "Fair", color: "#c77700" };
    return { key: "poor", label: "Poor", color: "#c4302b" };
  }

  function formatTime(utc) {
    if (!utc) return "Time unknown";
    const date = new Date(utc);
    if (Number.isNaN(date.getTime())) return utc;
    return date.toLocaleString(undefined, { dateStyle: "medium", timeStyle: "short" });
  }

  function title(s) {
    return s.id === null ? "Sighting" : "Sighting #" + s.id;
  }

  function showMessage(text, kind) {
    message.textContent = text;
    message.className = kind === "note" ? "message note" : "message";
    message.hidden = false;
  }

  function popupContent(s) {
    const q = quality(s.hdop);
    const rows = [
      ["Type", typeLabel(s.type)],
      ["Time", formatTime(s.utc)],
      ["UTC", s.utc || "Unknown"],
      ["Position", s.lat.toFixed(7) + ", " + s.lon.toFixed(7)],
      ["Altitude", s.alt_m === null ? "Unknown" : s.alt_m.toFixed(1) + " m"],
      ["Accuracy", s.hdop === null ? q.label : q.label + " (HDOP " + s.hdop.toFixed(2) + ")"],
      ["Satellites", s.sats === null ? "Unknown" : String(s.sats)],
    ];

    const wrap = document.createElement("div");
    const heading = document.createElement("p");
    heading.className = "popup-title";
    heading.textContent = title(s);
    wrap.appendChild(heading);

    const table = document.createElement("table");
    table.className = "popup-table";
    for (const [name, value] of rows) {
      const row = table.insertRow();
      const th = document.createElement("th");
      th.scope = "row";
      th.textContent = name;
      row.appendChild(th);
      row.insertCell().textContent = value;
    }
    wrap.appendChild(table);
    return wrap;
  }

  function dateRange(sightings) {
    const times = sightings
      .filter((s) => s.utc)
      .map((s) => new Date(s.utc).getTime())
      .filter((t) => !Number.isNaN(t));
    if (times.length === 0) return "";
    const format = (t) => new Date(t).toLocaleDateString(undefined, { dateStyle: "medium" });
    const first = format(Math.min(...times));
    const last = format(Math.max(...times));
    return first === last ? first : first + " to " + last;
  }

  function render(sightings) {
    markerLayer.clearLayers();
    list.textContent = "";

    // Newest first in the list. Sort a copy, as ids can restart after a log clear.
    const ordered = [...sightings].sort((a, b) => (b.utc || "").localeCompare(a.utc || ""));
    for (const s of ordered) {
      const q = quality(s.hdop);
      const marker = L.circleMarker([s.lat, s.lon], {
        radius: 8,
        color: "#ffffff",
        weight: 2,
        fillColor: q.color,
        fillOpacity: 0.95,
      }).bindPopup(popupContent(s));
      markerLayer.addLayer(marker);

      const button = document.createElement("button");
      button.type = "button";
      const dot = document.createElement("span");
      dot.className = "dot " + q.key;
      dot.title = q.label + " accuracy";
      const num = document.createElement("span");
      num.className = "num";
      num.textContent = s.id === null ? "–" : "#" + s.id;
      const when = document.createElement("span");
      when.className = "when";
      when.textContent = (s.type === null ? "" : typeLabel(s.type) + " · ") + formatTime(s.utc);
      button.append(dot, num, when);
      button.addEventListener("click", () => {
        map.setView([s.lat, s.lon], Math.max(map.getZoom(), 16));
        marker.openPopup();
      });
      const item = document.createElement("li");
      item.appendChild(button);
      list.appendChild(item);
    }

    el("summary").textContent = sightings.length === 1 ? "1 sighting" : sightings.length + " sightings";
    el("range").textContent = dateRange(sightings);
    results.hidden = false;
    map.invalidateSize();
    map.fitBounds(markerLayer.getBounds(), { padding: [40, 40], maxZoom: 16 });
  }

  function clearResults() {
    markerLayer.clearLayers();
    list.textContent = "";
    results.hidden = true;
    map.setView([20, 0], 2);
  }

  function load(text, sourceName) {
    message.hidden = true;
    let parsed;
    try {
      parsed = BlueJayParse.parseExport(text);
    } catch (err) {
      clearResults();
      showMessage("Couldn't read " + sourceName + ". " + err.message);
      return;
    }

    if (parsed.sightings.length === 0) {
      clearResults();
      showMessage(
        parsed.skipped > 0
          ? "None of the sightings in " + sourceName + " had a usable position."
          : sourceName + " has no sightings yet.",
        "note"
      );
      return;
    }

    render(parsed.sightings);
    if (parsed.skipped > 0) {
      const rows = parsed.skipped === 1 ? "1 row" : parsed.skipped + " rows";
      showMessage("Skipped " + rows + " in " + sourceName + " with no usable position.", "note");
    }
  }

  function readFile(file) {
    if (!file) return;
    const reader = new FileReader();
    reader.onload = () => load(reader.result, file.name);
    reader.onerror = () => showMessage("Couldn't open " + file.name + ". Please try again.");
    reader.readAsText(file);
  }

  fileInput.addEventListener("change", () => {
    readFile(fileInput.files[0]);
    fileInput.value = ""; // lets the same file be chosen again
  });

  for (const type of ["dragenter", "dragover"]) {
    document.addEventListener(type, (event) => {
      event.preventDefault();
      drop.classList.add("over");
    });
  }
  document.addEventListener("dragleave", (event) => {
    if (event.relatedTarget === null) drop.classList.remove("over");
  });
  document.addEventListener("drop", (event) => {
    event.preventDefault();
    drop.classList.remove("over");
    readFile(event.dataTransfer.files[0]);
  });

  el("sample").addEventListener("click", () => {
    load(SAMPLE_CSV, "the sample data");
    showMessage("Showing made-up sample data, not real sightings.", "note");
  });
})();
