// control.js — Waterfall ESP32 Controller Modules
// ES Module structure cho control.html

// ── Constants ─────────────────────────────────────────────
const G = 980;
const FW_BOARDS = 10;
const FRAME_BYTES = 4 + FW_BOARDS;
const ADMIN_PASSWORD = 'waterfall';

// ── State Manager ───────────────────────────────────────
const State = {
  numBoards: 10,
  CHIPS: 10,
  VALVES: 80,
  
  connected: false,
  connMode: 'none', // 'none' | 'lan' | 'mqtt'
  mqttTarget: '',
  
  optRmbg: true,
  optInv: false,
  optSat: true,
  optFlipH: false,
  optFlipV: true,
  
  thrVal: 40,
  dilVal: 1,
  
  frames: [],
  pixelRows: [],
  lastImg: null,
  
  lastFpsTime: 0,
  fpsCount: 0,
  fps: 0,
  
  playbackSpeed: 1.0,
  
  _currentMode: 'stream',
  _soundPattern: 'ripple',
  _textPerChar: false,
  _effectPattern: 'rain',
  _patternLoopActive: false,
  
  // Live simulation state
  liveValveBits: null,
  liveColIv: null,
  liveSimRunning: false,
  liveSimStartTime: 0,
  livePrevBits: null,
  _clockSimTimer: null,
  _effectTimer: null,
  _scriptTimeout: null,
  
  // Simulation state
  simRunning: false,
  animId: null,
  simCanvas: null,
  simCtx: null,
  simStartTime: 0,
  
  // Pump state
  _pumpCurMode: 'auto',
  _pumpPollIv: null,
  
  // Creator state
  creatorRows: [],
  creatorSelRow: 0,
  _creatorDragState: null,
  
  // WebSocket
  ws: null,
  wsConnected: false
};

// ── Shared helpers ───────────────────────────────────────────
function _clearSimTimers() {
  if (State._clockSimTimer) { clearInterval(State._clockSimTimer); State._clockSimTimer = null; }
  if (State._effectTimer)   { clearInterval(State._effectTimer); State._effectTimer = null; }
  if (State._scriptTimeout) { clearTimeout(State._scriptTimeout); State._scriptTimeout = null; }
}

// ── Utils ─────────────────────────────────────────────────
const Utils = {
  delay(ms) {
    return new Promise(r => setTimeout(r, ms));
  },
  
  log(msg, cls = "log-info") {
    const d = document.getElementById("log");
    const t = new Date().toLocaleTimeString("en", { hour12: false });
    d.innerHTML += `<div class="${cls}">[${t}] ${msg}</div>`;
    d.scrollTop = d.scrollHeight;
  },
  
  mkFrame(ts, bits) {
    const b = new ArrayBuffer(FRAME_BYTES);
    new DataView(b).setUint32(0, ts >>> 0, true);
    const src = bits instanceof Uint8Array ? bits : new Uint8Array(bits);
    const n = Math.min(src.length, FW_BOARDS);
    const off = FW_BOARDS - n;
    new Uint8Array(b).set(src.subarray(0, n), 4 + off);
    return b;
  },
  
  _toHex10(arr) {
    const padded = new Uint8Array(10);
    const n = arr.length;
    padded.set(arr, 10 - n);
    return Array.from(padded).map(b => b.toString(16).padStart(2, '0')).join('');
  }
};

