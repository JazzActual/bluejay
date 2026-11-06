// Parses BlueJay exports (CSV, GeoJSON, or pasted serial output) into a common
// list of sightings. Has no DOM dependency so it can be tested on its own.
(function (root) {
  "use strict";

  function toNumber(value) {
    if (value === undefined || value === null) return null;
    const text = String(value).trim();
    if (text === "" || text.toLowerCase() === "nan") return null;
    const n = Number(text);
    return Number.isFinite(n) ? n : null;
  }

  function validPosition(lat, lon) {
    return lat !== null && lon !== null &&
      lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180;
  }

  // Serial output wraps the data in -----BEGIN X----- / -----END X----- lines.
  function stripSerialMarkers(text) {
    const match = text.match(/-----BEGIN [A-Z]+-----\r?\n([\s\S]*?)\r?\n?-----END [A-Z]+-----/);
    return match ? match[1] : text;
  }

  function parseCsv(text) {
    const lines = text.split(/\r?\n/).filter((line) => line.trim() !== "");
    if (lines.length === 0) throw new Error("The file is empty.");

    const columns = lines[0].split(",").map((c) => c.trim().toLowerCase());
    const col = (name) => columns.indexOf(name);
    if (col("lat") === -1 || col("lon") === -1) {
      throw new Error("This CSV has no lat and lon columns, so it doesn't look like a BlueJay export.");
    }

    const sightings = [];
    let skipped = 0;
    for (const line of lines.slice(1)) {
      const cells = line.split(",");
      const lat = toNumber(cells[col("lat")]);
      const lon = toNumber(cells[col("lon")]);
      if (!validPosition(lat, lon)) {
        skipped++;
        continue;
      }
      sightings.push({
        id: toNumber(cells[col("id")]),
        utc: col("utc") === -1 ? null : (cells[col("utc")] || "").trim() || null,
        lat,
        lon,
        alt_m: toNumber(cells[col("alt_m")]),
        hdop: toNumber(cells[col("hdop")]),
        sats: toNumber(cells[col("sats")]),
      });
    }
    return { sightings, skipped };
  }

  function parseGeoJson(text) {
    let data;
    try {
      data = JSON.parse(text);
    } catch (err) {
      throw new Error("This file isn't valid JSON, so it may be incomplete.");
    }
    if (!data || data.type !== "FeatureCollection" || !Array.isArray(data.features)) {
      throw new Error("This JSON isn't a GeoJSON FeatureCollection, so it doesn't look like a BlueJay export.");
    }

    const sightings = [];
    let skipped = 0;
    for (const feature of data.features) {
      const geometry = feature && feature.geometry;
      const coords = geometry && geometry.type === "Point" ? geometry.coordinates : null;
      // GeoJSON stores longitude first.
      const lon = coords ? toNumber(coords[0]) : null;
      const lat = coords ? toNumber(coords[1]) : null;
      if (!validPosition(lat, lon)) {
        skipped++;
        continue;
      }
      const props = feature.properties || {};
      sightings.push({
        id: toNumber(props.id),
        utc: typeof props.utc === "string" ? props.utc : null,
        lat,
        lon,
        alt_m: toNumber(props.alt_m),
        hdop: toNumber(props.hdop),
        sats: toNumber(props.sats),
      });
    }
    return { sightings, skipped };
  }

  function parseExport(rawText) {
    const text = stripSerialMarkers(String(rawText).replace(/^﻿/, "")).trim();
    if (text === "") throw new Error("The file is empty.");
    return text.startsWith("{") ? parseGeoJson(text) : parseCsv(text);
  }

  const api = { parseExport };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else root.BlueJayParse = api;
})(typeof window !== "undefined" ? window : globalThis);
