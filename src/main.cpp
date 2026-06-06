#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <time.h>
#include <ArduinoJson.h>
#include <math.h>
#include "webui.h"

// ── MPU6050 ────────────────────────────────────────────────────────────────────
#define MPU_ADDR 0x68

// ── API ────────────────────────────────────────────────────────────────────────
const char* DEVICE_TOKEN = "afa80c3c-a9f9-433c-a8b1-d5daadcc53c6";
const char* INGEST_URL   = "https://90a993df-87ae-4f5e-abc1-ad4a8dac7944-00-2rd0rgeso3kmq.picard.replit.dev/api/ingest";

// ── Defaults ───────────────────────────────────────────────────────────────────
#define DEF_BATCH   50
#define DEF_ALPHA   0.2f
#define DEF_DELAY   20
#define DEF_CAL_N   200
#define MAX_BATCH   500

// ── Objetos ────────────────────────────────────────────────────────────────────
Preferences prefs;
WebServer   server(80);
WiFiManager wm;

// Hostname mDNS derivado do MAC — único por dispositivo, permanente
// Formato: "vibra-XXXXXX" → acesso via http://vibra-XXXXXX.local
char deviceHostname[24];

// ── Config em runtime ──────────────────────────────────────────────────────────
struct Config {
  int   batch;
  float alpha;
  int   delay_ms;
  int   cal_n;
} cfg;

// ── Estado do sensor ───────────────────────────────────────────────────────────
struct Readings {
  float     ema, rms, peak, std_dev;
  bool      valid;
  long long ts_ms;
} last = {0, 0, 0, 0, false, 0};

float Vf             = 0;
float grav_magnitude = 1.0f;
bool  calibrado      = false;
bool  last_post_ok   = false;

volatile bool req_cal        = false;
volatile bool req_restart    = false;
volatile bool req_wifi_reset = false;

// ── Preferences ────────────────────────────────────────────────────────────────
void loadConfig() {
  prefs.begin("cfg", true);
  cfg.batch    = prefs.getInt  ("batch",    DEF_BATCH);
  cfg.alpha    = prefs.getFloat("alpha",    DEF_ALPHA);
  cfg.delay_ms = prefs.getInt  ("delay_ms", DEF_DELAY);
  cfg.cal_n    = prefs.getInt  ("cal_n",    DEF_CAL_N);
  prefs.end();
}

void saveConfig() {
  prefs.begin("cfg", false);
  prefs.putInt  ("batch",    cfg.batch);
  prefs.putFloat("alpha",    cfg.alpha);
  prefs.putInt  ("delay_ms", cfg.delay_ms);
  prefs.putInt  ("cal_n",    cfg.cal_n);
  prefs.end();
}

// ── MPU6050 ────────────────────────────────────────────────────────────────────
void lerMPU(int16_t &ax, int16_t &ay, int16_t &az) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)6, (bool)true);
  ax = Wire.read() << 8 | Wire.read();
  ay = Wire.read() << 8 | Wire.read();
  az = Wire.read() << 8 | Wire.read();
}

void calibrarGravidade() {
  Serial.printf("Calibrando com %d amostras — sensor parado...\n", cfg.cal_n);
  float soma = 0;
  for (int i = 0; i < cfg.cal_n; i++) {
    int16_t ax, ay, az;
    lerMPU(ax, ay, az);
    soma += sqrt((float)ax*ax + (float)ay*ay + (float)az*az);
    delay(5);
  }
  grav_magnitude = soma / cfg.cal_n;
  calibrado = true;
  Vf = 0;
  Serial.printf("Magnitude de repouso: %.2f LSB | repouso≈1.0 vibração>1.0\n", grav_magnitude);
}

float magnitudeNorm(int16_t x, int16_t y, int16_t z) {
  return sqrt((float)x*x + (float)y*y + (float)z*z) / grav_magnitude;
}