// ── Connection Module ─────────────────────────────────────
const Connection = {
  connectESP() {
    if (location.protocol === "https:") {
      // MQTT mode
      return this.connectMQTT();
    }
    return this.connectLAN();
  },
  
  connectMQTT() {
    const sel = document.getElementById("mqtt-device-select");
    const chosen = sel.value;
    if (!chosen) { Utils.log("Chọn thiết bị từ dropdown trước!", "log-warn"); return; }
    
    State.mqttTarget = chosen;
    State.connMode = 'mqtt';
    State.connected = true;
    
    this._updateUI('mqtt', `MQTT → ${chosen}`);
    document.getElementById("conn-dot").className = 'dot on';
    document.getElementById("conn-dot").style.background = '#5db8a6';
    document.getElementById("btn-send").disabled = State.frames.length === 0;
    document.getElementById("btn-sim").disabled = false;
    
    Pump.startPoll();
    Utils.log(`Kết nối MQTT → ${chosen}`, "log-ok");
  },
  
  connectLAN() {
    const ip = document.getElementById("ip").value.trim();
    const port = parseInt(document.getElementById("port").value);
    const wsUrl = `ws://${ip}:${port}`;
    Utils.log(`Thử kết nối LAN: ${wsUrl}...`);
    
    if (State.ws) { State.ws.close(); State.ws = null; }
    
    try {
      State.ws = new WebSocket(wsUrl);
      State.ws.binaryType = "arraybuffer";
      
      State.ws.onopen = () => {
        State.connected = true;
        State.connMode = 'lan';
        this._updateUI('lan', `LAN ${ip}`);
        document.getElementById("conn-dot").style.background = "#27ae60";
        document.getElementById("btn-send").disabled = false;
        document.getElementById("btn-sim").disabled = false;
        Utils.log(`Kết nối LAN thành công @ ${ip}`, "log-ok");
      };
      
      State.ws.onclose = (e) => {
        State.connected = false;
        State.connMode = 'none';
        this._updateUI('none', 'Disconnected');
        document.getElementById("conn-dot").style.background = "#e74c3c";
        document.getElementById("btn-send").disabled = true;
        Utils.log(`Mất kết nối (code=${e.code})`, "log-warn");
      };
      
      State.ws.onerror = () => {
        Utils.log(`Không kết nối được tới ${ip}:${port}`, "log-err");
      };
      
      State.ws.onmessage = (e) => {
        if (typeof e.data !== 'string') return;
        try {
          const d = JSON.parse(e.data);
          if (d.type === 'valves' && d.bits) LiveValveHandler.onValves(d.bits);
          else if (d.type === 'pump') Pump.updateUI(d);
        } catch {}
      };
    } catch (e) { Utils.log(e.message, "log-err"); }
  },
  
  _updateUI(mode, label) {
    const badge = document.getElementById("mode-badge");
    if (mode === 'lan') {
      badge.style.display = "inline";
      badge.className = 'badge badge-teal';
      badge.textContent = "LAN";
    } else if (mode === 'mqtt') {
      badge.style.display = "inline";
      badge.className = 'badge badge-coral';
      badge.textContent = "MQTT";
    } else {
      badge.style.display = "none";
    }
    document.getElementById("conn-label").textContent = label;
  },
  
  disconnectESP() {
    if (State.ws) { State.ws.close(); State.ws = null; }
    State.connected = false;
    State.connMode = 'none';
    State.mqttTarget = '';
    this._updateUI('none', 'Disconnected');
    document.getElementById("conn-dot").style.background = "#e74c3c";
  },
  
  async sendCommand(cmd, opts = {}) {
    const payload = Object.assign({ cmd }, opts);
    if (State.connMode === 'lan' && State.ws && State.ws.readyState === 1) {
      State.ws.send(JSON.stringify(payload));
      return { success: true, method: 'lan' };
    } else if (State.connMode === 'mqtt') {
      if (State.mqttTarget) payload.target = State.mqttTarget;
      const res = await fetch('/api/cmd', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      });
      const data = await res.json();
      if (!data.success) throw new Error(data.error || 'MQTT failed');
      return data;
    } else {
      throw new Error('Not connected');
    }
  },
  
  sendRaw(buf) {
    if (State.connMode === 'lan' && State.ws && State.ws.readyState === 1) {
      State.ws.send(buf);
    }
  },
  
  sendReset() {
    if (State.connMode === 'lan') {
      this.sendRaw(Utils.mkFrame(0xffffffff, new Uint8Array(10)));
    } else {
      this.sendCommand('ALL_OFF').catch(e => Utils.log(e.message, "log-err"));
    }
  },
  
  sendStart() {
    this.sendRaw(Utils.mkFrame(0xfffffffe, new Uint8Array(10)));
  },
  
  fetchFwVersion(ip) {
    const badge = document.getElementById('fw-badge');
    badge.textContent = '…';
    badge.style.display = '';
    fetch(`http://${ip}:8080/version`, { signal: AbortSignal.timeout(4000) })
      .then(r => r.json())
      .then(j => {
        const v = j.fw || '?';
        badge.textContent = 'FW ' + v;
        Utils.log(`Firmware: v${v}`, 'log-ok');
      })
      .catch(() => { badge.style.display = 'none'; });
  }
};

// ── Image Processing Module ─────────────────────────────────
const ImageProcessor = {
  loadFile(input) {
    const file = input.files[0];
    if (!file) return;
    Utils.log(`Loading: ${file.name}`);
    const url = URL.createObjectURL(file);
    document.getElementById("thumb-preview").src = url;
    document.getElementById("thumb-preview").style.display = "block";
    
    const img = new Image();
    img.onload = () => {
      State.lastImg = img;
      Utils.log(`Loaded ${img.width}×${img.height}px`, "log-ok");
      this.reprocess();
    };
    img.src = url;
  },
  
  reprocess() {
    if (State.lastImg) {
      State.pixelRows = this.extractRows(State.lastImg);
      FrameBuilder.buildSequence();
    }
  },
  
  extractRows(img) {
    const W = State.VALVES;
    const H = Math.max(1, Math.round(img.height * (W / img.width)));
    const c = document.createElement("canvas");
    c.width = W;
    c.height = H;
    const ctx = c.getContext("2d");
    ctx.fillStyle = "#ffffff";
    ctx.fillRect(0, 0, W, H);
    ctx.drawImage(img, 0, 0, W, H);
    const d = ctx.getImageData(0, 0, W, H).data;
    const bin = new Uint8Array(W * H);
    
    for (let i = 0; i < W * H; i++) {
      const r = d[i * 4], g = d[i * 4 + 1], b = d[i * 4 + 2];
      const bright = r * 0.299 + g * 0.587 + b * 0.114;
      const maxC = Math.max(r, g, b), minC = Math.min(r, g, b);
      const sat = maxC === 0 ? 0 : (maxC - minC) / maxC;
      
      let isFg = false;
      if (State.optInv) {
        isFg = bright < 255 - State.thrVal;
      } else if (State.optRmbg) {
        isFg = bright - State.thrVal > 0 || sat * 255 - State.thrVal * 0.5 > 0;
      } else {
        isFg = bright > State.thrVal;
      }
      if (State.optSat && sat > 0.3 && bright > 30) isFg = true;
      bin[i] = isFg ? 1 : 0;
    }
    
    let cur = bin;
    for (let pass = 0; pass < State.dilVal; pass++) {
      const nxt = new Uint8Array(W * H);
      for (let y = 0; y < H; y++)
        for (let x = 0; x < W; x++) {
          if (cur[y * W + x]) { nxt[y * W + x] = 1; continue; }
          if ((x > 0 && cur[y * W + x - 1]) || (x < W - 1 && cur[y * W + x + 1]) ||
              (y > 0 && cur[(y - 1) * W + x]) || (y < H - 1 && cur[(y + 1) * W + x]))
            nxt[y * W + x] = 1;
        }
      cur = nxt;
    }
    
    const rows = [];
    for (let y = 0; y < H; y++) rows.push(cur.slice(y * W, (y + 1) * W));
    if (State.optFlipH) for (const row of rows) row.reverse();
    if (State.optFlipV) rows.reverse();
    return rows;
  },
  
  autoThreshold() {
    if (!State.lastImg) return;
    const W = State.VALVES, H = Math.max(1, Math.round(State.lastImg.height * (W / State.lastImg.width)));
    const c = document.createElement("canvas");
    c.width = W;
    c.height = H;
    const ctx = c.getContext("2d");
    ctx.fillStyle = "#fff"; ctx.fillRect(0, 0, W, H);
    ctx.drawImage(State.lastImg, 0, 0, W, H);
    const d = ctx.getImageData(0, 0, W, H).data;
    const scores = [];
    for (let i = 0; i < W * H; i++) {
      const r = d[i * 4], g = d[i * 4 + 1], b = d[i * 4 + 2];
      const bright = r * 0.299 + g * 0.587 + b * 0.114;
      const mx = Math.max(r, g, b), mn = Math.min(r, g, b);
      const sat = mx === 0 ? 0 : (mx - mn) / mx;
      let s = Math.max(0, bright - 15);
      if (sat > 0.18) s = Math.max(s, sat * 300);
      scores.push(s);
    }
    scores.sort((a, b) => a - b);
    const auto = Math.round(scores[Math.floor(scores.length * 0.7)]);
    document.getElementById("thr").value = auto;
    State.thrVal = auto;
    document.getElementById("thr-lbl").textContent = auto;
    Utils.log(`Auto threshold: ${auto}`, "log-ok");
    this.reprocess();
  }
};

