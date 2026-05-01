#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <time.h>
#include <ArduinoJson.h>
#include <math.h>
#include "secrets.h"

#define MPU_ADDR    0x68
#define BATCH_SIZE  50
#define CAL_SAMPLES 200   // amostras usadas na calibração da gravidade

WiFiMulti wifiMulti;

float alpha = 0.2;
float Vf    = 0;

// ── Magnitude do baseline de gravidade (em LSB) ───────────────────────────────
// Calculada uma vez na calibração: média de √(ax² + ay² + az²) em repouso
// Usada para normalizar cada amostra → repouso ≈ 1.0, vibrações > 1.0
float grav_magnitude = 1.0;   // inicializado em 1.0 para evitar divisão por zero
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
// Calibração: captura N amostras parado e calcula a magnitude média de repouso
// Essa magnitude vira o divisor de todas as amostras futuras
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
}

// ─────────────────────────────────────────────────────────────────────────────
// Magnitude normalizada pela gravidade
// Repouso → ~1.0 | Vibração leve → ~1.05–1.1 | Vibração severa → >1.3
// Independente da orientação do sensor
// ─────────────────────────────────────────────────────────────────────────────
float calcularMagnitudeNorm(int16_t x, int16_t y, int16_t z) {
  float mag_bruta = sqrt((float)x*x + (float)y*y + (float)z*z);
  return mag_bruta / grav_magnitude;
}

// ─────────────────────────────────────────────────────────────────────────────
// Processa comandos recebidos via Serial
// CAL → recalibra a magnitude de repouso sem reiniciar
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

  // ── Acorda o MPU6050 ──────────────────────────────────────────────────────
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission();
  delay(100);  // aguarda sensor estabilizar

  // ── Calibração inicial ────────────────────────────────────────────────────
  calibrarGravidade(CAL_SAMPLES);
  Serial.println("Sistema pronto. Envie 'CAL' pela Serial para recalibrar.\n");
}

// ─────────────────────────────────────────────────────────────────────────────

void loop() {
  processarComandoSerial();

  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wi-Fi desconectado, reconectando...");
    delay(500);
    return;
  }

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

    // Magnitude normalizada: repouso ≈ 1.0, vibrações > 1.0
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
    sample["mag"] = V;   // magnitude normalizada

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

  // ── Monta payload final ───────────────────────────────────────────────────
  doc["ema"]  = Vf;
  doc["rms"]  = rms;
  doc["peak"] = peak;
  doc["std"]  = std_dev;

  serializeJson(doc, Serial);
  Serial.println();
}