#include <ESP8266WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiClientSecure.h>

// WiFi Configuration
const char* ssid = "Tenda_AI_VERSION";
const char* password = "hamza2020ago";

// Firebase configuration
const char* projectId = "tanksystem-fpt";
const char* apiKey = "AIzaSyA2982ZqgdvWnYdZfT8PdlRc7LJ31sCDWY";
const char* firestoreHost = "firestore.googleapis.com";

// Define HTTPS client
WiFiClientSecure client;

// HC-SR04 Pins
const int trigPin = 12;
const int echoPin = 14;

// DS18B20 Pins
const int oneWireBus = 0; // Pin where the DS18B20 is connected
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

// LED Pins
const int greenLED = 2; // Tank full
const int redLED = 4;  // Tank empty

// Tank height constant
const int tankHeightCM = 20; // Height of the tank in cm

// Timing variables for non-blocking delay
unsigned long previousMillis = 0;
const long interval = 1000; // 1-second interval

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  // Initialize LEDs
  pinMode(greenLED, OUTPUT);
  pinMode(redLED, OUTPUT);

  // Initialize HC-SR04
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Start DS18B20 Temperature Sensor
  sensors.begin();

  // Connect to WiFi
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");

  // Allow insecure HTTPS for Firestore communication
  client.setInsecure();
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Measure Tank Capacity and Temperature
    float tankCapacity = measureTankCapacity();
    float temperature = measureTemperature();

    // Display on Serial Monitor
    Serial.print("Tank Capacity: ");
    Serial.print(tankCapacity);
    Serial.println(" %");
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" °C");

    // Update LED Status
    if (tankCapacity >= 80) {
      digitalWrite(greenLED, HIGH);
      digitalWrite(redLED, LOW);
    } else if (tankCapacity <= 10) {
      digitalWrite(greenLED, LOW);
      digitalWrite(redLED, HIGH);
    } else {
      digitalWrite(greenLED, LOW);
      digitalWrite(redLED, LOW);
    }

    // Send Data to Firestore
    updateFirestoreData(tankCapacity, temperature);
  }
}

float measureTankCapacity() {
  // Send Trigger Pulse
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Measure Echo Pulse Duration
  long duration = pulseIn(echoPin, HIGH);

  // Calculate Distance (cm)
  float distance = duration * 0.034 / 2;

  // Calculate Tank Capacity Percentage
  float filledHeight = tankHeightCM - distance; // Height of water in cm
  if (filledHeight < 0) filledHeight = 0;
  if (filledHeight > tankHeightCM) filledHeight = tankHeightCM;
  return (filledHeight / tankHeightCM) * 100; // Return percentage
}

float measureTemperature() {
  sensors.requestTemperatures();       // Send command to get temperatures
  return sensors.getTempCByIndex(0);   // Get the temperature in Celsius
}

void updateFirestoreData(float tankCapacity, float temperature) {
  // Firestore API endpoint
  String documentPath = "/TankData/6IS8p1NLjuhj2JyNon2W";
  String url = String("/v1/projects/") + projectId + "/databases/(default)/documents" + documentPath + "?key=" + apiKey;

  // JSON payload with float values
  String jsonPayload = String("{\"fields\": {") +
                       "\"TankCapacity\": { \"doubleValue\": " + String(tankCapacity) + " }, " +
                       "\"Temperature\": { \"doubleValue\": " + String(temperature) + " }" +
                       "}}";

  Serial.println("Connecting to Firestore...");
  if (!client.connect(firestoreHost, 443)) {
    Serial.println("Connection to Firestore failed!");
    client.stop();
    return; // Exit the function if connection fails
  }

  // Send HTTP PATCH request to Firestore
  client.println("PATCH " + url + " HTTP/1.1");
  client.println("Host: " + String(firestoreHost));
  client.println("Content-Type: application/json");
  client.print("Content-Length: ");
  client.println(jsonPayload.length());
  client.println();
  client.print(jsonPayload);

  // Timeout logic for response
  unsigned long startTime = millis();
  while ((client.connected() || client.available()) && (millis() - startTime < 1000)) { // Timeout after 5 seconds
    if (client.available()) {
      String response = client.readStringUntil('\n');
      Serial.println(response); // Print Firestore response
    }
  }

  if (millis() - startTime >= 1000) {
    Serial.println("Timeout reached while waiting for Firestore response.");
  }
  
  client.stop(); // Close the connection
}
