#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Firebase_ESP_Client.h>
#include <addons/RTDBHelper.h>
#include <addons/TokenHelper.h>
#include "time.h"

// === WiFi Credentials ===
#define WIFI_SSID "TECNO ETE"
#define WIFI_PASSWORD "AAAAAAAAAA"

// === Firebase Configuration ===
// IMPORTANT: Keep your API_KEY and DATABASE_URL secure in a real application.
// For development, ensure they are correct.
#define API_KEY "AIzaSyAWJh85grpimyf6qvLOQZq_VVNZZQHhSoI"
#define DATABASE_URL "https://electronic-click-default-rtdb.firebaseio.com/"

// === Realtime Database Paths ===
// Ensure these paths match your Firebase Realtime Database structure.
#define LED_COMMAND_PATH "artifacts/default-app-id/public/data/led_rtdb_control/command"
#define LED_STATE_PATH "artifacts/default-app-id/public/data/led_rtdb_control/currentLedState"
#define STATUS_PATH "esp32_status/counter"

// === GPIO Configuration ===
#define LED_PIN 5 // Ensure this is a valid GPIO pin on your ESP32

// === Global Variables ===
WiFiClientSecure netClient;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long lastWriteTime = 0;
const long writeInterval = 5000; // Write every 5 seconds
int counter = 0;
bool ledOn = false; // Tracks the current state of the LED

// Flag to indicate if time has been successfully synchronized
bool timeSynced = false;

// === Forward Declarations ===
void streamCallback(FirebaseStream data);
void streamTimeoutCallback(bool timeout);
void connectToWiFi();
void syncTime();
void connectToFirebase();

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Ensure LED is off initially
  Serial.println("ESP32 Firebase Realtime Database LED Control Sketch Starting...");

  connectToWiFi(); // Connect to WiFi
  syncTime();      // Synchronize time

  // Only proceed if WiFi is connected and time is synchronized
  if (WiFi.status() == WL_CONNECTED && timeSynced) {
    connectToFirebase(); // Connect to Firebase
  } else {
    Serial.println("❌ Initial setup failed. Please check WiFi or NTP configuration.");
    // Optionally, halt execution or try to re-initiate connections
    while(true) { delay(1000); } // Stop here if critical setup fails
  }

  // === Try reading initial command value ===
  // Attempt to read the initial command only if Firebase is ready
  if (Firebase.ready()) {
    Serial.println("Attempting initial read from Firebase...");
    if (Firebase.RTDB.getString(&fbdo, LED_COMMAND_PATH)) {
      String initialCommand = fbdo.stringData();
      Serial.print("✅ Initial read success. Value: ");
      Serial.println(initialCommand);
      // Set initial LED state based on what's in Firebase
      if (initialCommand == "on") {
        digitalWrite(LED_PIN, HIGH);
        ledOn = true;
        Serial.println("💡 Initial LED state set to ON.");
      } else if (initialCommand == "off") {
        digitalWrite(LED_PIN, LOW);
        ledOn = false;
        Serial.println("💡 Initial LED state set to OFF.");
      }
    } else {
      Serial.print("❌ Initial read failed: ");
      Serial.println(fbdo.errorReason());
      Serial.println("Proceeding without initial command state.");
    }

    // === Setup Stream Listener ===
    Serial.println("👂 Setting up Realtime Database stream listener for commands...");
    if (!Firebase.RTDB.beginStream(&fbdo, LED_COMMAND_PATH)) {
      Serial.print("❌ Failed to begin stream: ");
      Serial.println(fbdo.errorReason());
      Serial.println("Realtime updates will not be available.");
    } else {
      Serial.println("✅ Stream successfully initiated.");
      Firebase.RTDB.setStreamCallback(&fbdo, streamCallback, streamTimeoutCallback);
    }
  } else {
    Serial.println("Firebase was not ready. Stream listener not set up.");
  }
}

void loop() {
  // Always check Firebase.ready() before performing operations
  if (Firebase.ready()) {
    // Read stream data - this handles incoming commands
    Firebase.RTDB.readStream(&fbdo);

    // Periodically update a counter in Firebase
    if (millis() - lastWriteTime > writeInterval) {
      counter++;
      Serial.print("Attempting to write counter: ");
      Serial.println(counter);
      if (Firebase.RTDB.setInt(&fbdo, STATUS_PATH, counter)) {
        Serial.println("✅ Counter write successful.");
      } else {
        Serial.print("❌ Counter write failed: ");
        Serial.println(fbdo.errorReason());
      }
      lastWriteTime = millis();
    }
  } else {
    Serial.println("Firebase NOT ready in loop. Reconnecting...");
    // Attempt to reconnect if Firebase is not ready
    connectToFirebase();
  }

  delay(10); // Small delay to prevent watchdog timer issues
}

