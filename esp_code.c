#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h> // Add this library for JSON parsing

// Wi-Fi credentials
const char* ssid = "Hidden";
const char* password = "abdullah1122";
const char* serverName = "http://192.168.205.6:5000/update";

// LCD setup
LiquidCrystal_I2C lcd(0x27, 20, 4);
bool lcdInitialized = false;

// Pin definitions
const int voltagePin = 34;
const int currentPin = 35;
const int tdsPin = 32;
const int flowSensorPin = 26;
const int ldrPin = 33;
const int buzzerPin = 25;
const int pumpPin = 13; // LED/pump control pin
const int testBuzzerPin = 27;
const int greenLedPin = 12;

// Flow sensor variables
volatile int flowPulseCount = 0;
float flowRate = 0.0;
float liters = 0.0;
unsigned long lastFlowTime = 0;
const float FLOW_CALIBRATION_FACTOR = 7.5;

// Sensor calibration
float ACS712_offset = 2.5;
float ecCalibration = 1.0;
float Vref = 3.3;
float calibrationFactor = 0.909;
const float VOLTAGE_DIVIDER_RATIO = 5.0;

// Connection variables
bool serverConnected = false;
bool wifiConnected = false;
unsigned long lastAlertToggle = 0;
bool alertState = false;
unsigned long wifiReconnectAttempt = 0;
const int WIFI_RECONNECT_INTERVAL = 30000;
const int WIFI_CONNECTION_TIMEOUT = 10000;

// Timing variables
unsigned long lastSensorRead = 0;
unsigned long lastDataSend = 0;
unsigned long lastSerialOutput = 0;
const int SENSOR_READ_INTERVAL = 250;
const int SERVER_UPDATE_INTERVAL = 2000;
const int SERIAL_OUTPUT_INTERVAL = 1000;

// Debug variables
bool debugMode = true;
unsigned long lastManualLedToggle = 0;
const int MANUAL_LED_TOGGLE_INTERVAL = 10000; // 10 seconds for testing

// Sensor data structure
struct SensorData {
  float voltage;
  float current;
  float tdsValue;
  float flowRate;
  int ldrValue;
  bool pumpState;
} sensorData;

// Preferences
Preferences preferences;

// HTTP Client
HTTPClient http;
bool httpInitialized = false;

// Heartbeat
unsigned long lastHeartbeat = 0;
const int HEARTBEAT_INTERVAL = 5000;

void IRAM_ATTR flowPulse() {
  flowPulseCount++;
  lastFlowTime = millis();
}

bool initializeI2C() {
  Wire.begin();
  Wire.setClock(100000);
  return scanI2C();
}

bool scanI2C() {
  byte error, address;
  int nDevices = 0;
  Serial.println("Scanning I2C bus...");
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
      nDevices++;
    }
  }
  return nDevices > 0;
}

bool initializeLCD() {
  lcd.begin(20, 4);
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Eco-Go");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(100);
  return true;
}

void connectToWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  lcd.setCursor(0, 2);
  lcd.print("Connecting to WiFi");

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < WIFI_CONNECTION_TIMEOUT) {
    if (millis() - lastAlertToggle > 50) {
      alertState = !alertState;
      digitalWrite(testBuzzerPin, alertState);
      digitalWrite(greenLedPin, alertState);
      lastAlertToggle = millis();
      Serial.print(".");
    }
    yield();
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\nConnected to WiFi");
    Serial.println(WiFi.localIP());
    lcd.setCursor(0, 3);
    lcd.print("WiFi: " + WiFi.localIP().toString());
    digitalWrite(testBuzzerPin, LOW);
    digitalWrite(greenLedPin, HIGH);
  } else {
    Serial.println("\nFailed to connect to WiFi");
    lcd.setCursor(0, 3);
    lcd.print("WiFi: Failed");
    wifiConnected = false;
  }
}

void readSensors() {
  int voltageRaw = analogRead(voltagePin);
  sensorData.voltage = (voltageRaw * (Vref / 4095.0)) * VOLTAGE_DIVIDER_RATIO;

  int currentRaw = analogRead(currentPin);
  float voltageOut = (currentRaw * Vref) / 4095.0;
  sensorData.current = (voltageOut - ACS712_offset) / 0.185;

  float waterTemp = 25.0;
  int tdsRaw = analogRead(tdsPin);
  float tdsVoltage = tdsRaw * (Vref / 4095.0);
  float temperatureCoefficient = 1.0 + 0.02 * (waterTemp - 25.0);
  float ec = (tdsVoltage / temperatureCoefficient) * ecCalibration;
  sensorData.tdsValue = ((133.42 * pow(ec, 3)) - (255.86 * pow(ec, 2)) + (857.39 * ec)) * 0.5 * calibrationFactor;

  sensorData.ldrValue = analogRead(ldrPin);

  detachInterrupt(digitalPinToInterrupt(flowSensorPin));
  unsigned long currentTime = millis();
  if (currentTime - lastFlowTime > 1000) {
    flowRate = 0;
  } else {
    flowRate = (flowPulseCount / FLOW_CALIBRATION_FACTOR);
  }
  sensorData.flowRate = flowRate;
  liters += flowRate / (60.0 * (SENSOR_READ_INTERVAL / 1000.0));
  flowPulseCount = 0;
  attachInterrupt(digitalPinToInterrupt(flowSensorPin), flowPulse, RISING);

  sensorData.pumpState = digitalRead(pumpPin);

  if (sensorData.tdsValue > 1000 || (flowRate < 0.5 && flowRate > 0.1)) {
    digitalWrite(buzzerPin, HIGH);
  } else {
    digitalWrite(buzzerPin, LOW);
  }
}

