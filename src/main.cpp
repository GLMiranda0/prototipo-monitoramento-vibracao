#include <Arduino.h>
#include <Wire.h>

#define MPU_ADDR 0x68

float alpha = 0.2;
float Vf = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  // Inicializa MPU6050
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission();
}

float calcularMagnitude(int16_t x, int16_t y, int16_t z) {
  return sqrt(x * x + y * y + z * z);
}

void loop() {
  int16_t AcX, AcY, AcZ;

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);

  AcX = Wire.read() << 8 | Wire.read();
  AcY = Wire.read() << 8 | Wire.read();
  AcZ = Wire.read() << 8 | Wire.read();

  float V = calcularMagnitude(AcX, AcY, AcZ);

  // Filtro EMA
  Vf = alpha * V + (1 - alpha) * Vf;

  Serial.print("Vibracao: ");
  Serial.println(Vf);

  delay(50);
}