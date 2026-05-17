#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <time.h>
#include <ArduinoJson.h>
#include <math.h>
#include "webui.h"

// ── MPU6050 ────────────────────────────────────────────────────────────────────
#define MPU_ADDR 0x68

// ── Defaults ───────────────────────────────────────────────────────────────────
#define DEF_BROKER    "192.168.1.1"
#define DEF_PORT      1883
#define DEF_USER      ""
#define DEF_PASS      ""
#define DEF_ID        "esp32-vibration"
#define DEF_T_FEAT    "vibration/esp32/features"
#define DEF_T_STATUS  "vibration/esp32/status"
#define DEF_T_CAL     "vibration/esp32/calibration"
#define DEF_BATCH     50
#define DEF_ALPHA     0.2f
#define DEF_DELAY     20
#define DEF_CAL_N     200
#define MAX_BATCH     500

// ── Objetos ────────────────────────────────────────────────────────────────────
Preferences  prefs;
WebServer    server(80);
WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);
WiFiManager  wm;

// ── Config em runtime ──────────────────────────────────────────────────────────
struct Config {
  char  broker[64];
  int   port;
  char  user[32];
  char  pass[64];
  char  id[32];
  char  t_feat[64];
  char  t_status[64];
  char  t_cal[64];
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

volatile bool req_cal         = false;
volatile bool req_restart     = false;
volatile bool req_wifi_reset  = false;

// ── Preferences ────────────────────────────────────────────────────────────────
void loadConfig() {
  prefs.begin("cfg", true);
  strlcpy(cfg.broker,   prefs.getString("broker",   DEF_BROKER).c_str(),   sizeof(cfg.broker));
  cfg.port = prefs.getInt("port", DEF_PORT);
  strlcpy(cfg.user,     prefs.getString("user",     DEF_USER).c_str(),     sizeof(cfg.user));
  strlcpy(cfg.pass,     prefs.getString("pass",     DEF_PASS).c_str(),     sizeof(cfg.pass));
  strlcpy(cfg.id,       prefs.getString("id",       DEF_ID).c_str(),       sizeof(cfg.id));
  strlcpy(cfg.t_feat,   prefs.getString("t_feat",   DEF_T_FEAT).c_str(),   sizeof(cfg.t_feat));
  strlcpy(cfg.t_status, prefs.getString("t_status", DEF_T_STATUS).c_str(), sizeof(cfg.t_status));
  strlcpy(cfg.t_cal,    prefs.getString("t_cal",    DEF_T_CAL).c_str(),    sizeof(cfg.t_cal));
  cfg.batch    = prefs.getInt  ("batch",    DEF_BATCH);
  cfg.alpha    = prefs.getFloat("alpha",    DEF_ALPHA);
  cfg.delay_ms = prefs.getInt  ("delay_ms", DEF_DELAY);
  cfg.cal_n    = prefs.getInt  ("cal_n",    DEF_CAL_N);
  prefs.end();
}

void saveConfig() {
  prefs.begin("cfg", false);
  prefs.putString("broker",   cfg.broker);
  prefs.putInt   ("port",     cfg.port);
  prefs.putString("user",     cfg.user);
  prefs.putString("pass",     cfg.pass);
  prefs.putString("id",       cfg.id);
  prefs.putString("t_feat",   cfg.t_feat);
  prefs.putString("t_status", cfg.t_status);
  prefs.putString("t_cal",    cfg.t_cal);
  prefs.putInt   ("batch",    cfg.batch);
  prefs.putFloat ("alpha",    cfg.alpha);
  prefs.putInt   ("delay_ms", cfg.delay_ms);
  prefs.putInt   ("cal_n",    cfg.cal_n);
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

  if (mqtt.connected()) {
    char msg[64];
    snprintf(msg, sizeof(msg), "{\"grav_magnitude\":%.2f}", grav_magnitude);
    mqtt.publish(cfg.t_cal, msg, true);
  }
}

float magnitudeNorm(int16_t x, int16_t y, int16_t z) {
  return sqrt((float)x*x + (float)y*y + (float)z*z) / grav_magnitude;
}

// ── MQTT ───────────────────────────────────────────────────────────────────────
void reconnectMQTT() {
  if (mqtt.connected()) return;
  const char *u = strlen(cfg.user) ? cfg.user : nullptr;
  const char *p = strlen(cfg.pass) ? cfg.pass : nullptr;
  if (mqtt.connect(cfg.id, u, p, cfg.t_status, 0, true, "offline")) {
    mqtt.publish(cfg.t_status, "online", true);
    Serial.println("MQTT conectado.");
  }
}

// ── Handlers Web ───────────────────────────────────────────────────────────────
void handleRoot() {
  server.send(200, "text/html", WEBUI_HTML);
}

void handleStatus() {
  StaticJsonDocument<192> doc;
  doc["ip"]     = WiFi.localIP().toString();
  doc["ssid"]   = WiFi.SSID();
  doc["mqtt"]   = mqtt.connected();
  doc["cal"]    = calibrado;
  doc["grav"]   = grav_magnitude;
  doc["uptime"] = millis() / 1000;
  char buf[192];
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
  StaticJsonDocument<512> doc;
  doc["mqtt_broker"]   = cfg.broker;
  doc["mqtt_port"]     = cfg.port;
  doc["mqtt_user"]     = cfg.user;
  doc["mqtt_pass"]     = cfg.pass;
  doc["mqtt_id"]       = cfg.id;
  doc["topic_feat"]    = cfg.t_feat;
  doc["topic_status"]  = cfg.t_status;
  doc["topic_cal"]     = cfg.t_cal;
  doc["batch_size"]    = cfg.batch;
  doc["alpha"]         = cfg.alpha;
  doc["sample_delay"]  = cfg.delay_ms;
  doc["cal_samples"]   = cfg.cal_n;
  char buf[512];
  serializeJson(doc, buf);
  server.send(200, "application/json", buf);
}

void handlePostMqtt() {
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "application/json", "{\"error\":\"json invalido\"}");
    return;
  }
  if (doc["mqtt_broker"].is<const char*>())  strlcpy(cfg.broker,   doc["mqtt_broker"],   sizeof(cfg.broker));
  if (doc["mqtt_port"].is<int>())            cfg.port = doc["mqtt_port"];
  if (doc["mqtt_user"].is<const char*>())    strlcpy(cfg.user,     doc["mqtt_user"],     sizeof(cfg.user));
  if (doc["mqtt_pass"].is<const char*>())    strlcpy(cfg.pass,     doc["mqtt_pass"],     sizeof(cfg.pass));
  if (doc["mqtt_id"].is<const char*>())      strlcpy(cfg.id,       doc["mqtt_id"],       sizeof(cfg.id));
  if (doc["topic_feat"].is<const char*>())   strlcpy(cfg.t_feat,   doc["topic_feat"],    sizeof(cfg.t_feat));
  if (doc["topic_status"].is<const char*>()) strlcpy(cfg.t_status, doc["topic_status"],  sizeof(cfg.t_status));
  if (doc["topic_cal"].is<const char*>())    strlcpy(cfg.t_cal,    doc["topic_cal"],     sizeof(cfg.t_cal));
  saveConfig();
  mqtt.setServer(cfg.broker, cfg.port);
  mqtt.disconnect();
  server.send(200, "application/json", "{\"ok\":true}");
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
  server.on("/api/config/mqtt",   HTTP_POST, handlePostMqtt);
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

  // WiFiManager: conecta ao Wi-Fi salvo ou abre AP de configuração
  wm.setConfigPortalTimeout(180);
  char apName[32];
  snprintf(apName, sizeof(apName), "VibracaoSensor-%06X",
           (uint32_t)(ESP.getEfuseMac() & 0xFFFFFF));

  if (!wm.autoConnect(apName)) {
    Serial.println("Falha no WiFiManager, reiniciando...");
    ESP.restart();
  }

  Serial.println("Wi-Fi: " + WiFi.localIP().toString() + " (" + WiFi.SSID() + ")");

  configTime(-3 * 3600, 0, "pool.ntp.org", "time.google.com");

  mqtt.setServer(cfg.broker, cfg.port);
  mqtt.setBufferSize(512);
  reconnectMQTT();

  // Wake up MPU6050
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission();
  delay(100);

  calibrarGravidade();

  setupServer();
  Serial.println("Web UI: http://" + WiFi.localIP().toString());
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
  mqtt.loop();

  if (req_restart)    { delay(100); ESP.restart(); }
  if (req_wifi_reset) { wm.resetSettings(); delay(100); ESP.restart(); }
  if (req_cal)        { req_cal = false; calibrarGravidade(); }

  if (WiFi.status() != WL_CONNECTED) { delay(500); return; }
  reconnectMQTT();

  // ── Coleta do batch ──────────────────────────────────────────────────────────
  int   bs        = constrain(cfg.batch, 10, MAX_BATCH);
  float soma      = 0, soma_q = 0, peak = 0, soma_d2 = 0;
  float mags[MAX_BATCH];

  for (int i = 0; i < bs; i++) {
    server.handleClient();
    mqtt.loop();

    int16_t ax, ay, az;
    lerMPU(ax, ay, az);
    float V = magnitudeNorm(ax, ay, az);
    Vf = cfg.alpha * V + (1.0f - cfg.alpha) * Vf;

    mags[i]  = V;
    soma     += V;
    soma_q   += V * V;
    if (V > peak) peak = V;

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

  Serial.printf("EMA=%.4f RMS=%.4f Peak=%.4f Std=%.4f\n", Vf, rms, peak, std_dev);

  if (mqtt.connected()) {
    StaticJsonDocument<128> feat;
    feat["ema"]  = serialized(String(Vf,      4));
    feat["rms"]  = serialized(String(rms,     4));
    feat["peak"] = serialized(String(peak,    4));
    feat["std"]  = serialized(String(std_dev, 4));
    char buf[128];
    serializeJson(feat, buf);
    mqtt.publish(cfg.t_feat, buf);
  }
}