// ── Frame Builder Module ───────────────────────────────────
const FrameBuilder = {
  getAdv() {
    return parseInt(document.getElementById("adv").value) || 80;
  },
  
  getH() {
    return parseFloat(document.getElementById("height").value) || 150;
  },
  
  recalc() {
    const tf = Math.round(Math.sqrt((2 * this.getH()) / G) * 1000);
    document.getElementById("adv-lbl").textContent = `${this.getAdv()} ms (t_fall=${tf}ms)`;
    if (State.pixelRows.length) this.buildSequence();
  },
  
  buildSequence() {
    if (!State.pixelRows.length) return;
    
    const adv = this.getAdv();
    const tfall = Math.round(Math.sqrt((2 * this.getH()) / G) * 1000);
    const colIv = StrokeAnalysis.buildColIv(State.pixelRows, adv);
    
    const events = [];
    for (let c = 0; c < State.VALVES; c++) {
      for (const iv of colIv[c]) {
        const tOpen = Math.max(0, iv.vOpen - tfall);
        const tClose = Math.max(0, iv.vClose - tfall);
        events.push({ t: tOpen, col: c, open: true });
        events.push({ t: tClose, col: c, open: false });
      }
    }
    events.sort((a, b) => a.t !== b.t ? a.t - b.t : (a.open ? -1 : 1));
    
    const state = new Uint8Array(State.CHIPS);
    State.frames = [{ ts_ms: 0, bits: new Uint8Array(State.CHIPS) }];
    let i = 0;
    while (i < events.length) {
      const t = events[i].t;
      while (i < events.length && events[i].t === t) {
        const c = events[i].col;
        const revByte = State.CHIPS - 1 - Math.floor(c / 8);
        if (events[i].open) state[revByte] |= (1 << (c % 8));
        else state[revByte] &= ~(1 << (c % 8));
        i++;
      }
      State.frames.push({ ts_ms: t, bits: new Uint8Array(state) });
    }
    State.frames.push({ ts_ms: (events[events.length-1]?.t || 0) + adv * 2, bits: new Uint8Array(State.CHIPS) });
    
    document.getElementById("rows-d").value = State.pixelRows.length;
    document.getElementById("frames-d").value = State.frames.length;
    document.getElementById("btn-sim").disabled = false;
    document.getElementById("btn-send").disabled = !State.connected;
    Utils.log(`Smooth: ${State.frames.length} events`, "log-ok");
    AnimationInfo.update();
  }
};

// ── Stroke Analysis Module ─────────────────────────────────
const StrokeAnalysis = {
  fitStrokeFn(pts) {
    if (!pts.length) return () => 0;
    if (pts.length === 1) return () => pts[0].y;
    
    const ys = pts.map(p => p.y);
    const yRng = Math.max(...ys) - Math.min(...ys);
    if (yRng < 1.5) return () => ys.reduce((a,b) => a+b, 0) / ys.length;
    
    let sw=0, sx=0, sy=0, sxx=0, sxy=0;
    for (const {x, y} of pts) { sw++; sx+=x; sy+=y; sxx+=x*x; sxy+=x*y; }
    const det = sw*sxx - sx*sx;
    if (Math.abs(det) < 1e-6) return () => sy/sw;
    
    const a1 = (sw*sxy - sx*sy) / det;
    const b1 = (sy - a1*sx) / sw;
    
    let resSum = 0;
    for (const {x, y} of pts) {
      const diff = y - (a1*x + b1);
      resSum += diff * diff;
    }
    const rmse = Math.sqrt(resSum / pts.length);
    
    if (rmse > 1.5 && pts.length >= 4) {
      // Quadratic fit (simplified - only linear for now)
    }
    return (x) => a1*x + b1;
  },
  
  buildColIv(pixelRows, rowMs) {
    const nR = pixelRows.length;
    const NV = State.VALVES;
    const PROX = 3;
    
    const colRuns = [];
    for (let c = 0; c < NV; c++) {
      const runs = [];
      let rs = -1;
      for (let r = 0; r <= nR; r++) {
        const on = r < nR ? pixelRows[r][c] : false;
        if (on && rs < 0) rs = r;
        else if (!on && rs >= 0) { runs.push({rMin: rs, rMax: r-1}); rs = -1; }
      }
      colRuns.push(runs);
    }
    
    const visited = colRuns.map(runs => runs.map(() => false));
    const segments = [];
    
    for (let c = 0; c < NV; c++) {
      for (let i = 0; i < colRuns[c].length; i++) {
        if (visited[c][i]) continue;
        const seg = [];
        const queue = [{c, i}];
        visited[c][i] = true;
        while (queue.length) {
          const {c: qc, i: qi} = queue.shift();
          const run = colRuns[qc][qi];
          seg.push({c: qc, rMin: run.rMin, rMax: run.rMax});
          for (const nc of [qc-1, qc+1]) {
            if (nc < 0 || nc >= NV) continue;
            for (let ni = 0; ni < colRuns[nc].length; ni++) {
              if (visited[nc][ni]) continue;
              if (Math.abs(colRuns[nc][ni].rMin - run.rMin) <= PROX) {
                visited[nc][ni] = true;
                queue.push({c: nc, i: ni});
              }
            }
          }
        }
        segments.push(seg);
      }
    }
    
    const colIv = Array.from({length: NV}, () => []);
    for (const seg of segments) {
      seg.sort((a, b) => a.c - b.c);
      const pts = seg.map(s => ({x: s.c, y: s.rMin}));
      const fn = this.fitStrokeFn(pts);
      for (const {c, rMin, rMax} of seg) {
        const frac = fn(c);
        const dur = (rMax - rMin + 1) * rowMs;
        const openMs = Math.max(0, Math.round(frac * rowMs));
        colIv[c].push({vOpen: openMs, vClose: openMs + dur});
      }
    }
    for (let c = 0; c < NV; c++) colIv[c].sort((a, b) => a.vOpen - b.vOpen);
    return colIv;
  }
};

