#include <Wire.h>
#include "I2Cdev.h"
#include "MPU6050.h"

#define X_OFFSET -40
#define Y_OFFSET -3174
#define Z_OFFSET 1328

#define SAMPLES 30

#define LED 5

MPU6050 accel;

int32_t samples[SAMPLES];
int set_next = 0;

void setup() {
  memset(samples, 0, 10 * sizeof(int32_t));
  Serial.begin(38400);
  Wire.begin();
  accel.initialize();
  if (accel.testConnection()) Serial.write("Accelerometer initialized!");
  accel.setXAccelOffset(X_OFFSET);
  accel.setYAccelOffset(Y_OFFSET);
  accel.setZAccelOffset(Z_OFFSET);
  
  accel.setFullScaleAccelRange(0);
  
  pinMode(LED, OUTPUT);
}


void loop() {
  int16_t x, y, z;
 
  accel.getAcceleration(&x, &y, &z);
  
  int32_t net_accel = sqrt((int32_t)x*x + (int32_t)y*y + (int32_t)z*z);
  samples[set_next] = net_accel;
  set_next++;
  if (set_next == SAMPLES) set_next = 0;
  
  int avg = 0;
  for (int i = 0; i < SAMPLES; i++) {
    avg += samples[i] / SAMPLES;
  }
  
  analogWrite(LED, abs(avg - 16380) / 40 * 2);
  
  delay(10);
}
