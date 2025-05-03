#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>

const char* ssid = "Hidden";
const char* password = "abdullah1122";
const char* serverName = "http://192.168.205.6:5000/update";
const char* pumpStateUrl = "http://192.168.205.6:5000/api/pump_state";

LiquidCrystal_I2C lcd(0x27, 20, 4);
bool lcdInitialized = false;

const int voltagePin = 34;
const int currentPin = 35;
const int tdsPin = 32;
const int flowSensorPin = 26;
const int ldrPin = 33;
const int buzzerPin = 25;
const int pumpPin = 13;
const int testBuzzerPin = 27;
const int greenLedPin = 12;

volatile int flowPulseCount = 0;
float flowRate = 0.0;
float liters = 0.0;
unsigned long lastFlowTime = 0;
const float FLOW_CALIBRATION_FACTOR = 7.5;

float ACS712_offset = 2.5;
float ecCalibration = 1.0;
float Vref = 3.3;
float calibrationFactor = 0.909;
const float VOLTAGE_DIVIDER_RATIO = 5.0;

bool serverConnected = false;
bool wifiConnected = false;
unsigned long lastAlertToggle = 0;
bool alertState = false;
unsigned long wifiReconnectAttempt = 0;
const int WIFI_RECONNECT_INTERVAL = 5000;
const int WIFI_CONNECTION_TIMEOUT = 10000;

unsigned long lastSensorRead = 0;
unsigned long lastDataSend = 0;
unsigned long lastSerialOutput = 0;
const int SENSOR_READ_INTERVAL = 250;
const int SERVER_UPDATE_INTERVAL = 500;
const int SERIAL_OUTPUT_INTERVAL = 1000;

bool debugMode = true;

struct SensorData {
  float voltage;
  float current;
  float tdsValue;
  float flowRate;
  int ldrValue;
  bool pumpState;
} sensorData;

Preferences preferences;
HTTPClient http;
bool httpInitialized = false;

unsigned long lastHeartbeat = 0;
const int HEARTBEAT_INTERVAL = 5000;
unsigned long lastPumpToggle = 0;
const unsigned long PUMP_TOGGLE_COOLDOWN = 2000; // Skip redundant checks for 2s after toggle

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
  if (sensorData.voltage < 0.1 || sensorData.voltage > 30.0) sensorData.voltage = 0.0;

  int currentRaw = analogRead(currentPin);
  float voltageOut = (currentRaw * Vref) / 4095.0;
  sensorData.current = (voltageOut - ACS712_offset) / 0.185;
  if (sensorData.current < 0 || sensorData.current > 10.0) sensorData.current = 0.0;

  float waterTemp = 25.0;
  int tdsRaw = analogRead(tdsPin);
  float tdsVoltage = tdsRaw * (Vref / 4095.0);
  float temperatureCoefficient = 1.0 + 0.02 * (waterTemp - 25.0);
  float ec = (tdsVoltage / temperatureCoefficient) * ecCalibration;
  sensorData.tdsValue = ((133.42 * pow(ec, 3)) - (255.86 * pow(ec, 2)) + (857.39 * ec)) * 0.5 * calibrationFactor;
  if (sensorData.tdsValue < 0 || sensorData.tdsValue > 1000.0) sensorData.tdsValue = 0.0;

  sensorData.ldrValue = analogRead(ldrPin);
  if (sensorData.ldrValue < 10 || sensorData.ldrValue > 4095) sensorData.ldrValue = 0;

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

  // Removed sensorData.pumpState = digitalRead(pumpPin) to prevent hardware override

  if (sensorData.tdsValue > 1000 || (flowRate < 0.5 && flowRate > 0.1)) {
    digitalWrite(buzzerPin, HIGH);
  } else {
    digitalWrite(buzzerPin, LOW);
  }

  if (debugMode) {
    Serial.print("Sensor values - Voltage: "); Serial.print(sensorData.voltage);
    Serial.print(", Current: "); Serial.print(sensorData.current);
    Serial.print(", TDS: "); Serial.print(sensorData.tdsValue);
    Serial.print(", Flow: "); Serial.print(sensorData.flowRate);
    Serial.print(", Light: "); Serial.println(sensorData.ldrValue);
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

void checkPumpState() {
  if (!wifiConnected) {
    Serial.println("WiFi not connected, skipping pump state check");
    return;
  }

  if (!httpInitialized) {
    http.setTimeout(2000);
    httpInitialized = true;
  }

  static unsigned long lastLedToggle = 0;
  const unsigned long LED_DEBOUNCE_MS = 1000;

  int retries = 2;
  bool success = false;

  while (retries > 0 && !success) {
    http.begin(pumpStateUrl);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.GET();
    if (debugMode) {
      Serial.println("Checking pump state...");
    }

    if (httpResponseCode > 0) {
      String response = http.getString();
      if (!serverConnected) {
        serverConnected = true;
        Serial.println("Server connected!");
      }

      if (debugMode) {
        Serial.print("Pump state response: ");
        Serial.println(response);
      }

      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, response);
      if (error) {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
        retries--;
        if (retries > 0) {
          Serial.println("Retrying pump state check...");
          delay(500);
        }
        http.end();
        continue;
      }

      JsonObject pumpState = doc["pump_state"];
      if (pumpState && pumpState["pump_id"] == 1) {
        bool newState = pumpState["state"] == 1;
        unsigned long currentMillis = millis();
        if (newState != sensorData.pumpState && currentMillis - lastLedToggle > LED_DEBOUNCE_MS) {
          digitalWrite(pumpPin, newState ? HIGH : LOW);
          sensorData.pumpState = newState;
          lastLedToggle = currentMillis;
          lastPumpToggle = currentMillis; // Update toggle timestamp
          Serial.print("Pump 1 (Pin 13) set to ");
          Serial.println(newState ? "ON" : "OFF");
        } else {
          Serial.println("Pump state unchanged or debounced");
        }
      }
      success = true;
    } else {
      if (serverConnected) {
        serverConnected = false;
        Serial.print("Pump state check failed with error: ");
        Serial.println(httpResponseCode);
      }
      retries--;
      if (retries > 0) {
        Serial.println("Retrying pump state check...");
        delay(500);
      }
    }

    http.end();
  }

  if (!success) {
    Serial.println("Failed to check pump state after retries");
  }
}