// ── HTTP POST ─────────────────────────────────────────────────────────────────
void postIngest(int16_t ax, int16_t ay, int16_t az) {
  char recorded_at[32];
  time_t now = time(nullptr);
  strftime(recorded_at, sizeof(recorded_at), "%Y-%m-%dT%H:%M:%S.000Z", gmtime(&now));

  StaticJsonDocument<320> doc;
  doc["device_token"] = DEVICE_TOKEN;
  doc["recorded_at"]  = recorded_at;
  JsonObject payload  = doc.createNestedObject("payload");
  payload["x"]    = ax;
  payload["y"]    = ay;
  payload["z"]    = az;
  payload["rms"]  = last.rms;
  payload["ema"]  = last.ema;
  payload["peak"] = last.peak;
  payload["std"]  = last.std_dev;

  char body[320];
  serializeJson(doc, body);

  WiFiClientSecure client;
  client.setInsecure();   // sem verificação de CA — adequado para dev/apresentação

  HTTPClient http;
  if (!http.begin(client, INGEST_URL)) {
    Serial.println("POST: falha ao iniciar conexão");
    last_post_ok = false;
    return;
  }

  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  int code = http.POST(body);
  if (code > 0) {
    last_post_ok = (code == 200 || code == 201);
    Serial.printf("POST %d | x=%d y=%d z=%d | %s\n", code, ax, ay, az, recorded_at);
    if (!last_post_ok) {
      Serial.println("Resp: " + http.getString());
    }
  } else {
    last_post_ok = false;
    Serial.printf("POST erro: %s\n", http.errorToString(code).c_str());
  }
  http.end();
}

// ── Handlers Web ───────────────────────────────────────────────────────────────
void handleRoot() {
  server.send(200, "text/html", WEBUI_HTML);
}

void handleStatus() {
  StaticJsonDocument<256> doc;
  doc["ip"]       = WiFi.localIP().toString();
  doc["hostname"] = deviceHostname;
  doc["ssid"]     = WiFi.SSID();
  doc["api_ok"]   = last_post_ok;
  doc["cal"]      = calibrado;
  doc["grav"]     = grav_magnitude;
  doc["uptime"]   = millis() / 1000;
  char buf[256];
  serializeJson(doc, buf);
  server.send(200, "application/json", buf);
}

void handleReadings() {
  if (!last.valid) { server.send(503, "application/json", "{}"); return; }
  StaticJsonDocument<128> doc;
  doc["ema"]  = last.ema;
  doc["rms"]  = last.rms;
  doc["peak"] = last.peak;
  doc["std"]  = last.std_dev;
  doc["ts"]   = last.ts_ms;
  char buf[128];
  serializeJson(doc, buf);
  server.send(200, "application/json", buf);
}

void handleGetConfig() {
  StaticJsonDocument<128> doc;
  doc["batch_size"]   = cfg.batch;
  doc["alpha"]        = cfg.alpha;
  doc["sample_delay"] = cfg.delay_ms;
  doc["cal_samples"]  = cfg.cal_n;
  char buf[128];
  serializeJson(doc, buf);
  server.send(200, "application/json", buf);
}

