/* Full integrated sketch – Infinite Firebase records
   - ESP32
   - DS18B20 on GPIO33 (OneWire)
   - pH, Turbidity, TDS (analog)
   - NEO-6M GPS (TX->GPIO16, RX->GPIO17) using TinyGPS++
   - Firebase Realtime DB uploads (1 JSON per upload cycle)
   - GPS validity checks (HDOP threshold, min satellites)
*/

#include <Arduino.h>
#include <WiFi.h>
#include <HardwareSerial.h>
#include <Firebase_ESP_Client.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <TinyGPS++.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// ===== CONFIG =====
#define WIFI_SSID "YourWifi"
#define WIFI_PASSWORD "YourPassword"

#define API_KEY "YourAPIKey"
#define USER_EMAIL "YourEmail@gmail.com"
#define USER_PASSWORD "YourPassword"
#define DATABASE_URL "YourDataBaseURL"

// ===== PINS =====
#define ONE_WIRE_BUS 33
#define PH_PIN 34
#define TURB_PIN 35
#define TDS_PIN 32
#define GPS_RX 16
#define GPS_TX 17

// ===== GLOBALS =====
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

String uid;
String databasePath;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

TinyGPSPlus gps;
HardwareSerial GPSserial(1);

// Last known values for fallback
float lastTemp = 25.0;
float lastPH = 7.0;
int lastTurb = 0;
float lastTDS = 300.0;
float lastLat = 0.0;
float lastLon = 0.0;
int lastSat = 0;
float lastSpeed = 0.0;

// Timing
unsigned long sendDataPrevMillis = 0UL;
const unsigned long timerDelay = 30000UL; // 30s

// GPS thresholds
const float GPS_HDOP_MAX = 5.0;
const int GPS_MIN_SATS = 4;

// ===== HELPERS =====
void initWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
    if (millis() - start > 20000UL) {
      Serial.println("\nWiFi connect timeout, retrying...");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      start = millis();
    }
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
}

bool isValidTemperature(float t) {
  if (isnan(t) || t == -127.0f || t == 85.0f) return false;
  return true;
}

bool gpsReadingIsValid() {
  if (!gps.location.isValid() || !gps.hdop.isValid() || !gps.satellites.isValid()) return false;
  if (gps.hdop.hdop() > GPS_HDOP_MAX) return false;
  if (gps.satellites.value() < GPS_MIN_SATS) return false;
  return true;
}

String gpsTimestampToISO() {
  if (!gps.date.isValid() || !gps.time.isValid()) return String("");
  char buf[32];
  sprintf(buf, "%04d-%02d-%02dT%02d:%02d:%02dZ",
          gps.date.year(), gps.date.month(), gps.date.day(),
          gps.time.hour(), gps.time.minute(), gps.time.second());
  return String(buf);
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("\n=== Startup ===");
  initWiFi();

  // Firebase
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;
  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(8192);
  Firebase.begin(&config, &auth);

  // wait for UID
  Serial.print("Waiting for Firebase UID");
  unsigned long tstart = millis();
  while (auth.token.uid == "") {
    Serial.print(".");
    delay(300);
    if (millis() - tstart > 30000UL) break;
  }
  uid = auth.token.uid.c_str();
  Serial.println("\nUID: " + uid);
  databasePath = "/UsersData/" + uid + "/records";

  // Sensors
  sensors.begin();

  // GPS
  GPSserial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  Serial.println("GPSserial started (9600) on RX=" + String(GPS_RX) + " TX=" + String(GPS_TX));
}

// ===== LOOP =====
void loop() {
  // Read GPS continuously
  while (GPSserial.available()) {
    gps.encode(GPSserial.read());
  }

  if (WiFi.status() != WL_CONNECTED) initWiFi();

  // Upload data every timerDelay
  if (Firebase.ready() && millis() - sendDataPrevMillis > timerDelay) {
    sendDataPrevMillis = millis();

    // ---- DS18B20 ----
    sensors.requestTemperatures();
    float temperature = sensors.getTempCByIndex(0);
    if (!isValidTemperature(temperature)) temperature = lastTemp;
    lastTemp = temperature;

    // ---- pH ----
    int phRaw = analogRead(PH_PIN);
    float phValue = map(phRaw, 0, 4095, 0, 14);
    if (isnan(phValue) || phValue < 0.0f || phValue > 14.0f) phValue = lastPH;
    lastPH = phValue;

    // ---- Turbidity ----
    int turbRaw = analogRead(TURB_PIN);
    int turbidity = (turbRaw < 0) ? lastTurb : turbRaw;
    lastTurb = turbidity;

    // ---- TDS ----
    int tdsRaw = analogRead(TDS_PIN);
    float voltage = (tdsRaw / 4095.0f) * 3.3f;
    float tdsValue = (voltage * 500.0f);
    if (isnan(tdsValue) || tdsValue < 0 || tdsValue > 5000) tdsValue = lastTDS;
    lastTDS = tdsValue;

    // ---- GPS ----
    bool gpsValid = gpsReadingIsValid();
    float lat = gpsValid ? gps.location.lat() : lastLat;
    float lon = gpsValid ? gps.location.lng() : lastLon;
    int sats = gpsValid ? gps.satellites.value() : lastSat;
    float speed = gpsValid && gps.speed.isValid() ? gps.speed.kmph() : lastSpeed;
    String gpsTS = gpsValid ? gpsTimestampToISO() : "";

    lastLat = lat;
    lastLon = lon;
    lastSat = sats;
    lastSpeed = speed;

    // ---- Push JSON to Firebase ----
    FirebaseJson record;
    record.set("timestamp", gpsTS);
    record.set("temperature", temperature);
    record.set("ph", phValue);
    record.set("turbidity", turbidity);
    record.set("tds", tdsValue);
    record.set("gpsLat", lat);
    record.set("gpsLon", lon);
    record.set("gpsSat", sats);
    record.set("gpsSpeed", speed);

    if (Firebase.RTDB.pushJSON(&fbdo, databasePath.c_str(), &record)) {
      Serial.println("✔ Record pushed at " + gpsTS);
    } else {
      Serial.println("❌ Failed push: " + fbdo.errorReason());
    }
  }

  delay(20);
}