// ── Simulation Module ───────────────────────────────────────
const Simulation = {
  init() {
    State.simCanvas = document.getElementById("sim-canvas");
    State.simCtx = State.simCanvas?.getContext("2d");
  },
  
  start() {
    if (!State.pixelRows.length) { Utils.log("No image loaded", "log-warn"); return; }
    State.simRunning = true;
    State.simStartTime = performance.now();
    State.lastFpsTime = State.simStartTime;
    State.fpsCount = 0;
    this.tick();
  },
  
  stop() {
    State.simRunning = false;
    if (State.animId) cancelAnimationFrame(State.animId);
    State.animId = null;
  },
  
  stopAll() {
    if (State.animId) { cancelAnimationFrame(State.animId); State.animId = null; }
    State.simRunning = false;
    State._clearSimTimers();
    LiveValveHandler.stop();
    if (State.connMode === 'mqtt') {
      Connection.sendCommand('ALL_OFF').catch(() => {});
    } else {
      Connection.sendReset();
    }
    if (State.simCtx && State.simCanvas) {
      State.simCtx.clearRect(0, 0, State.simCanvas.width, State.simCanvas.height);
    }
    document.getElementById("progress").style.width = "0%";
  },
  
  tick() {
    if (!State.simRunning) return;
    // Simplified simulation tick
    State.animId = requestAnimationFrame(() => this.tick());
  },
  
  renderPreview(row, label) {
    const canvas = document.getElementById("pixel-canvas");
    if (!canvas) return;
    const containerW = (canvas.parentElement || document.body).clientWidth || 760;
    const pixW = Math.max(2, Math.floor(containerW / State.VALVES));
    canvas.width = State.VALVES * pixW;
    canvas.height = 44;
    const ctx = canvas.getContext("2d");
    ctx.fillStyle = "#020810";
    ctx.fillRect(0, 0, canvas.width, 44);
    
    let cnt = 0;
    for (let v = 0; v < State.VALVES; v++) {
      const on = Array.isArray(row) ? row[v] : (row instanceof Uint8Array ? (row[v >> 3] >> (7 - (v & 7))) & 1 : 0);
      ctx.fillStyle = on ? "#00d4ff" : "#030c18";
      ctx.fillRect(v * pixW, 3, Math.max(1, pixW - 1), 38);
      if (on) cnt++;
    }
    document.getElementById("preview-title").textContent = `80-valve — ${label}`;
    document.getElementById("dim-badge").textContent = `${cnt}/${State.VALVES} open`;
  }
};

// ── Mode Module ─────────────────────────────────────────────
const Mode = {
  set(mode) {
    State._currentMode = mode;
    this._updateButtons(mode);
    document.getElementById('sound-opts').style.display = mode === 'sound' ? '' : 'none';
    document.getElementById('clock-opts').style.display = mode === 'clock' ? '' : 'none';
    document.getElementById('text-opts').style.display = mode === 'text' ? '' : 'none';
    document.getElementById('effects-opts').style.display = mode === 'effect' ? '' : 'none';
    
    const opts = { mode };
    if (mode === 'sound') opts.pattern = State._soundPattern;
    else if (mode === 'clock') opts.sensitivity = parseInt(document.getElementById('clock-speed').value);
    else if (mode === 'text') {
      opts.text = document.getElementById('text-input').value || 'WATERFALL';
      opts.scale = parseInt(document.getElementById('text-size').value) || 2;
    }
    else if (mode === 'effect') opts.pattern = State._effectPattern;
    
    Connection.sendCommand('SET_MODE', opts).catch(e => console.warn('SET_MODE:', e));
  },
  
  _updateButtons(activeMode) {
    ['stream','sound','clock','text','effect'].forEach(m => {
      const btn = document.getElementById('mode-btn-' + m);
      if (btn) btn.className = m === activeMode ? 'btn btn-primary btn-sm' : 'btn btn-ghost btn-sm';
    });
  }
};

// ── Live Valve Handler Module ───────────────────────────────
const LiveValveHandler = {
  onValves(hexStr) {
    if (!State.liveValveBits) State.liveValveBits = new Uint8Array(State.CHIPS);
    for (let i = 0; i < State.CHIPS; i++) {
      State.liveValveBits[i] = parseInt(hexStr.slice(i * 2, i * 2 + 2), 16) || 0;
    }
    
    let cnt = 0;
    for (let v = 0; v < State.VALVES; v++) {
      if ((State.liveValveBits[v >> 3] >> (7 - (v & 7))) & 1) cnt++;
    }
    document.getElementById('live-active-count').textContent = cnt + ' / ' + State.VALVES;
    Simulation.renderPreview(State.liveValveBits, `ESP live — ${State._currentMode}`);
  },
  
  start() {
    State.liveColIv = Array.from({ length: State.VALVES }, () => []);
    State.livePrevBits = new Uint8Array(State.CHIPS);
    State.liveSimRunning = true;
    State.liveSimStartTime = performance.now();
  },
  
  stop() {
    State.liveSimRunning = false;
    State.liveColIv = null;
  }
};