void handlePostSensor() {
  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "application/json", "{\"error\":\"json invalido\"}");
    return;
  }
  if (doc["batch_size"].is<int>())   cfg.batch    = constrain((int)doc["batch_size"],   10, MAX_BATCH);
  if (doc["sample_delay"].is<int>()) cfg.delay_ms = constrain((int)doc["sample_delay"], 5,  2000);
  if (doc["cal_samples"].is<int>())  cfg.cal_n    = constrain((int)doc["cal_samples"],  50, 2000);
  if (doc["alpha"].is<float>()) {
    cfg.alpha = constrain((float)doc["alpha"], 0.01f, 1.0f);
    Vf = 0;
  }
  saveConfig();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleCalibrate() {
  req_cal = true;
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleWifiReset() {
  server.send(200, "application/json", "{\"ok\":true}");
  req_wifi_reset = true;
}

void handleRestart() {
  server.send(200, "application/json", "{\"ok\":true}");
  req_restart = true;
}

void setupServer() {
  server.on("/",                  HTTP_GET,  handleRoot);
  server.on("/api/status",        HTTP_GET,  handleStatus);
  server.on("/api/readings",      HTTP_GET,  handleReadings);
  server.on("/api/config",        HTTP_GET,  handleGetConfig);
  server.on("/api/config/sensor", HTTP_POST, handlePostSensor);
  server.on("/api/calibrate",     HTTP_POST, handleCalibrate);
  server.on("/api/wifi/reset",    HTTP_POST, handleWifiReset);
  server.on("/api/restart",       HTTP_POST, handleRestart);
  server.begin();
}

// ── Setup ──────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  loadConfig();

  // Hostname derivado do MAC — calculado antes do portal para exibir no captive portal
  snprintf(deviceHostname, sizeof(deviceHostname), "vibra-%06x",
           (uint32_t)(ESP.getEfuseMac() & 0xFFFFFF));

  WiFi.setHostname(deviceHostname);   // aparece no DHCP do roteador

  // Exibe o domínio mDNS na página de configuração Wi-Fi
  char portalInfo[128];
  snprintf(portalInfo, sizeof(portalInfo),
           "<p style='text-align:center;font-size:14px'>"
           "Ap&oacute;s configurar, acesse:<br>"
           "<b>http://%s.local</b></p>", deviceHostname);
  wm.setCustomMenuHTML(portalInfo);
  wm.setConfigPortalTimeout(180);

  // AP com mesmo identificador do hostname → usuário infere o domínio pelo nome da rede
  if (!wm.autoConnect(deviceHostname)) {
    Serial.println("Falha no WiFiManager, reiniciando...");
    ESP.restart();
  }

  if (MDNS.begin(deviceHostname)) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS: http://" + String(deviceHostname) + ".local");
  } else {
    Serial.println("Erro ao iniciar mDNS");
  }

  Serial.println("Wi-Fi: " + WiFi.localIP().toString() + " (" + WiFi.SSID() + ")");

  configTime(0, 0, "pool.ntp.org", "time.google.com");
  // Aguarda sincronização NTP (necessário para recorded_at correto)
  Serial.print("Sincronizando NTP");
  time_t t = 0;
  while (t < 1000000000) { delay(200); time(&t); Serial.print('.'); }
  Serial.println(" OK");

  // Wake up MPU6050
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission();
  delay(100);

  calibrarGravidade();

  setupServer();
  Serial.println("═══════════════════════════════════════");
  Serial.println("  Acesse: http://" + String(deviceHostname) + ".local");
  Serial.println("  IP:     http://" + WiFi.localIP().toString());
  Serial.println("═══════════════════════════════════════");
  Serial.println("Pronto. Serial: CAL para recalibrar.");
}

// ── Loop ───────────────────────────────────────────────────────────────────────
void processSerial() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim(); cmd.toUpperCase();
  if (cmd == "CAL") req_cal = true;
  else Serial.println("Comandos: CAL");
}

void loop() {
  server.handleClient();
  processSerial();

  if (req_restart)    { delay(100); ESP.restart(); }
  if (req_wifi_reset) { wm.resetSettings(); delay(100); ESP.restart(); }
  if (req_cal)        { req_cal = false; calibrarGravidade(); }

  if (WiFi.status() != WL_CONNECTED) { delay(500); return; }

  // ── Coleta do batch ──────────────────────────────────────────────────────────
  int     bs      = constrain(cfg.batch, 10, MAX_BATCH);
  float   soma    = 0, soma_q = 0, peak = 0, soma_d2 = 0;
  float   mags[MAX_BATCH];
  int16_t last_ax = 0, last_ay = 0, last_az = 0;

  for (int i = 0; i < bs; i++) {
    server.handleClient();

    int16_t ax, ay, az;
    lerMPU(ax, ay, az);
    float V = magnitudeNorm(ax, ay, az);
    Vf = cfg.alpha * V + (1.0f - cfg.alpha) * Vf;

    mags[i]  = V;
    soma     += V;
    soma_q   += V * V;
    if (V > peak) peak = V;

    last_ax = ax; last_ay = ay; last_az = az;

    delay(cfg.delay_ms);
  }

  float media   = soma / bs;
  float rms     = sqrt(soma_q / bs);
  for (int i = 0; i < bs; i++) { float d = mags[i] - media; soma_d2 += d * d; }
  float std_dev = sqrt(soma_d2 / bs);

  struct timeval tv;
  gettimeofday(&tv, nullptr);
  last = { Vf, rms, peak, std_dev, true,
           (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000 };

  Serial.printf("EMA=%.4f RMS=%.4f Peak=%.4f Std=%.4f | x=%d y=%d z=%d\n",
                Vf, rms, peak, std_dev, last_ax, last_ay, last_az);

  postIngest(last_ax, last_ay, last_az);
}
