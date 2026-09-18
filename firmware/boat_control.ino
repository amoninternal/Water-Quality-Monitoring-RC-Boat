#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>

// WIFI
const char* ssid = "YourWifi";
const char* pass = "YourPassword";

// MQTT
const char* mqtt_server = "broker.emqx.io";
WiFiClient espClient;
PubSubClient client(espClient);

// --- HARDWARE CONFIGURATION ---
Servo rudder;
int rudderPin = 4;
int currentAngle = 90; 

// DC MOTOR (Propulsion)
const int motorPinA = 18; 
const int motorPinB = 19; 

// --- STATE VARIABLES ---
// 0 = STOP, 1 = FORWARD, -1 = BACKWARD
int throttleState = 0; 

void callback(char* topic, byte* message, unsigned int length) {
  String cmd = "";
  for (int i = 0; i < length; i++) {
    cmd += (char)message[i];
  }

  Serial.print("Received: ");
  Serial.println(cmd);

  // --- LOGIC: UPDATE STATES ---
  
  // 1. Steering Commands
  if (cmd == "LEFT") {
    currentAngle = 45;
  }
  else if (cmd == "RIGHT") {
    currentAngle = 135;
  }
  else if (cmd == "CENTER") {
    currentAngle = 90;
  }

  // 2. Throttle Commands
  if (cmd == "FORWARD") {
    throttleState = 1;      // Set state to Forward
    Serial.println("Throttle: FORWARD");
  }
  else if (cmd == "BACKWARD") {
    throttleState = -1;     // Set state to Backward
    Serial.println("Throttle: REVERSE");
  }
  else if (cmd == "STOP") {
    throttleState = 0;      // Set state to Stop
    currentAngle = 90;      // Optional: Reset rudder on stop
    Serial.println("Throttle: STOP");
  }

  // --- APPLY HARDWARE CHANGES ---
  applyBoatState();
}

void applyBoatState() {
  // Apply Steering
  rudder.write(currentAngle);

  // Apply Throttle based on State
  if (throttleState == 1) {
    // FORWARD
    digitalWrite(motorPinA, HIGH);
    digitalWrite(motorPinB, LOW); 
  } 
  else if (throttleState == -1) {
    // BACKWARD (Reverse logic of Forward)
    digitalWrite(motorPinA, LOW);
    digitalWrite(motorPinB, HIGH); 
  } 
  else {
    // STOP (Both Low)
    digitalWrite(motorPinA, LOW);
    digitalWrite(motorPinB, LOW);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect("BoatServoReceiver")) {
      Serial.println("MQTT connected & subscribed!");
      client.subscribe("boat/control/cmd");
    }
    delay(500);
  }
}

void setup() {
  Serial.begin(115200);

  // ---- WIFI ----
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
  }
  Serial.println("\nWiFi Connected!");

  // ---- MQTT ----
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  // ---- PINS SETUP ----
  rudder.attach(rudderPin);
  pinMode(motorPinA, OUTPUT);
  pinMode(motorPinB, OUTPUT);

  // Initialize State (Stopped)
  applyBoatState();
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();
}