// ── Pump Module ───────────────────────────────────────────
const Pump = {
  setMode(mode) {
    State._pumpCurMode = mode;
    document.getElementById('pump-btn-auto').classList.toggle('active', mode === 'auto');
    document.getElementById('pump-btn-manual').classList.toggle('active', mode === 'manual');
    document.getElementById('pump-auto-ui').style.display = mode === 'auto' ? '' : 'none';
    document.getElementById('pump-manual-ui').style.display = mode === 'auto' ? 'none' : '';
    Connection.sendCommand('SET_MODE', { mode: 'pump', pattern: mode });
  },
  
  cmd(action) {
    Connection.sendCommand('SET_MODE', { mode: 'pump', pattern: action });
  },
  
  updateUI(d) {
    const dot = (id, on) => {
      const el = document.getElementById(id);
      el.className = 'pump-dot ' + (on ? 'on' : 'off');
    };
    const tag = (id, txt) => { const el = document.getElementById(id); if (el) el.textContent = txt; };
    
    dot('pdot-low', d.level_low);
    dot('pdot-high', d.level_high);
    dot('pdot-pump', d.pump);
    tag('ptag-low', d.level_low ? 'Kích hoạt' : 'Bình thường');
    tag('ptag-high', d.level_high ? 'Đầy' : 'Chưa đầy');
    tag('ptag-pump', d.pump ? `▶ Đang chạy ${d.run_sec || 0}s` : '■ Dừng');
  },
  
  startPoll() {
    if (State._pumpPollIv) clearInterval(State._pumpPollIv);
    if (State.connMode !== 'mqtt') return;
    const poll = () => fetch('/api/pump/status').then(r => r.json()).then(d => this.updateUI(d)).catch(() => {});
    poll();
    State._pumpPollIv = setInterval(poll, 8000);
  }
};

// ── Animation Info Module ─────────────────────────────────
const AnimationInfo = {
  update() {
    const frameCount = State.frames.length;
    const rowCount = State.pixelRows.length;
    document.getElementById("anim-info").innerHTML = `
      <div>Frames: ${frameCount}</div>
      <div>Rows: ${rowCount}</div>
      <div>Boards: ${State.numBoards}</div>
      <div>Size: ${(frameCount * State.VALVES).toLocaleString()} bytes</div>
    `;
  }
};

// ── Drag & Drop Module ────────────────────────────────────
const DragDrop = {
  init() {
    const dz = document.getElementById("dropzone");
    dz.addEventListener("dragover", (e) => { e.preventDefault(); dz.classList.add("drag"); });
    dz.addEventListener("dragleave", () => dz.classList.remove("drag"));
    dz.addEventListener("drop", (e) => {
      e.preventDefault(); dz.classList.remove("drag");
      const file = e.dataTransfer.files[0];
      if (file) {
        const fi = document.getElementById("file-input");
        const dt = new DataTransfer(); dt.items.add(file); fi.files = dt.files;
        ImageProcessor.loadFile(fi);
      }
    });
  }
};

// ── Resize Handler Module ─────────────────────────────────
const ResizeHandler = {
  init() {
    const STORAGE_KEY_V = 'wc_panel_left_w';
    const STORAGE_KEY_H = 'wc_panel_preview_h';
    const vHandle = document.getElementById('v-handle');
    const hHandle = document.getElementById('h-handle');
    const panelLeft = document.querySelector('.panel-left');
    const panelPreview = document.querySelector('.panel-preview');
    
    const savedW = localStorage.getItem(STORAGE_KEY_V);
    const savedH = localStorage.getItem(STORAGE_KEY_H);
    if (savedW) panelLeft.style.width = savedW + 'px';
    if (savedH) { panelPreview.style.height = savedH + 'px'; panelPreview.style.flexShrink = '0'; }
    
    let vActive = false, vX0, vW0;
    let hActive = false, hY0, hH0;
    
    vHandle.addEventListener('mousedown', e => {
      vActive = true; vX0 = e.clientX; vW0 = panelLeft.offsetWidth;
      vHandle.classList.add('active');
      document.body.style.cursor = 'col-resize';
      document.body.style.userSelect = 'none';
      e.preventDefault();
    });
    
    hHandle.addEventListener('mousedown', e => {
      hActive = true; hY0 = e.clientY; hH0 = panelPreview.offsetHeight;
      hHandle.classList.add('active');
      document.body.style.cursor = 'row-resize';
      document.body.style.userSelect = 'none';
      e.preventDefault();
    });
    
    document.addEventListener('mousemove', e => {
      if (vActive) panelLeft.style.width = Math.max(180, Math.min(520, vW0 + e.clientX - vX0)) + 'px';
      if (hActive) {
        const h = Math.max(60, Math.min(500, hH0 + e.clientY - hY0));
        panelPreview.style.height = h + 'px';
        panelPreview.style.flexShrink = '0';
      }
    });
    
    document.addEventListener('mouseup', () => {
      if (vActive) { vActive = false; vHandle.classList.remove('active'); document.body.style.cursor = ''; document.body.style.userSelect = ''; localStorage.setItem(STORAGE_KEY_V, panelLeft.offsetWidth); }
      if (hActive) { hActive = false; hHandle.classList.remove('active'); document.body.style.cursor = ''; document.body.style.userSelect = ''; localStorage.setItem(STORAGE_KEY_H, panelPreview.offsetHeight); }
    });
  }
};

