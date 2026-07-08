#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "filters.h"
#include "logger.h"

// Hardware Interface Structs
OneWire oneWire(PIN_ONEWIRE_BUS);
DallasTemperature tempSensor(&oneWire);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Filters
EwmaFilter batteryFilter(0.1f); // Smooth out high current-draw TX transients
EwmaFilter depthFilter(0.2f);
EwmaFilter turbidityFilter(0.15f);

// Globally Tracked Functional Values
float currentTemp = 0.0f;
float currentDepth = 0.0f;
float currentTurbidity = 0.0f;
float currentBatteryV = 7.4f;
int leakDebounceCounter = 0;

// Timing Flags
unsigned long lastSampleTime = 0;
unsigned long lastPublishTime = 0;

// Flags for Device Presence
bool tempSensorActive = false;
bool pressureSensorActive = false;

// Function Prototypes
void initializeSensors();
void checkNetworkStateMachine();
void processSensors();
void executeSafeState(const char* context);
void transmitRS485(const String& data);

void setup() {
    Serial.begin(115200);
    while(!Serial && millis() < 2000);
    
    Serial.println("\n--- Pelvyn Autonomous Node: Initializing Firmware ---");

    // Initialize System Storage Logging
    SystemLogger::init();

    // Set Hardware pin behaviors
    pinMode(PIN_LEAK_DIGITAL, INPUT_PULLUP);
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, LOW);

    // Initialize RS485 Control Pin & UART2
    pinMode(PIN_RS485_CTRL, OUTPUT);
    digitalWrite(PIN_RS485_CTRL, LOW); // Default to Listening (RX) mode
    Serial2.begin(RS485_BAUD_RATE, SERIAL_8N1, PIN_RS485_RX, PIN_RS485_TX);

    // Initialize Watchdog Timer
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = WDT_TIMEOUT_S * 1000,
        .idle_core_mask = 1 << 0, 
        .trigger_panic = true
    };
    esp_task_wdt_init(&wdt_config);
    esp_task_wdt_add(NULL); // Add main loop task reference

    // Initialize Sensor Hardware
    initializeSensors();

    // Set up network connection properties
    WiFi.mode(WIFI_STA);
    mqttClient.setServer("192.168.1.100", 1883); // Indicative topside gateway broker address
}

void loop() {
    // Pet the Hardware Watchdog Timer
    esp_task_wdt_reset();

    unsigned long currentMillis = millis();

    // Task Execution 1: Read, Filter, and Screen Sensoring Units
    if (currentMillis - lastSampleTime >= SENSOR_SAMPLE_INTERVAL_MS) {
        lastSampleTime = currentMillis;
        processSensors();
    }

    // Task Execution 2: Low-Impact Async Network Reconnections
    checkNetworkStateMachine();

    // Task Execution 3: Telemetry Broadcast Window
    if (currentMillis - lastPublishTime >= TELEMETRY_PUBLISH_INTERVAL_MS) {
        lastPublishTime = currentMillis;
        
        // Construct standard payload
        String payload = "{\"temp\":" + String(currentTemp) + 
                         ",\"depth\":" + String(currentDepth) + 
                         ",\"turbidity\":" + String(currentTurbidity) + 
                         ",\"v_bat\":" + String(currentBatteryV) + "}";

        // 1. Serial (Always On) Telemetry Push
        Serial.println("TELEMETRY_TX: " + payload);

        // 2. MQTT Async Push (Skips processing cleanly if link doesn't exist)
        if (mqttClient.connected()) {
            mqttClient.publish("pelvyn/auv/node01/telemetry", payload.c_str());
            digitalWrite(PIN_STATUS_LED, !digitalRead(PIN_STATUS_LED)); // Toggle LED upon successful active broadcast
        }

        // 3. RS485 Main Vehicle Expansion Push
        transmitRS485(payload);
    }

    mqttClient.loop();
}

/**
 * Safely transmits a payload over the half-duplex RS485 bus.
 */
void transmitRS485(const String& data) {
    // 1. Claim the bus (Transmit mode)
    digitalWrite(PIN_RS485_CTRL, HIGH);
    
    // 2. Write the data
    Serial2.println(data);
    
    // 3. Block until hardware buffer is physically empty
    Serial2.flush(); 
    
    // 4. Release the bus (Receive mode)
    digitalWrite(PIN_RS485_CTRL, LOW);
}

