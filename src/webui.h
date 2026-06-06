#pragma once

const char WEBUI_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Sensor de Vibração</title>
<style>
:root {
  --navy:   #1C2E54;
  --orange: #E97420;
  --orange-d: #C55F0F;
  --bg:     #EEF0F5;
  --surface:#FFFFFF;
  --surf2:  #F6F7FA;
  --text:   #1C2E54;
  --muted:  #64748B;
  --border: #DDE3EE;
  --green:  #0A9B6B;
  --red:    #D63B3B;
  --r:      10px;
  --sh:     0 1px 4px rgba(28,46,84,.09), 0 4px 16px rgba(28,46,84,.06);
}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;
     background:var(--bg);color:var(--text);min-height:100vh}

/* ── Header ── */
header{
  background:var(--navy);color:#fff;
  height:60px;padding:0 24px;
  display:flex;align-items:center;justify-content:space-between;
  box-shadow:0 2px 10px rgba(28,46,84,.35);
  position:sticky;top:0;z-index:100;
}
.brand{display:flex;align-items:center;gap:12px}
.brand-mark{
  width:34px;height:34px;border-radius:8px;
  background:var(--orange);
  display:flex;align-items:center;justify-content:center;
}
.brand-mark svg{width:20px;height:20px;fill:none;stroke:#fff;stroke-width:2.2;stroke-linecap:round}
.brand-text h1{font-size:16px;font-weight:700;letter-spacing:.02em}
.brand-text p{font-size:11px;opacity:.6;margin-top:1px}
.mqtt-badge{
  display:flex;align-items:center;gap:7px;
  background:rgba(255,255,255,.1);border-radius:20px;
  padding:5px 13px;font-size:12px;font-weight:500;
}
.dot{width:7px;height:7px;border-radius:50%;background:#6B7280;flex-shrink:0}
.dot.on{background:#34D399;box-shadow:0 0 0 3px rgba(52,211,153,.25)}
.dot.off{background:#F87171}

/* ── Layout ── */
main{
  max-width:860px;margin:0 auto;padding:24px 16px;
  display:grid;grid-template-columns:1fr 1fr;gap:18px;
}
@media(max-width:620px){main{grid-template-columns:1fr}}
.full{grid-column:1/-1}

/* ── Card ── */
.card{background:var(--surface);border-radius:var(--r);box-shadow:var(--sh);padding:22px}
.card-head{
  display:flex;align-items:center;gap:10px;
  padding-bottom:14px;margin-bottom:16px;
  border-bottom:1px solid var(--border);
}
.card-icon{
  width:30px;height:30px;border-radius:7px;
  background:rgba(233,116,32,.1);
  display:flex;align-items:center;justify-content:center;
  flex-shrink:0;
}
.card-icon svg{width:16px;height:16px;fill:none;stroke:var(--orange);stroke-width:2.2;stroke-linecap:round;stroke-linejoin:round}
.card-head h2{font-size:14px;font-weight:700;letter-spacing:.01em}

/* ── Leituras ── */
.metrics{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-bottom:14px}
@media(max-width:480px){.metrics{grid-template-columns:repeat(2,1fr)}}
.metric{
  background:var(--surf2);border:1px solid var(--border);
  border-radius:8px;padding:14px 10px;text-align:center;
  transition:border-color .2s;
}
.metric .lbl{font-size:10px;font-weight:700;text-transform:uppercase;letter-spacing:.07em;color:var(--muted);margin-bottom:7px}
.metric .val{font-size:22px;font-weight:800;color:var(--navy);font-variant-numeric:tabular-nums;line-height:1}
.metric .unit{font-size:10px;color:var(--muted);margin-top:4px}
.metric.hi .val{color:var(--red)}
.metric.ok .val{color:var(--green)}

.readings-footer{display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:10px}
.ts{font-size:11px;color:var(--muted)}

/* ── Form ── */
.fg{margin-bottom:12px}
label{display:block;font-size:12px;font-weight:600;color:var(--muted);margin-bottom:5px;letter-spacing:.02em}
input[type=text],input[type=number],input[type=password]{
  width:100%;padding:8px 11px;
  border:1.5px solid var(--border);border-radius:7px;
  font-size:13px;color:var(--text);background:var(--surf2);
  outline:none;transition:border-color .15s,background .15s;
}
input:focus{border-color:var(--orange);background:#fff}
.row2{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.sec{font-size:10px;font-weight:800;text-transform:uppercase;letter-spacing:.08em;color:var(--muted);margin:16px 0 10px}

/* ── Slider alpha ── */
.slider-wrap{display:flex;align-items:center;gap:10px}
input[type=range]{flex:1;accent-color:var(--orange);height:4px}
.alpha-num{width:64px;text-align:center}

/* ── Botões ── */
.btn{
  display:inline-flex;align-items:center;justify-content:center;gap:6px;
  padding:8px 16px;border-radius:7px;font-size:13px;font-weight:700;
  border:none;cursor:pointer;transition:all .15s;
}
.btn-primary{background:var(--orange);color:#fff}
.btn-primary:hover{background:var(--orange-d)}
.btn-ghost{background:var(--surf2);color:var(--text);border:1.5px solid var(--border)}
.btn-ghost:hover{border-color:var(--orange);color:var(--orange)}
.btn-danger{background:transparent;color:var(--red);border:1.5px solid var(--red)}
.btn-danger:hover{background:var(--red);color:#fff}
.btn:disabled{opacity:.5;cursor:not-allowed}
.actions{display:flex;gap:8px;margin-top:16px;flex-wrap:wrap}

hr.div{border:none;border-top:1px solid var(--border);margin:16px 0}

/* ── Spinner ── */
.spin{display:inline-block;width:13px;height:13px;
  border:2px solid rgba(255,255,255,.3);border-top-color:#fff;
  border-radius:50%;animation:spin .6s linear infinite}
@keyframes spin{to{transform:rotate(360deg)}}

/* ── Toast ── */
#toast{
  position:fixed;bottom:24px;right:24px;
  background:var(--navy);color:#fff;
  padding:11px 18px;border-radius:8px;font-size:13px;font-weight:600;
  box-shadow:0 4px 20px rgba(0,0,0,.2);
  transform:translateY(70px);opacity:0;
  transition:all .28s cubic-bezier(.34,1.56,.64,1);
  z-index:999;pointer-events:none;
}
#toast.show{transform:translateY(0);opacity:1}
#toast.ok{border-left:4px solid var(--green)}
#toast.err{border-left:4px solid var(--red)}

/* ── Info rows (wifi) ── */
.info{display:flex;justify-content:space-between;align-items:center;
  padding:9px 0;border-bottom:1px solid var(--border);font-size:13px}
.info:last-child{border:none}
.info .k{color:var(--muted)}
.info .v{font-weight:600}

/* ── Hint ── */
.hint{font-size:11px;color:var(--muted);margin-top:4px;line-height:1.4}
</style>
</head>
<body>

<header>
  <div class="brand">
    <div class="brand-mark">
      <!-- wave/vibration icon -->
      <svg viewBox="0 0 24 24"><polyline points="2,12 5,6 8,18 11,9 14,15 17,3 20,12 22,12"/></svg>
    </div>
    <div class="brand-text">
      <h1>Sensor de Vibra&ccedil;&atilde;o</h1>
      <p id="ip-lbl">Carregando&hellip;</p>
    </div>
  </div>
  <div class="mqtt-badge">
    <div class="dot" id="dot"></div>
    <span id="api-lbl">API</span>
  </div>
</header>

<main>

  <!-- ── Última leitura ── -->
  <div class="card full">
    <div class="card-head">
      <div class="card-icon">
        <svg viewBox="0 0 24 24"><polyline points="22,12 18,12 15,21 9,3 6,12 2,12"/></svg>
      </div>
      <h2>Última Leitura</h2>
    </div>
    <div class="metrics">
      <div class="metric" id="box-ema">
        <div class="lbl">EMA</div>
        <div class="val" id="v-ema">—</div>
        <div class="unit">adimensional</div>
      </div>
      <div class="metric" id="box-rms">
        <div class="lbl">RMS</div>
        <div class="val" id="v-rms">—</div>
        <div class="unit">adimensional</div>
      </div>
      <div class="metric" id="box-peak">
        <div class="lbl">Pico</div>
        <div class="val" id="v-peak">—</div>
        <div class="unit">adimensional</div>
      </div>
      <div class="metric" id="box-std">
        <div class="lbl">Desvio Padr&atilde;o</div>
        <div class="val" id="v-std">—</div>
        <div class="unit">adimensional</div>
      </div>
    </div>
    <div class="readings-footer">
      <span class="ts" id="ts">Aguardando primeira leitura&hellip;</span>
      <div style="display:flex;gap:8px">
        <button class="btn btn-ghost" onclick="loadReadings()">&#8635; Atualizar</button>
        <button class="btn btn-primary" id="btn-cal" onclick="calibrate()">&#9878; Calibrar</button>
      </div>
    </div>
  </div>

  <!-- ── Sensor + Sistema ── -->
  <div class="card">
    <div class="card-head">
      <div class="card-icon">
        <svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="3"/><path d="M19.07 4.93a10 10 0 010 14.14M4.93 4.93a10 10 0 000 14.14"/></svg>
      </div>
      <h2>Sensor</h2>
    </div>

    <div class="row2">
      <div class="fg">
        <label>Tamanho do Batch</label>
        <input type="number" id="batch_size" min="10" max="500">
      </div>
      <div class="fg">
        <label>Intervalo (ms)</label>
        <input type="number" id="sample_delay" min="5" max="2000">
      </div>
    </div>
    <div class="fg">
      <label>Amostras de Calibra&ccedil;&atilde;o</label>
      <input type="number" id="cal_samples" min="50" max="2000">
    </div>
    <div class="fg">
      <label>K do EMA (&alpha;)</label>
      <div class="slider-wrap">
        <input type="range" id="alpha_r" min="0.01" max="1" step="0.01"
          oninput="syncAlpha(this.value)">
        <input type="number" id="alpha" class="alpha-num" min="0.01" max="1" step="0.01"
          oninput="syncAlpha(this.value)">
      </div>
      <div class="hint">Menor = mais suave &bull; Maior = mais reativo</div>
    </div>

    <div class="actions">
      <button class="btn btn-primary" onclick="saveSensor()">Salvar Sensor</button>
    </div>

    <hr class="div">
    <div class="sec">Wi-Fi &amp; Sistema</div>

    <div class="info">
      <span class="k">Rede conectada</span>
      <span class="v" id="ssid-lbl">—</span>
    </div>
    <div class="info">
      <span class="k">Endere&ccedil;o IP</span>
      <span class="v" id="ip-lbl2">—</span>
    </div>
    <div class="info">
      <span class="k">Nome local (mDNS)</span>
      <a class="v" id="mdns-lbl" href="#" target="_blank" style="color:var(--orange);text-decoration:none">—</a>
    </div>

    <div class="actions" style="margin-top:14px">
      <button class="btn btn-ghost" onclick="resetWifi()">Redefinir Wi-Fi</button>
      <button class="btn btn-danger" onclick="restartDevice()">Reiniciar</button>
    </div>
  </div>

</main>

<div id="toast"></div>

<script>
function syncAlpha(v) {
  v = Math.min(1, Math.max(0.01, parseFloat(v) || 0.01));
  document.getElementById('alpha').value   = v.toFixed(2);
  document.getElementById('alpha_r').value = v;
}

async function api(path, opts) {
  const r = await fetch(path, opts);
  if (!r.ok) { const t = await r.text(); throw new Error(t || r.status); }
  return r.json();
}

let toastTimer;
function toast(msg, type='ok') {
  const el = document.getElementById('toast');
  el.textContent = msg;
  el.className = 'show ' + type;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => el.className = '', 3000);
}

function setMetric(id, boxId, val) {
  const el = document.getElementById(id);
  const box = document.getElementById(boxId);
  el.textContent = val.toFixed(4);
  box.className = 'metric' + (val > 1.5 ? ' hi' : val < 1.1 ? ' ok' : '');
}

async function loadReadings() {
  try {
    const d = await api('/api/readings');
    setMetric('v-ema',  'box-ema',  d.ema);
    setMetric('v-rms',  'box-rms',  d.rms);
    setMetric('v-peak', 'box-peak', d.peak);
    setMetric('v-std',  'box-std',  d.std);
    const t = new Date(d.ts);
    document.getElementById('ts').textContent =
      'Atualizado: ' + t.toLocaleTimeString('pt-BR');
  } catch(e) {
    if (e.message !== '503') document.getElementById('ts').textContent = 'Sem dados ainda';
  }
}

async function loadStatus() {
  try {
    const s = await api('/api/status');
    document.getElementById('ip-lbl').textContent   = s.ip;
    document.getElementById('ip-lbl2').textContent  = s.ip;
    document.getElementById('ssid-lbl').textContent = s.ssid;
    const dot = document.getElementById('dot');
    const lbl = document.getElementById('api-lbl');
    dot.className = 'dot ' + (s.api_ok ? 'on' : 'off');
    lbl.textContent = s.api_ok ? 'API ok' : 'API erro';
    if (s.hostname) {
      const mdns = document.getElementById('mdns-lbl');
      const url = 'http://' + s.hostname + '.local';
      mdns.textContent = s.hostname + '.local';
      mdns.href = url;
    }
  } catch(e) {}
}

async function loadConfig() {
  try {
    const c = await api('/api/config');
    ['batch_size','sample_delay','cal_samples'].forEach(k => {
      const el = document.getElementById(k);
      if (el && c[k] !== undefined) el.value = c[k];
    });
    syncAlpha(c.alpha ?? 0.2);
  } catch(e) {}
}

async function saveSensor() {
  const body = {
    batch_size:   Number(document.getElementById('batch_size').value),
    sample_delay: Number(document.getElementById('sample_delay').value),
    cal_samples:  Number(document.getElementById('cal_samples').value),
    alpha:        parseFloat(document.getElementById('alpha').value),
  };
  try {
    await api('/api/config/sensor', {method:'POST',
      headers:{'Content-Type':'application/json'}, body:JSON.stringify(body)});
    toast('Configurações do sensor salvas!');
  } catch(e) { toast('Erro: ' + e.message, 'err'); }
}

async function calibrate() {
  const btn = document.getElementById('btn-cal');
  btn.disabled = true;
  btn.innerHTML = '<span class="spin"></span> Calibrando...';
  try {
    await api('/api/calibrate', {method:'POST'});
    toast('Calibração iniciada! Mantenha o sensor parado.');
  } catch(e) { toast('Erro: ' + e.message, 'err'); }
  setTimeout(() => { btn.disabled=false; btn.innerHTML='&#9878; Calibrar'; }, 6000);
}

async function resetWifi() {
  if (!confirm('Apagará o Wi-Fi salvo e reiniciará em modo AP.\nContinuar?')) return;
  try {
    await api('/api/wifi/reset', {method:'POST'});
    toast('Wi-Fi redefinido. Conecte ao AP do dispositivo.');
  } catch(e) { toast('Erro: ' + e.message, 'err'); }
}

async function restartDevice() {
  if (!confirm('Reiniciar o dispositivo?')) return;
  try {
    await api('/api/restart', {method:'POST'});
    toast('Reiniciando...');
  } catch(e) { toast('Erro: ' + e.message, 'err'); }
}

// Init
loadConfig();
loadStatus();
loadReadings();
setInterval(loadReadings, 5000);
setInterval(loadStatus,   10000);
</script>

</body>
</html>
)rawliteral";