// ── Init ───────────────────────────────────────────────────
window.addEventListener("load", () => {
  Simulation.init();
  DragDrop.init();
  ResizeHandler.init();
  Utils.log("=== Water Curtain Controller ===", "log-ok");
  
  // Detect protocol
  if (location.protocol === "https:") {
    document.getElementById("mqtt-picker").style.display = "block";
    document.getElementById("lan-form").style.display = "none";
    Utils.log("Truy cập từ Internet (HTTPS) — dùng MQTT để điều khiển", "log-warn");
  } else {
    document.getElementById("mqtt-picker").style.display = "none";
    document.getElementById("lan-form").style.display = "block";
    
    const savedIp = localStorage.getItem('selected_esp_ip');
    if (savedIp) {
      document.getElementById("ip").value = savedIp;
      Utils.log(`IP từ Home page: ${savedIp} — đang tự kết nối...`, "log-info");
      setTimeout(() => Connection.connectESP(), 400);
    }
  }
});

// ── Cascade Test Module ─────────────────────────────────
const Cascade = {
  delta(d) {
    const el = document.getElementById('cascade-boards');
    el.value = Math.max(1, Math.min(16, (parseInt(el.value) || 10) + d));
  },
  
  async run() {
    const boards = Math.max(1, Math.min(16, parseInt(document.getElementById('cascade-boards').value) || State.numBoards));
    const delayMs = parseInt(document.getElementById('cascade-delay').value) || 200;
    const totalBits = boards * 8;
    const mode = document.getElementById('cascade-mode').value;
    
    if (!State.connected) { Utils.log('Not connected', 'log-warn'); return; }
    
    if (mode === 'all') {
      Utils.log(`Test: ALL ON — ${boards} boards`, 'log-info');
      const allBits = new Uint8Array(State.CHIPS).fill(0xFF);
      if (State.connMode === 'mqtt') {
        await Connection.sendCommand('SET', { bits: Utils._toHex10(allBits) });
      }
      return;
    }
    
    const getBitsArr = bit => mode === 'cascade' ? this._cascadeBits(bit) : this._sequentialBits(bit);
    
    for (let bit = 0; bit < totalBits; bit++) {
      if (!State.connected) break;
      if (State.connMode === 'mqtt') {
        await Connection.sendCommand('SET', { bits: Utils._toHex10(getBitsArr(bit)) });
        await Utils.delay(delayMs);
      }
    }
  },
  
  _cascadeBits(currentBit) {
    const n = State.CHIPS || 10;
    const arr = new Uint8Array(n);
    for (let b = 0; b <= currentBit; b++) {
      const wireIdx = n - 1 - Math.floor(b / 8);
      const bitInByte = b % 8;
      if (wireIdx >= 0 && wireIdx < n) arr[wireIdx] |= (1 << bitInByte);
    }
    return arr;
  },
  
  _sequentialBits(currentBit) {
    const n = State.CHIPS || 10;
    const arr = new Uint8Array(n);
    const wireIdx = n - 1 - Math.floor(currentBit / 8);
    const bitInByte = currentBit % 8;
    if (wireIdx >= 0 && wireIdx < n) arr[wireIdx] |= (1 << bitInByte);
    return arr;
  }
};

// ── Pattern Generator Module ───────────────────────────────
const Pattern = {
  generate(type, rows, cols, density = 0.5) {
    const pattern = [];
    const center = { x: cols / 2, y: rows / 2 };
    
    for (let y = 0; y < rows; y++) {
      const row = new Uint8Array(Math.ceil(cols / 8));
      for (let x = 0; x < cols; x++) {
        let active = false;
        switch (type) {
          case "spiral":
            const dx = x - center.x, dy = y - center.y;
            const angle = Math.atan2(dy, dx), dist = Math.hypot(dx, dy);
            const spiral = (angle + dist * (Math.PI / 30)) % (Math.PI * 2);
            active = spiral < Math.PI * density;
            break;
          case "wave":
            const waveY = Math.sin(x * (2 * Math.PI * 2 / cols)) * (rows * 0.35);
            const band = Math.max(rows * 0.08, rows * density * 0.25);
            active = Math.abs(y - center.y - waveY) < band;
            break;
          case "striped":
            const sw = Math.max(1, Math.floor(cols * (1 - density)));
            active = Math.floor(x / 2) % (sw + 1) < 2;
            break;
          case "checkerboard":
            const bs = Math.max(1, Math.floor((cols * (1 - density)) / 2));
            active = (Math.floor(x / bs) + Math.floor(y / bs)) % 2 === 0;
            break;
          case "rain": active = Math.random() < density; break;
          case "diagonal": active = (x + y) % (Math.floor(cols * (1 - density)) + 1) < 2; break;
          case "pulse": active = Math.hypot(x - center.x, y - center.y) < Math.hypot(cols / 2, rows / 2) * density; break;
        }
        if (active) row[Math.floor(x / 8)] |= 1 << (x % 8);
      }
      // Reverse byte order for daisy-chain
      const rev = new Uint8Array(row.length);
      for (let i = 0; i < row.length; i++) rev[row.length - 1 - i] = row[i];
      pattern.push(rev);
    }
    return pattern;
  },
  
  updatePreview() {
    const type = document.getElementById("pattern-type").value;
    const density = parseInt(document.getElementById("pattern-density").value) / 100;
    const pattern = this.generate(type, 8, State.VALVES, density);
    if (pattern.length) {
      const firstRow = new Array(State.VALVES);
      for (let v = 0; v < State.VALVES; v++) {
        firstRow[v] = (pattern[0][State.CHIPS - 1 - Math.floor(v / 8)] >> (v % 8)) & 1;
      }
      Simulation.renderPreview(firstRow, `${type} — preview`);
    }
  }
};

