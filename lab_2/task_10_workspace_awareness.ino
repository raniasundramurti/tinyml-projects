#include <PDM.h>
#include <Arduino_BMI270_BMM150.h>
#include <Arduino_APDS9960.h>

// mic setup
short sampleBuffer[256];
volatile int samplesRead = 0;

void onPDMdata() {
  int bytesAvailable = PDM.available();
  PDM.read(sampleBuffer, bytesAvailable);
  samplesRead = bytesAvailable / 2;
}

// Thresholds
#define MIC_THRESHOLD     50
#define LIGHT_THRESHOLD   150
#define MOTION_THRESHOLD  0.4
#define PROX_THRESHOLD    100

void setup() {
  Serial.begin(115200);
  delay(1500);

  PDM.onReceive(onPDMdata);
  if (!PDM.begin(1, 16000)) {
    Serial.println("Failed to start PDM.");
    while (1);
  }

  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU.");
    while (1);
  }

  if (!APDS.begin()) {
    Serial.println("Failed to initialize APDS9960.");
    while (1);
  }

  Serial.println("Smart Workspace Classifier started");
}

void loop() {
  // Mic
  int micLevel = 0;
  if (samplesRead) {
    long sum = 0;
    for (int i = 0; i < samplesRead; i++) {
      sum += abs(sampleBuffer[i]);
    }
    micLevel = sum / samplesRead;
    samplesRead = 0;
  }

  // Light/proximity
  int clearVal = 0;
  int proximity = 255;
  int r, g, b, c;
  if (APDS.colorAvailable()) {
    APDS.readColor(r, g, b, c);
    clearVal = c;
  }
  if (APDS.proximityAvailable()) {
    proximity = APDS.readProximity();
  }

  // IMU
  float ax, ay, az;
  float motionMetric = 0;
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(ax, ay, az);
    motionMetric = abs(ax) + abs(ay) + abs(az - 1.0);
  }

  // Decision-making
  bool sound   = micLevel > MIC_THRESHOLD;
  bool dark    = clearVal < LIGHT_THRESHOLD;
  bool moving  = motionMetric > MOTION_THRESHOLD;
  bool near    = proximity < PROX_THRESHOLD;

  // Printing raw values
  Serial.print("raw,mic="); Serial.print(micLevel);
  Serial.print(",clear="); Serial.print(clearVal);
  Serial.print(",motion="); Serial.print(motionMetric, 3);
  Serial.print(",prox="); Serial.println(proximity);

  // Printing flags
  Serial.print("flags,sound="); Serial.print(sound);
  Serial.print(",dark="); Serial.print(dark);
  Serial.print(",moving="); Serial.print(moving);
  Serial.print(",near="); Serial.println(near);

  // Classifying
  String label = "UNKNOWN";

  if (!sound && !dark && !moving && !near) {
    label = "QUIET_BRIGHT_STEADY_FAR";
  } else if (sound && !dark && !moving && !near) {
    label = "NOISY_BRIGHT_STEADY_FAR";
  } else if (!sound && dark && !moving && near) {
    label = "QUIET_DARK_STEADY_NEAR";
  } else if (sound && !dark && moving && near) {
    label = "NOISY_BRIGHT_MOVING_NEAR";
  }

  Serial.print("state,"); Serial.println(label);
  Serial.println();

  delay(500);
}