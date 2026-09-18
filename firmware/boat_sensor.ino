<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8" />
<meta name="viewport" content="width=device-width,initial-scale=1" />
<title>WARTEQ Lake Bulletin Board (Heterogeneity Clustering)</title>

<link href="https://fonts.googleapis.com/css2?family=Poppins:wght@400;500;600&display=swap" rel="stylesheet">
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<script src="https://cdn.jsdelivr.net/npm/chartjs-plugin-zoom@1.3.1"></script>
<link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css" />
<script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>

<style>
*{margin:0;padding:0;box-sizing:border-box;font-family:"Poppins",sans-serif;}
body{background:linear-gradient(135deg,#c9e8ff,#e8fff5);min-height:100vh;display:flex;flex-direction:column;align-items:center;padding:20px;}
h1{color:#02475e;margin-bottom:20px;text-align:center;font-size:1.8rem;}
.board-container{width:100%;max-width:960px;background:rgba(255,255,255,0.6);backdrop-filter:blur(10px);border-radius:20px;box-shadow:0 8px 30px rgba(0,0,0,0.1);padding:20px;}
.controls{display:flex;flex-wrap:wrap;gap:10px;margin-bottom:15px;}
.controls input{flex:1 1 30%;padding:10px;border-radius:10px;border:none;background:rgba(255,255,255,0.9);box-shadow:0 2px 5px rgba(0,0,0,0.05);transition:0.3s;}
.controls input:focus{outline:none;box-shadow:0 0 0 2px #6cc4a1;}
#postBtn{flex:1 1 100%;background:linear-gradient(135deg,#6cc4a1,#3aa6b9);color:white;border:none;border-radius:10px;font-weight:600;padding:12px;cursor:pointer;transition:0.3s;}
#postBtn:hover{transform:translateY(-1px);}
.lake-card{background:rgba(255,255,255,0.92);border-radius:15px;padding:15px;margin-bottom:18px;box-shadow:0 3px 8px rgba(0,0,0,0.06);transition:0.3s;}
.lake-title{font-weight:600;color:#02475e;font-size:1rem;display:flex;justify-content:space-between;align-items:center;cursor:pointer;}
.toggle-btn{background:none;border:none;color:#3aa6b9;font-size:1.2rem;cursor:pointer;transition: transform 0.3s;}
.toggle-btn.open{transform:rotate(90deg);}
.chart-container{width:100%;min-height:300px;margin-top:10px;}
.chart-container canvas{width:100% !important;height:300px !important;display:block;}
.lake-map{width:100%;height:320px;margin-top:12px;border-radius:12px;overflow:hidden;box-shadow:0 3px 10px rgba(0,0,0,0.08);}
.map-controls{display:flex;justify-content:flex-end;gap:8px;margin-top:8px;}
.small-btn{padding:6px 10px;border-radius:8px;border:none;font-size:0.8rem;cursor:pointer;font-weight:500;background:#e6f5f9;color:#266d83;transition:0.2s;}
.small-btn:hover{background:#d4eff7;}
.record-btn{background:#fde6e4;color:#d84234;}
.record-btn.recording{background:#d84234;color:#fff;}
.llm-box{margin-top:12px;padding:10px 12px;border-radius:10px;background:#f7fbff;border:1px solid #dbe8ff;font-size:0.85rem;color:#234;}
.legend{margin-top:12px;background:white;padding:10px 14px;border-radius:12px;font-size:0.85rem;box-shadow:0 2px 6px rgba(0,0,0,0.1);line-height:1.6;}
.mini-note{margin-top:8px;font-size:0.82rem;color:#345;}
@media (max-width:800px){.controls input{flex:1 1 100%;}.lake-map{height:260px;}.chart-container canvas{height:200px !important;}}
</style>
</head>

<body>
<h1>🌊 WARTEQ Lake Bulletin Board</h1>

<div class="board-container">
  <div class="controls">
    <input id="lakeNameInput" type="text" placeholder="Lake name (e.g., Lake Toba)">
    <input id="uidInput" type="text" placeholder="Firebase UID (optional)">
    <input id="statusInput" type="text" placeholder="Initial note (optional)">
    <button id="postBtn">Add Lake Card</button>
  </div>

  <div id="bulletinList"></div>

  <p class="mini-note">
    Tip: paste an existing Firebase node UID to link a card to its IoT data; otherwise leave blank to create a manual card.
  </p>
</div>

<!-- Firebase -->
<script src="https://www.gstatic.com/firebasejs/8.10.0/firebase-app.js"></script>
<script src="https://www.gstatic.com/firebasejs/8.10.0/firebase-database.js"></script>

<script>
// ==================== Firebase ====================
const firebaseConfig = {
  apiKey: "AIzaSyDSz1j8DTLDVTHcIgpKHnafDp_W_gWiXcQ",
  authDomain: "warteq-9eb97.firebaseapp.com",
  databaseURL: "https://warteq-9eb97-default-rtdb.asia-southeast1.firebasedatabase.app",
  projectId: "warteq-9eb97",
  storageBucket: "warteq-9eb97.appspot.com",
  messagingSenderId: "649486578429",
  appId: "1:649486578429:web:eac16b9e596d4b5a934494"
};
firebase.initializeApp(firebaseConfig);
const db = firebase.database();

// ==================== Globals ====================
const activeCards = {};
const recordIntervals = {};

// ================== PRETRAINED KMEANS MODEL (6 FEATURES) ==================
// Feature order MUST match training:
// [pH, Temp, Turb, TDS, log1p(Turb), log1p(TDS)]
const KMEANS_MODEL = {
  "mean": [7.161140014648438, 22.05439998626709, 4.169399991989136, 220.39167922973633, 1.6397390811443329, 5.39922532081604],
  "scale": [0.10742295993250746, 0.9022198700051385, 0.3970939910119104, 8.336401450687758, 0.07815235340852718, 0.03762435049413629],
  "cluster_centers": [
    [0.7115621566772461, 0.18475952744483948, 0.7892654538154602, 0.8202961683273315, 0.784477174282074, 0.8208693265914917],
    [-0.6638749241828918, -0.49781298637390137, 0.27034997940063477, -0.8594855666160583, 0.27699118852615356, -0.860650897026062],
    [0.4186360836029053, 0.6239734888076782, -1.1296221017837524, 0.63169926404953, -1.1358827352523804, 0.6330371499061584]
  ]
};

// ---- IMPORTANT: These colors are intentionally NOT traffic-light colors.
// They represent "Zone A/B/C" visually without implying pollution.
// If you WANT Good/Moderate/Poor colors, set these to green/yellow/red.
const ZONE_COLORS = ["#2D9CDB", "#9B51E0", "#F2994A"]; // blue, purple, orange
const ZONE_NAMES  = ["Zone A", "Zone B", "Zone C"];

// For the ranked interpretation (relative quality)
const QUALITY_COLORS = ["#2ecc71", "#f1c40f", "#e74c3c"]; // Good, Moderate, Poor
const QUALITY_NAMES  = ["Good", "Moderate", "Poor"];

// Toggle this:
// true  = show Good/Moderate/Poor after ranking clusters by an index
// false = show Zone A/B/C (no “good/bad” implication)
const USE_QUALITY_RANKING = true;

// ================== KMeans Prediction ==================
function predictClusterPretrained(ph, temp, turbidity, tds) {
  const mean = KMEANS_MODEL.mean;
  const scale = KMEANS_MODEL.scale;
  const centers = KMEANS_MODEL.cluster_centers;

  const logTurb = Math.log1p(turbidity);
  const logTds  = Math.log1p(tds);

  const x = [ph, temp, turbidity, tds, logTurb, logTds];
  const z = x.map((v, i) => (v - mean[i]) / scale[i]);

  let best = 0;
  let bestDist = Infinity;

  for (let c = 0; c < centers.length; c++) {
    let dist = 0;
    for (let i = 0; i < z.length; i++) {
      const d = z[i] - centers[c][i];
      dist += d * d;
    }
    if (dist < bestDist) {
      bestDist = dist;
      best = c;
    }
  }
  return best; // 0,1,2
}

// ================== Utility ==================
function padArray(arr, length) {
  const result = (arr || []).slice();
  while (result.length < length) result.push(null);
  return result;
}
function addJitter(arr, amount = 0.05) {
  return (arr || []).map(v => v !== null && v !== undefined ? v + (Math.random() * 2 - 1) * amount : null);
}
function mean(arr){ return arr.length ? arr.reduce((a,b)=>a+b,0)/arr.length : 0; }
function std(arr){
  if (arr.length < 2) return 1;
  const m = mean(arr);
  const v = arr.reduce((s,x)=>s+(x-m)*(x-m),0)/(arr.length-1);
  return Math.sqrt(v) || 1;
}

// ================== Cluster -> Quality Ranking ==================
// We rank clusters using a simple index:
// higher turbidity + higher TDS + pH deviation => "worse"
function buildGlobalStats(points){
  const tds = points.map(p=>p.tds).filter(v=>v!=null);
  const turb = points.map(p=>p.turb).filter(v=>v!=null);
  return {
    tdsMean: mean(tds), tdsStd: std(tds),
    turbMean: mean(turb), turbStd: std(turb)
  };
}
function pointQualityScore(p, stats){
  const zTds  = (p.tds  - stats.tdsMean)  / stats.tdsStd;
  const zTurb = (p.turb - stats.turbMean) / stats.turbStd;
  const phPenalty = Math.abs(p.ph - 7); // small penalty
  // weights (explainable in PPT)
  return 0.5*zTds + 0.4*zTurb + 0.1*phPenalty;
}
function mapClustersToQuality(points){
  const stats = buildGlobalStats(points);
  const scoresByCluster = [[],[],[]];

  points.forEach(p=>{
    if (p.clusterIdx==null) return;
    if (p.ph==null||p.turb==null||p.tds==null) return;
    scoresByCluster[p.clusterIdx].push(pointQualityScore(p, stats));
  });

  const avgScore = scoresByCluster.map(a => a.length ? mean(a) : 0);
  const order = [0,1,2].sort((a,b)=>avgScore[a]-avgScore[b]); // lowest score = best

  const qualityOfCluster = {};
  qualityOfCluster[order[0]] = 0; // Good
  qualityOfCluster[order[1]] = 1; // Moderate
  qualityOfCluster[order[2]] = 2; // Poor

  return { qualityOfCluster, avgScore };
}

// ==================== Summary & Legend ====================
function updateSummaryFromPoints(cardKey, points) {
  const card = activeCards[cardKey];
  if (!card) return;

  const summaryEl = document.getElementById(`${card.safeName}_llm`);
  if (!summaryEl) return;

  if (!points || !points.length) {
    summaryEl.innerHTML = "No data available yet.";
    return;
  }

  const n = points.length;
  let sumPh = 0, sumTemp = 0, sumTurb = 0, sumTds = 0;
  let validPh = 0, validTemp = 0, validTurb = 0, validTds = 0;

  points.forEach(p => {
    if (p.ph != null)   { sumPh   += p.ph; validPh++; }
    if (p.temp != null) { sumTemp += p.temp; validTemp++; }
    if (p.turb != null) { sumTurb += p.turb; validTurb++; }
    if (p.tds != null)  { sumTds  += p.tds; validTds++; }
  });

  const avgPh   = validPh   ? (sumPh / validPh).toFixed(2) : "-";
  const avgTemp = validTemp ? (sumTemp / validTemp).toFixed(1) : "-";
  const avgTurb = validTurb ? (sumTurb / validTurb).toFixed(1) : "-";
  const avgTds  = validTds  ? (sumTds  / validTds).toFixed(0) : "-";

  // counts by displayed group (either zones or quality)
  let counts = [0,0,0];
  points.forEach(p=>{
    const idx = (USE_QUALITY_RANKING ? p.qualityIdx : p.clusterIdx);
    if (idx != null && counts[idx] != null) counts[idx]++;
  });
  const pct = counts.map(c => Math.round((c / n) * 100));
  const dominant = counts.indexOf(Math.max(...counts));

  if (!USE_QUALITY_RANKING) {
    // heterogeneity wording (no good/bad)
    summaryEl.innerHTML =
      `Based on <b>${n}</b> measurements, the lake shows <b>heterogeneity</b> across <b>3 zones</b>.<br>` +
      `Dominant zone: <b>${ZONE_NAMES[dominant]}</b> (${pct[dominant]}% of points).<br>` +
      `Average conditions: pH <b>${avgPh}</b>, temperature <b>${avgTemp}°C</b>, ` +
      `turbidity <b>${avgTurb} NTU</b>, TDS <b>${avgTds} ppm</b>.<br>` +
      `Interpretation: clusters indicate <b>different water-characteristic patterns</b> in different areas (not an absolute pollution label).`;
  } else {
    // ranked quality wording
    summaryEl.innerHTML =
      `Based on <b>${n}</b> measurements, the lake shows <b>spatial heterogeneity</b> with 3 groups.<br>` +
      `We rank clusters into <b>Good / Moderate / Poor</b> using an index (TDS + turbidity + pH deviation).<br>` +
      `Dominant group: <b>${QUALITY_NAMES[dominant]}</b> (${pct[dominant]}% of points).<br>` +
      `Average conditions: pH <b>${avgPh}</b>, temperature <b>${avgTemp}°C</b>, ` +
      `turbidity <b>${avgTurb} NTU</b>, TDS <b>${avgTds} ppm</b>.`;
  }
}

function updateLegend(safeName, counts) {
  const legendDiv = document.getElementById(`${safeName}_legend`);
  if (!legendDiv) return;

  if (!USE_QUALITY_RANKING) {
    legendDiv.innerHTML =
      `<div>🔵 <span style="color:${ZONE_COLORS[0]};">${ZONE_NAMES[0]}</span> — ${counts[0]}</div>` +
      `<div>🟣 <span style="color:${ZONE_COLORS[1]};">${ZONE_NAMES[1]}</span> — ${counts[1]}</div>` +
      `<div>🟠 <span style="color:${ZONE_COLORS[2]};">${ZONE_NAMES[2]}</span> — ${counts[2]}</div>`;
  } else {
    legendDiv.innerHTML =
      `<div>🌿 <span style="color:${QUALITY_COLORS[0]};">${QUALITY_NAMES[0]}</span> — ${counts[0]}</div>` +
      `<div>⚠ <span style="color:${QUALITY_COLORS[1]};">${QUALITY_NAMES[1]}</span> — ${counts[1]}</div>` +
      `<div>🚨 <span style="color:${QUALITY_COLORS[2]};">${QUALITY_NAMES[2]}</span> — ${counts[2]}</div>`;
  }
}

// ==================== Map dots ====================
function drawClusteredDots(markerLayer, map, points, qualityOfCluster) {
  markerLayer.clearLayers();
  if (!points || !points.length) return;

  const latlngs = [];
  points.forEach(p => {
    const lat = p.lat, lon = p.lon;
    const ph = p.ph, temp = p.temp, turb = p.turb, tds = p.tds;
    if (lat == null || lon == null || ph == null || temp == null || turb == null || tds == null) return;

    // raw cluster
    const clusterIdx = predictClusterPretrained(ph, temp, turb, tds);
    p.clusterIdx = clusterIdx;

    // mapped quality (optional)
    if (USE_QUALITY_RANKING && qualityOfCluster) {
      p.qualityIdx = qualityOfCluster[clusterIdx];
    } else {
      p.qualityIdx = null;
    }

    const shownIdx = USE_QUALITY_RANKING ? p.qualityIdx : clusterIdx;
    const color = USE_QUALITY_RANKING
      ? (QUALITY_COLORS[shownIdx] || "#888")
      : (ZONE_COLORS[shownIdx] || "#888");

    const title = USE_QUALITY_RANKING
      ? `<b>${QUALITY_NAMES[shownIdx]}</b> (Cluster ${clusterIdx + 1})`
      : `<b>${ZONE_NAMES[shownIdx]}</b> (Cluster ${clusterIdx + 1})`;

    L.circleMarker([lat, lon], {
      radius: 4,
      color: color,
      fillColor: color,
      fillOpacity: 0.9
    }).bindPopup(
      `${title}<br>` +
      `pH: ${Number(ph).toFixed(2)}<br>` +
      `Temp: ${Number(temp).toFixed(1)} °C<br>` +
      `Turbidity: ${Number(turb).toFixed(1)} NTU<br>` +
      `TDS: ${Number(tds).toFixed(0)} ppm`
    ).addTo(markerLayer);

    latlngs.push([lat, lon]);
  });

  if (latlngs.length) map.fitBounds(latlngs);
}

// ==================== Create Lake Card ====================
function createLakeCard(name, uid, note = "") {
  const safeName = name.replace(/\s+/g, "") + "_" + Date.now() + "_" + Math.random().toString(36).substr(2, 5);
  const key = uid || safeName;

  const card = document.createElement("div");
card.className = "lake-card";
card.innerHTML = `
  <div class="lake-title">
    <span>${name}</span>
    <div>
      <button class="toggle-btn">▶</button>
      <button class="remove-btn" style="margin-left:8px;">🗑</button>
    </div>
  </div>

  <div class="chart-container">
    <canvas id="${safeName}_chart"></canvas>
  </div>

  <div class="lake-map" id="${safeName}_map"></div>

  <div class="map-controls">
    <button class="small-btn load-json-btn">Load Cluster JSON</button>
    <button class="small-btn record-btn" id="${safeName}_record">Start Recording</button>
  </div>

  <input type="file" accept=".json" class="json-input" style="display:none">

  <div class="llm-box">
    <h4>${USE_QUALITY_RANKING ? "Heterogeneity Summary (Ranked)" : "Heterogeneity Summary"}</h4>

    <p id="${safeName}_llm">${note || "No data yet."}</p>

    ${
      USE_QUALITY_RANKING
        ? `
          <div class="mini-note" style="margin-top:8px; font-size:0.8rem; color:#456;">
            Note: Good/Moderate/Poor is <b>relative</b> for this dataset; K-Means is unsupervised.
          </div>
        `
        : `
          <div class="mini-note" style="margin-top:8px; font-size:0.8rem; color:#456;">
            Note: Colors represent cluster groups only (no good/bad meaning).
          </div>
        `
    }
  </div>

  <div class="legend" id="${safeName}_legend"></div>
  `;

  document.getElementById("bulletinList").appendChild(card);

  // Toggle chart
  const toggleBtn = card.querySelector(".toggle-btn");
  const chartContainer = card.querySelector(".chart-container");
  toggleBtn.addEventListener("click", () => {
    const shown = chartContainer.style.display !== "none";
    chartContainer.style.display = shown ? "none" : "block";
    toggleBtn.classList.toggle("open", !shown);
  });

  // Remove button
  const removeBtn = card.querySelector(".remove-btn");
  removeBtn.addEventListener("click", () => {
    if (!confirm(`Remove "${name}"?`)) return;
    if (uid && recordIntervals[uid]) {
      clearInterval(recordIntervals[uid]);
      delete recordIntervals[uid];
    }
    delete activeCards[key];
    card.remove();
  });

  // Chart
  const ctx = card.querySelector("canvas").getContext("2d");
  const chart = new Chart(ctx, {
    type: "line",
    data: {
      labels: [],
      datasets: [
        { label: "pH", data: [], borderColor: "#2396f3", fill: false },
        { label: "Temperature", data: [], borderColor: "#2ecc71", fill: false },
        { label: "Turbidity", data: [], borderColor: "#f1c40f", fill: false },
        { label: "TDS", data: [], borderColor: "#9b59b6", fill: false }
      ]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      plugins: {
        zoom: {
          zoom: { wheel: { enabled: true }, pinch: { enabled: true }, mode: "xy" },
          pan: { enabled: true, mode: "xy", modifierKey: "shift" }
        },
        legend: { display: true }
      },
      interaction: { intersect: false, mode: "index" }
    }
  });

  // Map
  const map = L.map(`${safeName}_map`).setView([-0.5, 100], 6);
  L.tileLayer("https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png", { maxZoom: 19 }).addTo(map);
  const markerLayer = L.layerGroup().addTo(map);

  activeCards[key] = { chart, map, markerLayer, safeName, uid };

  // Initialize legend
  updateLegend(safeName, [0,0,0]);

  // JSON loader
  const jsonInput = card.querySelector(".json-input");
  card.querySelector(".load-json-btn").addEventListener("click", () => jsonInput.click());

  jsonInput.addEventListener("change", evt => {
    const file = evt.target.files[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = () => {
      try {
        const json = JSON.parse(reader.result);
        const items = Array.isArray(json) ? json : Object.values(json);

        const points = items.map(p => {
          const ph = p.ph ?? p.phValue ?? null;
          const temp = p.temperature ?? p.temp ?? null;
          const turb = p.turbidity ?? p.turb ?? null;
          const tds = p.tds ?? p.tdsValue ?? null;
          const lat = p.lat ?? p.latitude ?? null;
          const lon = p.lon ?? p.longitude ?? null;

          return {
            ph: (ph != null ? Number(ph) : null),
            temp: (temp != null ? Number(temp) : null),
            turb: (turb != null ? Number(turb) : null),
            tds: (tds != null ? Number(tds) : null),
            lat: (lat != null ? Number(lat) : null),
            lon: (lon != null ? Number(lon) : null),
            clusterIdx: null,
            qualityIdx: null
          };
        });

        // First pass: get clusters assigned (needed before ranking)
        points.forEach(p=>{
          if (p.ph==null||p.temp==null||p.turb==null||p.tds==null) return;
          p.clusterIdx = predictClusterPretrained(p.ph,p.temp,p.turb,p.tds);
        });

        // Optional: map cluster -> Good/Moderate/Poor
        let qualityOfCluster = null;
        if (USE_QUALITY_RANKING) {
          qualityOfCluster = mapClustersToQuality(points).qualityOfCluster;
          points.forEach(p=>{
            if (p.clusterIdx==null) return;
            p.qualityIdx = qualityOfCluster[p.clusterIdx];
          });
        }

        // Draw dots
        drawClusteredDots(markerLayer, map, points, qualityOfCluster);

        // Chart update + point colors
        const n = points.length;
        const phArr = points.map(p => p.ph);
        const tempArr = points.map(p => p.temp);
        const turbArr = points.map(p => p.turb);
        const tdsArr = points.map(p => p.tds);

        const colorArray = points.map(p => {
          const idx = USE_QUALITY_RANKING ? p.qualityIdx : p.clusterIdx;
          if (idx == null) return "#bdc3c7";
          return USE_QUALITY_RANKING ? (QUALITY_COLORS[idx] || "#bdc3c7") : (ZONE_COLORS[idx] || "#bdc3c7");
        });

        chart.data.labels = Array.from({ length: n }, (_, i) => i);
        chart.data.datasets[0].data = addJitter(padArray(phArr, n), 0.1);
        chart.data.datasets[1].data = addJitter(padArray(tempArr, n), 0.1);
        chart.data.datasets[2].data = addJitter(padArray(turbArr, n), 0.1);
        chart.data.datasets[3].data = addJitter(padArray(tdsArr, n), 0.2);
        chart.data.datasets.forEach(ds => ds.pointBackgroundColor = colorArray);
        chart.update();

        // Summary + legend counts
        updateSummaryFromPoints(key, points);
        const counts = [0,0,0];
        points.forEach(p => {
          const idx = USE_QUALITY_RANKING ? p.qualityIdx : p.clusterIdx;
          if (idx != null) counts[idx]++;
        });
        updateLegend(safeName, counts);

      } catch (err) {
        console.error(err);
        alert("Failed to parse JSON. Make sure it's valid.");
      }
    };
    reader.readAsText(file);
  });

  // Record button (reads from Firebase path == uid)
  const recordBtn = card.querySelector(".record-btn");
  recordBtn.addEventListener("click", () => {
    if (!uid) {
      alert("To record live data, please create this card with a Firebase UID.");
      return;
    }

    if (recordBtn.classList.contains("recording")) {
      recordBtn.classList.remove("recording");
      recordBtn.textContent = "Start Recording";
      if (recordIntervals[uid]) { clearInterval(recordIntervals[uid]); delete recordIntervals[uid]; }
      return;
    }

    recordBtn.classList.add("recording");
    recordBtn.textContent = "Stop Recording";

    recordIntervals[uid] = setInterval(() => {
      db.ref(uid).once("value").then(snapshot => {
        const val = snapshot.val();
        if (!val) return;

        const items = Array.isArray(val) ? val : Object.values(val);

        const points = items.map(p => {
          const ph = p.ph ?? p.phValue ?? null;
          const temp = p.temperature ?? p.temp ?? null;
          const turb = p.turbidity ?? p.turb ?? null;
          const tds = p.tds ?? p.tdsValue ?? null;
          const lat = p.lat ?? p.latitude ?? null;
          const lon = p.lon ?? p.longitude ?? null;

          return {
            ph: (ph != null ? Number(ph) : null),
            temp: (temp != null ? Number(temp) : null),
            turb: (turb != null ? Number(turb) : null),
            tds: (tds != null ? Number(tds) : null),
            lat: (lat != null ? Number(lat) : null),
            lon: (lon != null ? Number(lon) : null),
            clusterIdx: null,
            qualityIdx: null
          };
        });

        // cluster assign
        points.forEach(p=>{
          if (p.ph==null||p.temp==null||p.turb==null||p.tds==null) return;
          p.clusterIdx = predictClusterPretrained(p.ph,p.temp,p.turb,p.tds);
        });

        // optional ranking
        let qualityOfCluster = null;
        if (USE_QUALITY_RANKING) {
          qualityOfCluster = mapClustersToQuality(points).qualityOfCluster;
          points.forEach(p=>{
            if (p.clusterIdx==null) return;
            p.qualityIdx = qualityOfCluster[p.clusterIdx];
          });
        }

        drawClusteredDots(markerLayer, map, points, qualityOfCluster);

        const n = points.length;
        const phArr = points.map(p => p.ph);
        const tempArr = points.map(p => p.temp);
        const turbArr = points.map(p => p.turb);
        const tdsArr = points.map(p => p.tds);

        const colorArray = points.map(p => {
          const idx = USE_QUALITY_RANKING ? p.qualityIdx : p.clusterIdx;
          if (idx == null) return "#bdc3c7";
          return USE_QUALITY_RANKING ? (QUALITY_COLORS[idx] || "#bdc3c7") : (ZONE_COLORS[idx] || "#bdc3c7");
        });

        chart.data.labels = Array.from({ length: n }, (_, i) => i);
        chart.data.datasets[0].data = addJitter(padArray(phArr, n), 0.1);
        chart.data.datasets[1].data = addJitter(padArray(tempArr, n), 0.1);
        chart.data.datasets[2].data = addJitter(padArray(turbArr, n), 0.1);
        chart.data.datasets[3].data = addJitter(padArray(tdsArr, n), 0.2);
        chart.data.datasets.forEach(ds => ds.pointBackgroundColor = colorArray);
        chart.update();

        updateSummaryFromPoints(key, points);

        const counts = [0,0,0];
        points.forEach(p => {
          const idx = USE_QUALITY_RANKING ? p.qualityIdx : p.clusterIdx;
          if (idx != null) counts[idx]++;
        });
        updateLegend(safeName, counts);

      });
    }, 5000);
  });
}

// ==================== Add Button ====================
document.getElementById("postBtn").addEventListener("click", () => {
  const name = document.getElementById("lakeNameInput").value.trim();
  const uid  = document.getElementById("uidInput").value.trim();
  const note = document.getElementById("statusInput").value.trim();

  if (!name) { alert("Enter lake name"); return; }

  createLakeCard(name, uid, note);

  document.getElementById("lakeNameInput").value = "";
  document.getElementById("uidInput").value = "";
  document.getElementById("statusInput").value = "";
});
</script>
</body>
</html>

this for html web