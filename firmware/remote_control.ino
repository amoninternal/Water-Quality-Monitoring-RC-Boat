#include <WiFi.h>
#include <PubSubClient.h>

// -------- WIFI --------
const char* ssid = "YourWiFi";
const char* pass = "YourPassword";

// -------- MQTT --------
const char* mqtt_server = "broker.emqx.io";

WiFiClient espClient;
PubSubClient client(espClient);

// -------- BUTTON PINS --------
#define LEFT_PIN   32
#define FWD_PIN    33
#define RIGHT_PIN  25
#define BACK_PIN   26

void setupWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nWiFi connected!");
}

void reconnect() {
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect("BoatController")) {
      Serial.println("MQTT connected!");
    }
    delay(400);
  }
}

void sendCmd(const char* cmd) {
  Serial.print("Sending: ");
  Serial.println(cmd);
  client.publish("boat/control/cmd", cmd);
}

void setup() {
  Serial.begin(115200);

  pinMode(LEFT_PIN, INPUT_PULLUP);
  pinMode(FWD_PIN, INPUT_PULLUP);
  pinMode(RIGHT_PIN, INPUT_PULLUP);
  pinMode(BACK_PIN, INPUT_PULLUP);

  setupWiFi();
  client.setServer(mqtt_server, 1883);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  // --- Buttons ---
  if (!digitalRead(FWD_PIN)) {
    sendCmd("FORWARD");
  }
  else if (!digitalRead(BACK_PIN)) {
    sendCmd("BACK");
  }
  else if (!digitalRead(LEFT_PIN)) {
    sendCmd("LEFT");
  }
  else if (!digitalRead(RIGHT_PIN)) {
    sendCmd("RIGHT");
  }
  else {
    sendCmd("STOP");
  }

  delay(150); // Anti-spam delay
}