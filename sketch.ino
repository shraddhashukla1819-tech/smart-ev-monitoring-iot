
#define BLYNK_TEMPLATE_ID   "TMPL3D4yoyTfq"
#define BLYNK_TEMPLATE_NAME "Smart EV Monitoring"
#define BLYNK_AUTH_TOKEN    "TAou-_lkTwgillwMDIrjxT_QpiC7DDG4"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

// =========================================================================
// 2. PIN DEFINITIONS & SENSOR OBJECTS
// =========================================================================
#define VOLT_PIN      34   // Potentiometer 1: Charging Voltage
#define CURR_PIN      35   // Potentiometer 2: Charging Current
#define LOAD_PIN      32   // Potentiometer 3: Motor Load
#define DHT_PIN       15   // DHT22: Charger/Battery Temperature
#define PARK_SW_PIN   4    // Slide Switch: HIGH = Parked, LOW = Driving
#define PIR_PIN       13   // PIR Sensor: Cabin/Perimeter Motion
#define BUZZER_PIN    25   // Buzzer: Theft Alarm
#define RED_LED_PIN   26   // Alert LED
#define GREEN_LED_PIN 27   // Normal/Safe LED

#define DHTTYPE DHT22
DHT dht(DHT_PIN, DHTTYPE);
Adafruit_MPU6050 mpu;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Wokwi Default Wi-Fi (Change SSID to "WRONG_WIFI" to test Offline Mode!)
char ssid[] = "Wokwi-GUEST";
char pass[] = "";

// Threshold Constants
const float MAX_SAFE_VOLT   = 85.0;  // Over-voltage above 85V
const float MAX_SAFE_CURR   = 30.0;  // Excessive current above 30A
const float MAX_SAFE_TEMP   = 45.0;  // Over-temperature above 45°C
const int   MAX_MOTOR_LOAD  = 80;    // Excessive motor load above 80%
const float ACCEL_THRESHOLD = 12.0;  // Sudden acceleration/braking (m/s^2)

// Global State Variables
float voltage = 0.0, current = 0.0, temperature = 28.0;
int motorLoad = 0;
float accelX = 0.0, accelY = 0.0;
bool isParked = false;
bool theftAlert = false;
bool wifiOnline = false;

String chargingStatus = "NORMAL";
String driverStatus   = "NORMAL";
String alertSummary   = "System Normal";

unsigned long lastSensorRead = 0;
unsigned long lastBlynkSend  = 0;
unsigned long lastLcdSwitch  = 0;
int lcdPage = 0;

void setup() {
  Serial.begin(115200);

  pinMode(PARK_SW_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);

  // Initialize I2C LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Smart EV System");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");

  // Initialize DHT22 & MPU6050
  dht.begin();
  if (!mpu.begin()) {
    Serial.println("MPU6050 not found! Check wiring.");
  } else {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  }

  // Non-blocking Wi-Fi & Blynk initialization (Allows offline fallback)
  WiFi.begin(ssid, pass);
  Blynk.config(BLYNK_AUTH_TOKEN);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 4000) {
    delay(200);
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiOnline = true;
    Blynk.connect(2000);
    Serial.println("Wi-Fi & Blynk Connected!");
  } else {
    wifiOnline = false;
    Serial.println("Wi-Fi Disconnected! Running in Local Offline Mode.");
  }
  lcd.clear();
}