// === Function Implementations ===

void connectToWiFi() {
  Serial.print("🔌 Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); // Removed 'true' for default auto-reconnect behavior
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 40) { // Try for up to 20 seconds (40 * 500ms)
    Serial.print(".");
    delay(500);
    retries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi connected successfully.");
    Serial.print("📶 ESP32 IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ Failed to connect to WiFi.");
  }
}

void syncTime() {
  Serial.println("⏱️ Synchronizing time with NTP server...");
  // Use a more robust NTP setup with multiple servers
  configTime(0, 0, "pool.ntp.org", "time.nist.gov", "0.africa.pool.ntp.org"); // Added an African NTP server
  time_t now = time(nullptr);
  int retries = 0;
  // Wait for time to be set, up to 30 seconds
  while (now < 10000 && retries < 60) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    retries++;
  }
  if (now > 10000) {
    Serial.println("\n✅ Time synchronized.");
    timeSynced = true;
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    Serial.printf("Current time: %s", asctime(&timeinfo));
  } else {
    Serial.println("\n❌ Failed to synchronize time with NTP server after multiple attempts.");
    timeSynced = false;
  }
}

void connectToFirebase() {
  Serial.println("🔥 Preparing Firebase configuration...");
  // netClient.setInsecure(); // This line is crucial for bypassing cert validation, but consider removing for production
  // If you want secure connection, you need to provide Firebase root CA certificates.
  // Example (replace with actual Firebase CA):
  // netClient.setCACert(FIREBASE_CERT_PROD);

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // Use Email/Password Auth
  // Ensure this email and password are registered in your Firebase project's Authentication section.
  auth.user.email = "admin@email.com";
  auth.user.password = "123456";

  Serial.println("🔥 Connecting to Firebase...");
  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true); // Automatically reconnect to network if disconnected
  Firebase.reconnectWiFi(true);   // Automatically reconnect to WiFi if disconnected
  Firebase.setDoubleDigits(5);   // Set precision for double values

  Serial.print("⏳ Waiting for Firebase to be ready");
  unsigned long timeoutMillis = millis();
  // Wait for Firebase to become ready, up to 30 seconds
  while (!Firebase.ready() && (millis() - timeoutMillis < 30000)) {
    Serial.print(".");
    delay(500);
  }
  if (Firebase.ready()) {
    Serial.println("\n✅ Firebase is ready!");
    // You can now proceed with Firebase operations
  } else {
    Serial.print("\n❌ Firebase not ready after timeout. Error: ");
    Serial.println(fbdo.errorReason()); // This will show the actual Firebase error
    // If you continuously get SSL errors here, it's likely a time sync or network issue.
  }
}

void streamCallback(FirebaseStream data) {
  Serial.print("📥 Received Stream Event at Path: ");
  Serial.println(data.dataPath());

  if (data.dataType() == "string") {
    String command = data.stringData();
    Serial.print("Received command: ");
    Serial.println(command);

    if (command == "on" && !ledOn) {
      digitalWrite(LED_PIN, HIGH);
      ledOn = true;
      Serial.println("💡 LED turned ON");
      // Update LED state in Firebase
      if (!Firebase.RTDB.setString(&fbdo, LED_STATE_PATH, "on")) {
        Serial.print("❌ Failed to update LED_STATE_PATH to 'on': ");
        Serial.println(fbdo.errorReason());
      }
    } else if (command == "off" && ledOn) {
      digitalWrite(LED_PIN, LOW);
      ledOn = false;
      Serial.println("💡 LED turned OFF");
      // Update LED state in Firebase
      if (!Firebase.RTDB.setString(&fbdo, LED_STATE_PATH, "off")) {
        Serial.print("❌ Failed to update LED_STATE_PATH to 'off': ");
        Serial.println(fbdo.errorReason());
      }
    } else {
      Serial.println("❓ Unknown or duplicate command. No action taken.");
    }
  } else {
    Serial.print("Unhandled data type: ");
    Serial.println(data.dataType());
  }
}

void streamTimeoutCallback(bool timeout) {
  if (timeout) {
    Serial.println("⚠️ Stream timeout, attempting to reconnect...");
  }
  // The Firebase library typically handles stream re-establishment automatically.
  // No need for manual re-beginStream here unless you want custom logic.
}