void updateLCD() {
  if (!lcdInitialized) return;

  lcd.setCursor(0, 0);
  lcd.print("V:"); lcd.print(sensorData.voltage, 1);
  lcd.print(" I:"); lcd.print(sensorData.current, 1);

  lcd.setCursor(0, 1);
  lcd.print("TDS:"); lcd.print((int)sensorData.tdsValue);
  lcd.print("ppm F:"); lcd.print(sensorData.flowRate, 1);

  lcd.setCursor(0, 2);
  lcd.print("Light:"); lcd.print(sensorData.ldrValue);
  lcd.print("     ");

  lcd.setCursor(0, 3);
  if (wifiConnected) {
    lcd.print("Total:"); lcd.print(liters, 1); lcd.print("L");
  } else {
    lcd.print("WiFi: Disconnected ");
  }

  lcd.setCursor(16, 3);
  lcd.print(sensorData.pumpState ? "ON " : "OFF");
}

// Improved server data handling with proper JSON parsing
void sendDataToServer() {
  if (!wifiConnected) return;

  if (!httpInitialized) {
    http.setTimeout(5000);
    httpInitialized = true;
  }

  String jsonData = "{\"voltage\":" + String(sensorData.voltage, 1) +
                    ",\"current\":" + String(sensorData.current, 2) +
                    ",\"tds\":" + String(sensorData.tdsValue, 1) +
                    ",\"flow\":" + String(sensorData.flowRate, 1) +
                    ",\"light\":" + String(sensorData.ldrValue) +
                    ",\"pump\":1}"; // Assuming pump 1 for simplicity

  http.begin(serverName);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(jsonData);

  if (httpResponseCode > 0) {
    String response = http.getString();
    if (!serverConnected) {
      serverConnected = true;
      Serial.println("Server connected!");
    }

    // Debug: Print received response
    if (debugMode) {
      Serial.println("Server response:");
      Serial.println(response);
    }

    // Use a more flexible approach to parse pump control commands
    // Option 1: Simple string check (more flexible than exact match)
    if (response.indexOf("\"pump_id\": 1") != -1 && response.indexOf("\"state\": 1") != -1) {
      digitalWrite(pumpPin, HIGH);
      sensorData.pumpState = true;
      Serial.println("Pump command received: ON");
    } else if (response.indexOf("\"pump_id\": 1") != -1 && response.indexOf("\"state\": 0") != -1) {
      digitalWrite(pumpPin, LOW);
      sensorData.pumpState = false;
      Serial.println("Pump command received: OFF");
    }

    // Option 2: Use proper JSON parsing (more robust but requires ArduinoJson library)
    // Uncomment if you add ArduinoJson library
    /*
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error && doc.containsKey("pump_states")) {
      JsonArray pumpStates = doc["pump_states"];
      for (JsonObject pumpState : pumpStates) {
        if (pumpState["pump_id"] == 1) {
          if (pumpState["state"] == 1) {
            digitalWrite(pumpPin, HIGH);
            sensorData.pumpState = true;
            Serial.println("Pump turned ON via JSON parsing");
          } else {
            digitalWrite(pumpPin, LOW);
            sensorData.pumpState = false;
            Serial.println("Pump turned OFF via JSON parsing");
          }
        }
      }
    }
    */
  } else {
    if (serverConnected) {
      serverConnected = false;
      Serial.print("Server connection failed with error: ");
      Serial.println(httpResponseCode);
    }
  }

  http.end();
}

void printToSerial() {
  Serial.print("Voltage: "); Serial.print(sensorData.voltage, 1); Serial.print("V | ");
  Serial.print("Current: "); Serial.print(sensorData.current, 2); Serial.print("A | ");
  Serial.print("TDS: "); Serial.print(sensorData.tdsValue, 1); Serial.print("ppm | ");
  Serial.print("Flow: "); Serial.print(sensorData.flowRate, 1); Serial.print(" L/min | ");
  Serial.print("Light: "); Serial.print(sensorData.ldrValue); Serial.print(" | ");
  Serial.print("Pump/LED (Pin 13): "); Serial.println(sensorData.pumpState ? "ON" : "OFF");
}