void readAndAnalyzeSensors() {
  // --- MODULE 1: EV CHARGING MONITORING ---
  int rawVolt = analogRead(VOLT_PIN);
  int rawCurr = analogRead(CURR_PIN);
  voltage = (rawVolt / 4095.0) * 100.0; // Mapped to 0 - 100V
  current = (rawCurr / 4095.0) * 50.0;  // Mapped to 0 - 50A

  float t = dht.readTemperature();
  if (!isnan(t)) {
    temperature = t;
  }

  if (current > MAX_SAFE_CURR) {
    chargingStatus = "OVER-CURRENT!";
  } else if (voltage > MAX_SAFE_VOLT) {
    chargingStatus = "OVER-VOLTAGE!";
  } else if (temperature > MAX_SAFE_TEMP) {
    chargingStatus = "OVER-TEMP!";
  } else if (voltage < 5.0 && current < 0.5) {
    chargingStatus = "IDLE / UNPLUGGED";
  } else {
    chargingStatus = "CHARGING OK";
  }

  // --- MODULE 2: DRIVER BEHAVIOUR ANALYSIS ---
  int rawLoad = analogRead(LOAD_PIN);
  motorLoad = map(rawLoad, 0, 4095, 0, 100);

  sensors_event_t a, g, tempEvent;
  mpu.getEvent(&a, &g, &tempEvent);
  accelX = a.acceleration.x;
  accelY = a.acceleration.y;

  isParked = (digitalRead(PARK_SW_PIN) == HIGH);

  if (isParked) {
    driverStatus = "PARKED (IDLE)";
  } else {
    if (accelX > ACCEL_THRESHOLD) {
      driverStatus = "SUDDEN ACCEL!";
    } else if (accelX < -ACCEL_THRESHOLD) {
      driverStatus = "SUDDEN BRAKE!";
    } else if (motorLoad > MAX_MOTOR_LOAD) {
      driverStatus = "HIGH LOAD!";
    } else {
      driverStatus = "SAFE DRIVING";
    }
  }

  // --- MODULE 3: ANTI-THEFT DETECTION ---
  bool motionDetected = (digitalRead(PIR_PIN) == HIGH);
  bool vehicleMoved   = (abs(accelX) > 2.5 || abs(accelY) > 2.5);

  if (isParked && (motionDetected || vehicleMoved)) {
    theftAlert = true;
    tone(BUZZER_PIN, 1000); // Activate Buzzer Alarm
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, LOW);
  } else {
    theftAlert = false;
    noTone(BUZZER_PIN);     // Deactivate Buzzer
    bool anyFault = (chargingStatus != "CHARGING OK" && chargingStatus != "IDLE / UNPLUGGED") ||
                    (driverStatus != "SAFE DRIVING" && driverStatus != "PARKED (IDLE)");
    digitalWrite(RED_LED_PIN, anyFault ? HIGH : LOW);
    digitalWrite(GREEN_LED_PIN, anyFault ? LOW : HIGH);
  }

  // Build Overall Alert Summary
  if (theftAlert) {
    alertSummary = "THEFT ALERT! Unauthorized movement!";
  } else if (chargingStatus != "CHARGING OK" && chargingStatus != "IDLE / UNPLUGGED") {
    alertSummary = "CHARGING FAULT: " + chargingStatus;
  } else if (driverStatus != "SAFE DRIVING" && driverStatus != "PARKED (IDLE)") {
    alertSummary = "DRIVER ALERT: " + driverStatus;
  } else {
    alertSummary = isParked ? "Vehicle Parked & Armed" : "EV Operating Normally";
  }

  // Print Detailed Logs to Serial Monitor (Works Online & Offline)
  Serial.println("--------------------------------------------------");
  Serial.printf("WiFi: %s | Mode: %s\n", wifiOnline ? "ONLINE" : "OFFLINE", isParked ? "PARKED" : "DRIVING");
  Serial.printf("[Charging] V: %.1fV | I: %.1fA | Temp: %.1fC -> %s\n", voltage, current, temperature, chargingStatus.c_str());
  Serial.printf("[Driver]   Load: %d%% | AccelX: %.1f m/s2 -> %s\n", motorLoad, accelX, driverStatus.c_str());
  Serial.printf("[Security] Theft Alert: %s\n", theftAlert ? "YES (BUZZER ON)" : "NO");
}

void updateLocalLCD() {
  lcd.clear();
  if (theftAlert) {
    // Immediate override on LCD during theft
    lcd.setCursor(0, 0);
    lcd.print("!! THEFT ALERT !!");
    lcd.setCursor(0, 1);
    lcd.print(wifiOnline ? "WiFi:ON  ALM:ON" : "WiFi:OFF ALM:ON");
    return;
  }

  if (lcdPage == 0) {
    // Page 0: Charging Telemetry
    lcd.setCursor(0, 0);
    lcd.printf("V:%.0fV I:%.1fA %.0fC", voltage, current, temperature);
    lcd.setCursor(0, 1);
    lcd.print(chargingStatus);
  } else if (lcdPage == 1) {
    // Page 1: Driver Behaviour
    lcd.setCursor(0, 0);
    lcd.printf("Load:%d%% Ax:%.1f", motorLoad, accelX);
    lcd.setCursor(0, 1);
    lcd.print(driverStatus);
  } else {
    // Page 2: Security & Offline/Online Status
    lcd.setCursor(0, 0);
    lcd.print(isParked ? "Mode: PARKED" : "Mode: DRIVING");
    lcd.setCursor(0, 1);
    lcd.print(wifiOnline ? "WiFi: ONLINE" : "WiFi: OFFLINE");
  }
}

void sendToBlynk() {
  if (!wifiOnline || !Blynk.connected()) return;

  Blynk.virtualWrite(V0, voltage);
  Blynk.virtualWrite(V1, current);
  Blynk.virtualWrite(V2, temperature);
  Blynk.virtualWrite(V3, chargingStatus);
  Blynk.virtualWrite(V4, motorLoad);
  Blynk.virtualWrite(V5, driverStatus);
  Blynk.virtualWrite(V6, isParked ? "PARKED (ARMED)" : "DRIVING");
  Blynk.virtualWrite(V7, theftAlert ? 1 : 0);
  Blynk.virtualWrite(V8, alertSummary);
}

void loop() {
  // Check Wi-Fi Status Continuously (Non-blocking Offline Fallback)
  if (WiFi.status() == WL_CONNECTED) {
    wifiOnline = true;
    if (!Blynk.connected()) {
      Blynk.connect(500);
    }
    Blynk.run();
  } else {
    wifiOnline = false;
  }

  // Read sensors every 1 second
  if (millis() - lastSensorRead >= 1000) {
    lastSensorRead = millis();
    readAndAnalyzeSensors();
  }

  // Cycle LCD screens every 2.5 seconds
  if (millis() - lastLcdSwitch >= 2500) {
    lastLcdSwitch = millis();
    lcdPage = (lcdPage + 1) % 3;
    updateLocalLCD();
  }

  // Push telemetry to Blynk every 1.5 seconds
  if (millis() - lastBlynkSend >= 1500) {
    lastBlynkSend = millis();
    sendToBlynk();
  }
}