void sendDataToServer() {
  if (!wifiConnected) {
    Serial.println("WiFi not connected, skipping server update");
    return;
  }

  if (!httpInitialized) {
    http.setTimeout(2000);
    httpInitialized = true;
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastPumpToggle < PUMP_TOGGLE_COOLDOWN) {
    Serial.println("Skipping server update due to recent pump toggle");
    return;
  }

  String jsonData = "{\"voltage\":" + String(sensorData.voltage, 1) +
                    ",\"current\":" + String(sensorData.current, 2) +
                    ",\"tds\":" + String(sensorData.tdsValue, 1) +
                    ",\"flow\":" + String(sensorData.flowRate, 1) +
                    ",\"light\":" + String(sensorData.ldrValue) +
                    ",\"switch\":" + String(sensorData.pumpState ? 1 : 0) +
                    ",\"pump_id\":1}";

  static unsigned long lastLedToggle = 0;
  const unsigned long LED_DEBOUNCE_MS = 1000;

  int retries = 3;
  bool success = false;

  while (retries > 0 && !success) {
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.POST(jsonData);
    if (debugMode) {
      Serial.print("Sending: ");
      Serial.println(jsonData);
    }

    if (httpResponseCode > 0) {
      String response = http.getString();
      if (!serverConnected) {
        serverConnected = true;
        Serial.println("Server connected!");
      }

      if (debugMode) {
        Serial.print("Server response: ");
        Serial.println(response);
      }

      StaticJsonDocument<512> doc;
      DeserializationError error = deserializeJson(doc, response);
      if (error) {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
        retries--;
        if (retries > 0) {
          Serial.println("Retrying server update...");
          delay(500);
        }
        http.end();
        continue;
      }

      JsonArray pumpStates = doc["pump_states"];
      for (JsonObject pump : pumpStates) {
        if (pump["pump_id"] == 1) {
          bool newState = pump["state"] == 1;
          unsigned long currentMillis = millis();
          if (newState != sensorData.pumpState && currentMillis - lastLedToggle > LED_DEBOUNCE_MS) {
            digitalWrite(pumpPin, newState ? HIGH : LOW);
            sensorData.pumpState = newState;
            lastLedToggle = currentMillis;
            lastPumpToggle = currentMillis; // Update toggle timestamp
            Serial.print("Pump 1 (Pin 13) set to ");
            Serial.println(newState ? "ON" : "OFF");
          } else {
            Serial.println("Pump state unchanged or debounced");
          }
          break;
        }
      }
      success = true;
    } else {
      if (serverConnected) {
        serverConnected = false;
        Serial.print("Server connection failed with error: ");
        Serial.println(httpResponseCode);
      }
      retries--;
      if (retries > 0) {
        Serial.println("Retrying server update...");
        delay(500);
      }
    }

    http.end();
  }

  if (!success) {
    Serial.println("Failed to send data after retries");
  }
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
  pinMode(pumpPin, OUTPUT);
  pinMode(testBuzzerPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);

  digitalWrite(pumpPin, LOW);
  digitalWrite(testBuzzerPin, LOW);
  digitalWrite(greenLedPin, LOW);

  Serial.println("Testing pin 13 (pump/LED)...");
  digitalWrite(pumpPin, HIGH);
  delay(1000);
  digitalWrite(pumpPin, LOW);
  delay(1000);
  digitalWrite(pumpPin, HIGH);
  delay(1000);
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

  // Initialize pump state
  sensorData.pumpState = false;
  digitalWrite(pumpPin, LOW);

  Serial.println("Setup complete!");
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastHeartbeat > HEARTBEAT_INTERVAL) {
    Serial.println("System heartbeat: running");
    Serial.print("Pin 13 state: ");
    Serial.println(digitalRead(pumpPin) ? "HIGH (ON)" : "LOW (OFF)");
    Serial.print("WiFi status: ");
    Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
    lastHeartbeat = currentMillis;
  }

  manageWiFiConnection();
  manageLEDsAndBuzzer();

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