void initializeSensors() {
    // 1. Temperature Sensor Initialization Setup
    tempSensor.begin();
    if (tempSensor.getDeviceCount() > 0) {
        tempSensorActive = true;
        SystemLogger::logEvent("INIT", "DS18B20 temperature sensor detected.");
    } else {
        tempSensorActive = false;
        SystemLogger::logEvent("TEMP_MISSING", "DS18B20 temp probe absent on boot.");
    }

    // 2. I2C Initialization for Depth Sensor
    Wire.begin();
    Wire.beginTransmission(0x76); // MS5837-30BA Address
    if (Wire.endTransmission() == 0) {
        pressureSensorActive = true;
        SystemLogger::logEvent("INIT", "MS5837-30BA depth sensor found.");
    } else {
        pressureSensorActive = false;
        SystemLogger::logEvent("PRESSURE_MISSING", "MS5837-30BA sensor absent on boot.");
    }
}

void processSensors() {
    // --- TEMPERATURE PROCESSING ---
    if (tempSensorActive) {
        tempSensor.requestTemperatures();
        float rawT = tempSensor.getTempCByIndex(0);
        if (rawT != DEVICE_DISCONNECTED_C) {
            currentTemp = rawT; // Retained if read validated
        } else {
            // Drop mid-mission handling: holds last valid item but logs incident
            SystemLogger::logEvent("WARN", "DS18B20 disconnected mid-mission.");
        }
    }

    // --- DEPTH / PRESSURE PROCESSING ---
    if (pressureSensorActive) {
        // Simulating decoding data sequence over I2C.
        // For physical device implementation, actual library calls replace this step.
        float rawDepth = 12.5f; 
        currentDepth = depthFilter.update(rawDepth);

        if (currentDepth > DEPTH_MAX_M) {
            SystemLogger::logEvent("CRITICAL", "Depth envelope exceeded threshold!");
            executeSafeState("OVER_DEPTH");
        }
    }

    // --- TURBIDITY ADC PROCESSING ---
    int rawTurbidityADC = analogRead(PIN_TURBIDITY_ADC);
    float processedTurbidityVal = (rawTurbidityADC / 4095.0f) * 100.0f; // Simplified scaling parameter
    currentTurbidity = turbidityFilter.update(processedTurbidityVal);

    // --- BATTERY FUEL GAUGING VIA ADC ---
    int rawBatADC = analogRead(PIN_BATTERY_ADC);
    float rawVolts = (rawBatADC / 4095.0f) * 3.3f * 2.5f; // Scaling calculations for external voltage divider circuit
    currentBatteryV = batteryFilter.update(rawVolts);

    if (currentBatteryV <= BATTERY_CRITICAL_V) {
        SystemLogger::logEvent("CRITICAL", "Battery exhausted. Emergency mode forced.");
        executeSafeState("BATTERY_CRITICAL");
    } else if (currentBatteryV <= BATTERY_LOW_V) {
        static bool lowBatLogged = false;
        if (!lowBatLogged) {
            SystemLogger::logEvent("BATTERY_LOW", "Pack depletion warning.");
            lowBatLogged = true;
        }
    }

    // --- DEBOUNCED LEAK INGRESS DETECTOR ---
    if (digitalRead(PIN_LEAK_DIGITAL) == LOW) { // Low indicates short across the active-low pulled-up contacts
        leakDebounceCounter++;
        if (leakDebounceCounter >= 3) { // 3 continuous cycles validation
            SystemLogger::logEvent("CRITICAL", "Water ingress confirmed inside hull!");
            executeSafeState("LEAK_DETECTED");
        }
    } else {
        leakDebounceCounter = 0; // Clear immediately if noise clears
    }
}

void checkNetworkStateMachine() {
    // Non-blocking checking state framework
    if (WiFi.status() != WL_CONNECTED) {
        static unsigned long lastConnectAttempt = 0;
        if (millis() - lastConnectAttempt > 10000) { // Check pacing boundaries every 10s asynchronously
            lastConnectAttempt = millis();
            WiFi.begin("Pelvyn_Surface_Buoy", "UnderwaterSecurePass");
        }
        return; // Early breakout, avoids downstream processing bottlenecks
    }

    // Handle MQTT connectivity if network layer active
    if (!mqttClient.connected()) {
        static unsigned long lastMqttAttempt = 0;
        if (millis() - lastMqttAttempt > 8000) {
            lastMqttAttempt = millis();
            if (mqttClient.connect("Pelvyn_Node_01")) {
                SystemLogger::logEvent("NET", "MQTT link established with Topsides.");
            }
        }
    }
}

void executeSafeState(const char* context) {
    SystemLogger::logEvent("HALT", context);
    
    // Shut down functional modems to prevent hazardous power draws
    WiFi.disconnect(true);
    
    // Solid Status indicator illumination signaling failure lockout
    digitalWrite(PIN_STATUS_LED, HIGH);

    // Endless low-power preservation safety state. MCU relies entirely on internal or external 
    // physical physical buoyancy measures for recovery. Hardware Watchdog handles hard locks.
    while (true) {
        esp_task_wdt_reset(); // Keep watchdog alive if we are intentionally parked here
        delay(500);
    }
}