// ── Frame Creator Module ───────────────────────────────────
const Creator = {
  CC_H: 9,
  
  init() {
    const canvas = document.getElementById('creator-canvas');
    if (!canvas) return;
    this.draw();
    this._attachEvents();
  },
  
  _attachEvents() {
    const canvas = document.getElementById('creator-canvas');
    
    canvas.addEventListener('pointerdown', e => {
      e.preventDefault();
      canvas.setPointerCapture(e.pointerId);
      if (!State.creatorRows.length) this.addRow();
      const { col, row } = this._getCell(e);
      if (row < 0 || row >= State.creatorRows.length) return;
      State.creatorSelRow = row;
      const byteIdx = col >> 3;
      const bit = 7 - (col & 7);
      const curOn = (State.creatorRows[row][byteIdx] >> bit) & 1;
      State._creatorDragState = { painting: curOn ? 0 : 1 };
      if (curOn) State.creatorRows[row][byteIdx] &= ~(1 << bit);
      else State.creatorRows[row][byteIdx] |= (1 << bit);
      this.draw();
    });
    
    canvas.addEventListener('pointermove', e => {
      if (!State._creatorDragState) return;
      const { col, row } = this._getCell(e);
      if (row < 0 || row >= State.creatorRows.length) return;
      State.creatorSelRow = row;
      const byteIdx = col >> 3;
      const bit = 7 - (col & 7);
      if (State._creatorDragState.painting === 1) State.creatorRows[row][byteIdx] |= (1 << bit);
      else State.creatorRows[row][byteIdx] &= ~(1 << bit);
      this.draw();
    });
  },
  
  _getCell(e) {
    const canvas = document.getElementById('creator-canvas');
    const rect = canvas.getBoundingClientRect();
    const scaleX = canvas.width / rect.width;
    const scaleY = canvas.height / rect.height;
    const px = (e.clientX - rect.left) * scaleX;
    const py = (e.clientY - rect.top) * scaleY;
    return { col: Math.floor(px / (canvas.width / State.VALVES)), row: Math.floor(py / this.CC_H) };
  },
  
  draw() {
    const canvas = document.getElementById('creator-canvas');
    if (!canvas) return;
    const nRows = Math.max(State.creatorRows.length, 1);
    canvas.width = State.VALVES;
    canvas.height = nRows * this.CC_H;
    const ctx = canvas.getContext('2d');
    ctx.fillStyle = '#010d1a';
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    
    for (let r = 0; r < State.creatorRows.length; r++) {
      const yTop = r * this.CC_H;
      if (r === State.creatorSelRow) {
        ctx.fillStyle = 'rgba(80,120,200,0.18)';
        ctx.fillRect(0, yTop, State.VALVES, this.CC_H);
      }
      for (let v = 0; v < State.VALVES; v++) {
        const byteIdx = v >> 3;
        const bit = 7 - (v & 7);
        if ((State.creatorRows[r][byteIdx] >> bit) & 1) {
          ctx.fillStyle = '#00bfff';
          ctx.fillRect(v, yTop + 1, 1, this.CC_H - 2);
        }
      }
    }
    
    if (State.creatorRows.length) {
      ctx.strokeStyle = 'rgba(100,160,255,0.6)';
      ctx.lineWidth = 1;
      ctx.strokeRect(0.5, State.creatorSelRow * this.CC_H + 0.5, State.VALVES - 1, this.CC_H - 1);
    }
    
    document.getElementById('creator-info').textContent = `${State.creatorRows.length} rows · row ${State.creatorSelRow + 1} sel`;
  },
  
  addRow() {
    const idx = State.creatorRows.length;
    State.creatorRows.splice(idx, 0, new Uint8Array(State.CHIPS));
    State.creatorSelRow = idx;
    this.draw();
  },
  
  dupRow() {
    if (!State.creatorRows.length) return;
    const copy = new Uint8Array(State.creatorRows[State.creatorSelRow]);
    State.creatorRows.splice(State.creatorSelRow + 1, 0, copy);
    State.creatorSelRow++;
    this.draw();
  },
  
  delRow() {
    if (!State.creatorRows.length) return;
    State.creatorRows.splice(State.creatorSelRow, 1);
    if (State.creatorSelRow >= State.creatorRows.length) State.creatorSelRow = Math.max(0, State.creatorRows.length - 1);
    this.draw();
  },
  
  clear() {
    State.creatorRows = [];
    State.creatorSelRow = 0;
    this.draw();
  }
};

// ── Admin Module ───────────────────────────────────────────
const Admin = {
  unlock() {
    const pw = document.getElementById('admin-pw').value;
    const err = document.getElementById('admin-pw-err');
    if (pw !== ADMIN_PASSWORD) {
      err.style.display = 'block';
      document.getElementById('admin-pw').style.borderColor = 'var(--danger)';
      return;
    }
    err.style.display = 'none';
    document.getElementById('admin-locked').style.display = 'none';
    document.getElementById('admin-unlocked').style.display = 'block';
    document.getElementById('admin-pw').value = '';
  },
  
  lock() {
    document.getElementById('admin-locked').style.display = 'block';
    document.getElementById('admin-unlocked').style.display = 'none';
  }
};

// ── Test Mode Module ───────────────────────────────────────
const TestMode = {
  buildUI() {
    const numBits = Math.min(parseInt(document.getElementById("test-bits").value) || State.VALVES, State.VALVES);
    const container = document.getElementById("test-checkboxes");
    container.innerHTML = "";
    
    for (let i = 0; i < numBits; i++) {
      const label = document.createElement("label");
      label.style.display = "flex";
      label.style.alignItems = "center";
      label.style.gap = "4px";
      label.style.fontSize = "11px";
      const cb = document.createElement("input");
      cb.type = "checkbox";
      cb.id = `test-bit-${i}`;
      cb.value = i;
      cb.onchange = this.updatePreview.bind(this);
      label.appendChild(cb);
      label.appendChild(document.createTextNode(`B${Math.floor(i/8)+1}:${i%8}`));
      container.appendChild(label);
    }
    document.getElementById("test-bits-container").style.display = "block";
  },
  
  updatePreview() {
    const bits = this.getBits();
    const row = new Array(State.VALVES);
    for (let x = 0; x < State.VALVES; x++) {
      const byteIdx = Math.floor(x / 8);
      row[x] = (bits[State.CHIPS - 1 - byteIdx] >> (x % 8)) & 1;
    }
    Simulation.renderPreview(row, 'test mode');
  },
  
  getBits() {
    const numBits = parseInt(document.getElementById("test-bits").value) || 8;
    const bits = new Uint8Array(State.CHIPS);
    for (let i = 0; i < numBits; i++) {
      const cb = document.getElementById(`test-bit-${i}`);
      if (cb && cb.checked) {
        bits[State.CHIPS - 1 - Math.floor(i / 8)] |= 1 << (i % 8);
      }
    }
    return bits;
  }
};

