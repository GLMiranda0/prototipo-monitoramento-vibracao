#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <PubSubClient.h>
#include <time.h>
#include <ArduinoJson.h>
#include <math.h>
#include "secrets.h"

#define MPU_ADDR    0x68
#define BATCH_SIZE  50
#define CAL_SAMPLES 200

// ── Tópicos MQTT ──────────────────────────────────────────────────────────────
#define TOPIC_FEATURES  "vibration/esp32/features"
#define TOPIC_STATUS    "vibration/esp32/status"
#define TOPIC_CAL       "vibration/esp32/calibration"

WiFiMulti  wifiMulti;
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

float alpha = 0.2;
float Vf    = 0;

// ── Baseline de gravidade ─────────────────────────────────────────────────────
float grav_magnitude = 1.0;
bool  calibrado      = false;

// ─────────────────────────────────────────────────────────────────────────────
// Lê um único sample do MPU6050
// ─────────────────────────────────────────────────────────────────────────────
void lerMPU(int16_t &ax, int16_t &ay, int16_t &az) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)6, (bool)true);
  ax = Wire.read() << 8 | Wire.read();
  ay = Wire.read() << 8 | Wire.read();
  az = Wire.read() << 8 | Wire.read();
}

// ─────────────────────────────────────────────────────────────────────────────
// Calibração: calcula a magnitude média de repouso
// ─────────────────────────────────────────────────────────────────────────────
void calibrarGravidade(int amostras = CAL_SAMPLES) {
  Serial.printf("Calibrando gravidade com %d amostras — mantenha o sensor parado...\n", amostras);

  float soma_mag = 0;
  for (int i = 0; i < amostras; i++) {
    int16_t ax, ay, az;
    lerMPU(ax, ay, az);
    soma_mag += sqrt((float)ax*ax + (float)ay*ay + (float)az*az);
    delay(5);
  }

  grav_magnitude = soma_mag / amostras;
  calibrado = true;

  Serial.printf("Magnitude de repouso calibrada: %.2f LSB\n", grav_magnitude);
  Serial.println("Em operacao normal: repouso ≈ 1.0 | vibracao > 1.0");

  // Publica confirmação no tópico de calibração
  if (mqtt.connected()) {
    char msg[64];
    snprintf(msg, sizeof(msg), "{\"grav_magnitude\":%.2f}", grav_magnitude);
    mqtt.publish(TOPIC_CAL, msg, true);  // retained=true
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Magnitude normalizada pela gravidade
// ─────────────────────────────────────────────────────────────────────────────
float calcularMagnitudeNorm(int16_t x, int16_t y, int16_t z) {
  float mag_bruta = sqrt((float)x*x + (float)y*y + (float)z*z);
  return mag_bruta / grav_magnitude;
}

// ─────────────────────────────────────────────────────────────────────────────
// Reconexão MQTT (sem bloquear o loop)
// ─────────────────────────────────────────────────────────────────────────────
void reconnectMQTT() {
  if (mqtt.connected()) return;

  Serial.print("Conectando ao broker MQTT...");
  // Last Will: marca o dispositivo como offline se a conexão cair
  if (mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD, TOPIC_STATUS, 0, true, "offline")) {
    Serial.println(" conectado.");
    mqtt.publish(TOPIC_STATUS, "online", true);  // retained=true
  } else {
    Serial.printf(" falhou (rc=%d), tentando novamente no próximo batch.\n", mqtt.state());
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Processa comandos recebidos via Serial
// CAL → recalibra sem reiniciar
// ─────────────────────────────────────────────────────────────────────────────
void processarComandoSerial() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toUpperCase();

  if (cmd == "CAL") {
    calibrarGravidade(CAL_SAMPLES);
  } else {
    Serial.println("Comando desconhecido. Comandos disponíveis: CAL");
  }
}

// ─────────────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  // ── Wi-Fi ─────────────────────────────────────────────────────────────────
  const char* networks[][2] = WIFI_NETWORKS;
  int count = sizeof(networks) / sizeof(networks[0]);
  for (int i = 0; i < count; i++) {
    wifiMulti.addAP(networks[i][0], networks[i][1]);
  }

  Serial.print("Conectando ao Wi-Fi");
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi conectado: " + WiFi.localIP().toString());
  Serial.println("Rede: " + WiFi.SSID());

  // ── NTP ───────────────────────────────────────────────────────────────────
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.google.com");
  Serial.print("Sincronizando NTP");
  struct tm t;
  while (!getLocalTime(&t)) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nNTP sincronizado.");

  // ── MQTT ──────────────────────────────────────────────────────────────────
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setBufferSize(512);   // suficiente para o JSON de features
  reconnectMQTT();

  // ── MPU6050 ───────────────────────────────────────────────────────────────
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission();
  delay(100);

  calibrarGravidade(CAL_SAMPLES);
  Serial.println("Sistema pronto. Envie 'CAL' pela Serial para recalibrar.\n");
}

// ─────────────────────────────────────────────────────────────────────────────

void loop() {
  processarComandoSerial();
  mqtt.loop();  // mantém a conexão MQTT viva

  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wi-Fi desconectado, reconectando...");
    delay(500);
    return;
  }

  reconnectMQTT();  // reconecta se necessário, sem bloquear

  DynamicJsonDocument doc(6144);
  JsonArray batch = doc.createNestedArray("batch");

  // Acumuladores para features
  float soma_quadrados = 0;
  float peak           = 0;
  float soma           = 0;
  float soma_quad_dev  = 0;
  float mags[BATCH_SIZE];

  // ── Coleta do batch ───────────────────────────────────────────────────────
  for (int i = 0; i < BATCH_SIZE; i++) {
    int16_t AcX, AcY, AcZ;
    lerMPU(AcX, AcY, AcZ);

    float V = calcularMagnitudeNorm(AcX, AcY, AcZ);
    Vf = alpha * V + (1 - alpha) * Vf;

    mags[i]         = V;
    soma           += V;
    soma_quadrados += V * V;
    if (V > peak) peak = V;

    struct timeval tv;
    gettimeofday(&tv, nullptr);
    long long ts_ms = (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;

    JsonObject sample = batch.createNestedObject();
    sample["ts"]  = ts_ms;
    sample["ax"]  = AcX;
    sample["ay"]  = AcY;
    sample["az"]  = AcZ;
    sample["mag"] = V;

    delay(20);
  }

  // ── Cálculo das features ──────────────────────────────────────────────────
  float media = soma / BATCH_SIZE;
  float rms   = sqrt(soma_quadrados / BATCH_SIZE);

  for (int i = 0; i < BATCH_SIZE; i++) {
    float dev = mags[i] - media;
    soma_quad_dev += dev * dev;
  }
  float std_dev = sqrt(soma_quad_dev / BATCH_SIZE);

  // ── Monta payload Serial (batch completo) ─────────────────────────────────
  doc["ema"]  = Vf;
  doc["rms"]  = rms;
  doc["peak"] = peak;
  doc["std"]  = std_dev;

  serializeJson(doc, Serial);
  Serial.println();

  // ── Publica features via MQTT ─────────────────────────────────────────────
  if (mqtt.connected()) {
    StaticJsonDocument<128> feat;
    feat["ema"]  = serialized(String(Vf,    4));
    feat["rms"]  = serialized(String(rms,   4));
    feat["peak"] = serialized(String(peak,  4));
    feat["std"]  = serialized(String(std_dev, 4));

    char buf[128];
    serializeJson(feat, buf);
    mqtt.publish(TOPIC_FEATURES, buf);
  }
}