#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <time.h>
#include <ArduinoJson.h>
#include "secrets.h"

#define MPU_ADDR   0x68
#define BATCH_SIZE 50

WiFiMulti wifiMulti;

float alpha = 0.2;
float Vf    = 0;

float calcularMagnitude(int16_t x, int16_t y, int16_t z) {
  return sqrt(x * x + y * y + z * z);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  // Registra todas as redes do secrets.h
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

  configTime(-3 * 3600, 0, "pool.ntp.org", "time.google.com");
  Serial.print("Sincronizando NTP");
  struct tm t;
  while (!getLocalTime(&t)) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nNTP sincronizado.");

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission();
}

void loop() {
  // Reconecta automaticamente se cair
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wi-Fi desconectado, reconectando...");
    delay(500);
    return;
  }

  DynamicJsonDocument doc(6144);
  JsonArray batch = doc.createNestedArray("batch");

  for (int i = 0; i < BATCH_SIZE; i++) {
    int16_t AcX, AcY, AcZ;

    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x3B);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)6, (bool)true);

    AcX = Wire.read() << 8 | Wire.read();
    AcY = Wire.read() << 8 | Wire.read();
    AcZ = Wire.read() << 8 | Wire.read();

    float V = calcularMagnitude(AcX, AcY, AcZ);
    Vf = alpha * V + (1 - alpha) * Vf;

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

  doc["ema"] = Vf;

  serializeJson(doc, Serial);
  Serial.println();
}