// Export modules to window for inline handlers
window.Connection = Connection;
window.ImageProcessor = ImageProcessor;
window.FrameBuilder = FrameBuilder;
window.Simulation = Simulation;
window.Mode = Mode;
window.Pump = Pump;
window.Cascade = Cascade;
window.Pattern = Pattern;
window.Creator = Creator;
window.Admin = Admin;
window.TestMode = TestMode;
window.AnimationInfo = AnimationInfo;

// ── Legacy wrapper functions (for inline handlers) ───────────
window.connectESP = () => Connection.connectESP();
window.disconnectESP = () => Connection.disconnectESP();
window.setMode = (mode) => Mode.set(mode);
window.setSoundPattern = (p) => Mode._setSoundPattern(p);
window.setEffectPattern = (p) => Mode._setEffectPattern(p);
window.toggleOpt = (k) => {
  State[`opt${k.charAt(0).toUpperCase() + k.slice(1)}`] = document.getElementById(`cb-${k}`).checked;
  ImageProcessor.reprocess();
};
window.onThr = () => ImageProcessor.onThr();
window.onDil = () => ImageProcessor.onDil();
window.loadFile = (input) => ImageProcessor.loadFile(input);
window.recalc = () => FrameBuilder.recalc();
window.updateSpeed = () => { State.playbackSpeed = parseFloat(document.getElementById('speed').value); document.getElementById('speed-lbl').textContent = State.playbackSpeed.toFixed(1) + 'x'; };
window.startStream = () => { if (State.frames.length) Connection._sendFrameStream(State.frames, FrameBuilder.getAdv(), parseInt(document.getElementById('loop-gap-ms')?.value) || 0); else Utils.log('Chưa load ảnh', 'log-warn'); };
window.stopAll = Simulation.stopAll;
window.testValves = async () => {
  if (!State.connected) { Utils.log('Not connected', 'log-warn'); return; }
  Utils.log('Testing valves...');
  Connection.sendReset();
  for (let v = 0; v < State.VALVES; v++) {
    const b = new Uint8Array(10);
    b[Math.floor(v / 8)] |= 1 << (v % 8);
    Connection.sendRaw(Utils.mkFrame(v * 120, b));
  }
};
window.cascadeBoardsDelta = (d) => Cascade.delta(d);
window.runCascadeTest = () => Cascade.run();
window.setTextDisplayMode = (m) => { State._textPerChar = m === 'char'; };
window.runText = () => Mode._runText();
window.stopText = () => Mode._stopText();
window.runClock = () => Mode._runClock();
window.stopClock = () => Mode._stopClock();
window.runEffect = () => Mode._runEffect();
window.stopEffect = () => Mode._stopEffect();
window.generateAndSendPattern = () => {
  const type = document.getElementById('pattern-type').value;
  const density = parseInt(document.getElementById('pattern-density').value) / 100;
  const numRows = parseInt(document.getElementById('pattern-rows').value) || 10;
  const frameMs = FrameBuilder.getAdv();
  State.creatorRows = Pattern.generate(type, numRows, State.VALVES, density);
  State.pixelRows = State.creatorRows;
  FrameBuilder.buildSequence();
  Utils.log(`Pattern ${type} generated`, 'log-ok');
};
window.simulatePatternOnly = () => {
  State.pixelRows = Pattern.generate(document.getElementById('pattern-type').value, 
    parseInt(document.getElementById('pattern-rows').value) || 10, State.VALVES, 
    parseInt(document.getElementById('pattern-density').value) / 100);
  FrameBuilder.buildSequence();
  Simulation.start();
};
window.loadPaintFrames = () => {
  try {
    const raw = localStorage.getItem('paint_frames');
    if (!raw) { Utils.log('No paint frames', 'log-warn'); return; }
    const data = JSON.parse(raw);
    State.pixelRows = data.frames.map(h => {
      const arr = new Uint8Array(State.CHIPS);
      for (let i = 0; i < State.CHIPS; i++) arr[i] = parseInt(h.slice(i*2, i*2+2), 16) || 0;
      return arr;
    });
    FrameBuilder.buildSequence();
  } catch(e) { Utils.log('Load error: ' + e.message, 'log-err'); }
};
window.startSimOnly = () => Simulation.start();
window.updateConfig = () => { State.numBoards = parseInt(document.getElementById('config-boards').value) || 1; State.CHIPS = State.numBoards; State.VALVES = State.numBoards * 8; document.getElementById('config-boards-val').textContent = State.numBoards; document.getElementById('config-valves-val').textContent = State.VALVES; };
window.toggleSec = (bodyId, chevId) => {
  const body = document.getElementById(bodyId);
  const chev = document.getElementById(chevId);
  if (body) { body.classList.toggle('collapsed'); if (chev) chev.classList.toggle('open', !body.classList.contains('collapsed')); }
};
window.fetchFwVersion = (ip) => Connection.fetchFwVersion(ip);
window.setPumpMode = (mode) => Pump.setMode(mode);
window.sendPumpCmd = (action) => Pump.cmd(action);
window.adminUnlock = () => Admin.unlock();
window.adminLock = () => Admin.lock();