void manageLEDsAndBuzzer() {
  unsigned long currentMillis = millis();

  if (!wifiConnected) {
    if (currentMillis - lastAlertToggle > 50) {
      alertState = !alertState;
      digitalWrite(testBuzzerPin, alertState);
      digitalWrite(greenLedPin, alertState);
      lastAlertToggle = currentMillis;
    }
  } else {
    if (serverConnected) {
      digitalWrite(testBuzzerPin, LOW);
      digitalWrite(greenLedPin, HIGH);
    } else {
      digitalWrite(testBuzzerPin, LOW);
      if (currentMillis - lastAlertToggle > 50) {
        alertState = !alertState;
        digitalWrite(greenLedPin, alertState);
        lastAlertToggle = currentMillis;
      }
    }
  }
}

// Test function to verify pin 13 is working correctly
void testLED() {
  unsigned long currentMillis = millis();
  
  // Toggle LED every MANUAL_LED_TOGGLE_INTERVAL if in debug mode
  if (debugMode && currentMillis - lastManualLedToggle > MANUAL_LED_TOGGLE_INTERVAL) {
    bool newState = !digitalRead(pumpPin);
    digitalWrite(pumpPin, newState);
    sensorData.pumpState = newState;
    Serial.print("MANUAL TEST: Toggling pump/LED to ");
    Serial.println(newState ? "ON" : "OFF");
    lastManualLedToggle = currentMillis;
  }
}

void manageWiFiConnection() {
  unsigned long currentMillis = millis();

  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiConnected) {
      wifiConnected = true;
      Serial.println("WiFi connected!");
      lcd.setCursor(0, 3);
      lcd.print("WiFi: " + WiFi.localIP().toString());
    }
  } else {
    if (wifiConnected) {
      wifiConnected = false;
      serverConnected = false;
      Serial.println("WiFi disconnected!");
      lcd.setCursor(0, 3);
      lcd.print("WiFi: Disconnected");
    }

    if (currentMillis - wifiReconnectAttempt > WIFI_RECONNECT_INTERVAL) {
      Serial.println("Attempting WiFi reconnection...");
      connectToWiFi();
      wifiReconnectAttempt = currentMillis;
    }
  }
}

void saveSettings() {
  preferences.begin("eco-go", false);
  preferences.putFloat("total-liters", liters);
  preferences.end();
}

void loadSettings() {
  preferences.begin("eco-go", true);
  liters = preferences.getFloat("total-liters", 0.0);
  preferences.end();
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nEco-Go Water Quality Monitor Starting...");

  pinMode(flowSensorPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);
  pinMode(pumpPin, OUTPUT); // Set pin 13 as output for LED/pump
  pinMode(testBuzzerPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);

  // Explicitly initialize pin 13 to OFF state
  digitalWrite(pumpPin, LOW);
  digitalWrite(testBuzzerPin, LOW);
  digitalWrite(greenLedPin, LOW);

  // Test LED on pin 13 during startup
  Serial.println("Testing pin 13 (pump/LED)...");
  digitalWrite(pumpPin, HIGH);
  delay(500);
  digitalWrite(pumpPin, LOW);
  Serial.println("Pin 13 test complete");

  if (!initializeI2C()) {
    Serial.println("WARNING: I2C initialization issue");
  }

  if (initializeLCD()) {
    lcdInitialized = true;
    Serial.println("LCD initialized successfully");
  } else {
    Serial.println("ERROR: LCD initialization failed");
  }

  loadSettings();
  connectToWiFi();
  attachInterrupt(digitalPinToInterrupt(flowSensorPin), flowPulse, RISING);

  Serial.println("Setup complete!");
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastHeartbeat > HEARTBEAT_INTERVAL) {
    Serial.println("System heartbeat: running");
    // Print pin 13 state with each heartbeat
    Serial.print("Pin 13 state: ");
    Serial.println(digitalRead(pumpPin) ? "HIGH (ON)" : "LOW (OFF)");
    lastHeartbeat = currentMillis;
  }

  manageWiFiConnection();
  manageLEDsAndBuzzer();
  
  // Test LED functionality if in debug mode
  testLED();

  if (currentMillis - lastSensorRead > SENSOR_READ_INTERVAL) {
    readSensors();
    updateLCD();
    lastSensorRead = currentMillis;
  }

  if (wifiConnected && (currentMillis - lastDataSend > SERVER_UPDATE_INTERVAL)) {
    sendDataToServer();
    lastDataSend = currentMillis;
  }

  if (currentMillis - lastSerialOutput > SERIAL_OUTPUT_INTERVAL) {
    printToSerial();
    lastSerialOutput = currentMillis;

    static unsigned long lastSave = 0;
    if (currentMillis - lastSave > 60000) {
      saveSettings();
      lastSave = currentMillis;
    }
  }

  yield();
}
