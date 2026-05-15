#include <Arduino_HS300x.h>
#include <Arduino_BMI270_BMM150.h>
#include <Arduino_APDS9960.h>

// thresholds
#define HUMIDITY_JUMP_THRESHOLD    20
#define TEMP_RISE_THRESHOLD        0.5
#define MAG_SHIFT_THRESHOLD        100
#define LIGHT_CHANGE_THRESHOLD     150

// tracking
float baselineHumidity = -1;
float baselineTemp = -1;
float baselineMag = -1;
int baselineClear = -1;

unsigned long lastEventTime = 0;
#define COOLDOWN_MS 3000

void setup() {
  Serial.begin(115200);
  delay(1500);

  if (!HS300x.begin()) {
    Serial.println("Failed to initialize humidity/temperature sensor.");
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

  Serial.println("Environmental Event Detector started");
  Serial.println("Calibrating baseline");
  delay(3000);

  // establish baseline
  baselineHumidity = HS300x.readHumidity();
  baselineTemp = HS300x.readTemperature();

  float mx, my, mz;
  if (IMU.magneticFieldAvailable()) {
    IMU.readMagneticField(mx, my, mz);
    baselineMag = sqrt(mx*mx + my*my + mz*mz);
  }

  int r, g, b, c;
  if (APDS.colorAvailable()) {
    APDS.readColor(r, g, b, c);
    baselineClear = c;
  }

  Serial.println("Baseline established, monitoring started");
}

void loop() {
  // reading sensors
  float rh = HS300x.readHumidity();
  float temp = HS300x.readTemperature();

  float mx, my, mz;
  float magMag = baselineMag;
  if (IMU.magneticFieldAvailable()) {
    IMU.readMagneticField(mx, my, mz);
    magMag = sqrt(mx*mx + my*my + mz*mz);
  }

  int r, g, b, c;
  int clearVal = baselineClear;
  if (APDS.colorAvailable()) {
    APDS.readColor(r, g, b, c);
    clearVal = c;
  }

  // event indicators
  bool humidJump        = (rh - baselineHumidity) > HUMIDITY_JUMP_THRESHOLD;
  bool tempRise         = (temp - baselineTemp) > TEMP_RISE_THRESHOLD;
  bool magShift         = abs(magMag - baselineMag) > MAG_SHIFT_THRESHOLD;
  bool lightColorChange = abs(clearVal - baselineClear) > LIGHT_CHANGE_THRESHOLD;

  // printing raw valeus
  Serial.print("raw,rh="); Serial.print(rh, 2);
  Serial.print(",temp="); Serial.print(temp, 2);
  Serial.print(",mag="); Serial.print(magMag, 2);
  Serial.print(",r="); Serial.print(r);
  Serial.print(",g="); Serial.print(g);
  Serial.print(",b="); Serial.print(b);
  Serial.print(",clear="); Serial.println(clearVal);

  // printing flags
  Serial.print("flags,humid_jump="); Serial.print(humidJump);
  Serial.print(",temp_rise="); Serial.print(tempRise);
  Serial.print(",mag_shift="); Serial.print(magShift);
  Serial.print(",light_or_color_change="); Serial.println(lightColorChange);

  // classifying situations
  String label = "BASELINE_NORMAL";
  unsigned long now = millis();

  if (now - lastEventTime > COOLDOWN_MS) {
    if (humidJump || tempRise) {
      label = "BREATH_OR_WARM_AIR_EVENT";
      lastEventTime = now;
    } else if (magShift) {
      label = "MAGNETIC_DISTURBANCE_EVENT";
      lastEventTime = now;
    } else if (lightColorChange) {
      label = "LIGHT_OR_COLOR_CHANGE_EVENT";
      lastEventTime = now;
    }
  }

  Serial.print("event,"); Serial.println(label);
  Serial.println();

  delay(500);
}