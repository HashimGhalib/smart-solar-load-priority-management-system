#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- ESP32 Pin Settings ---
#define PIN_BATT_SENSE    34
#define PIN_CURR_SENSE    32
#define PIN_RELAY_MEDIUM  18  // Switches Medium Load Relay & Yellow LED
#define PIN_RELAY_LOW     19  // Switches Low Load Relay & Blue LED
#define PIN_ALARM         5   // Warning Buzzer

// --- Telemetry Constants ---
const float ADC_RES = 4095.0f;
const float V_REF = 3.3f;
const float VOLT_DIVIDER = 6.0f; // Multiplier scales standard 3.3V max up to 19.8V max
const float MAX_CURRENT = 30.0f; // Scale mapping for simulated 0-30A load current

// --- Battery Management Thresholds (12V Nominal Battery) ---
const float BATT_CRITICAL   = 11.0f; // Cutoff for all non-essential loads + Alert
const float BATT_MEDIUM_OK   = 12.0f; // Threshold to permit Medium-Priority load
const float BATT_SUFFICIENT  = 13.0f; // Threshold to permit Low-Priority load
const float CURRENT_LIMIT    = 20.0f; // Overcurrent limit protection

LiquidCrystal_I2C lcd(0x27, 16, 2);
unsigned long lastUpdate = 0;

void setup() {
    Serial.begin(115200);
    
    // Define I/O states
    pinMode(PIN_RELAY_MEDIUM, OUTPUT);
    pinMode(PIN_RELAY_LOW, OUTPUT);
    pinMode(PIN_ALARM, OUTPUT);

    // Turn off relays and warning alarms on startup
    digitalWrite(PIN_RELAY_MEDIUM, LOW);
    digitalWrite(PIN_RELAY_LOW, LOW);
    digitalWrite(PIN_ALARM, LOW);

    // Initialize display with standard ESP32 I2C pins (SDA=21, SCL=22)
    Wire.begin(21, 22);
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Priority System");
    lcd.setCursor(0, 1);
    lcd.print("ESP32 Initialized");
    delay(2000);
    lcd.clear();
}

void loop() {
    // Read raw 12-bit ADC values (0-4095)
    int rawVolt = analogRead(PIN_BATT_SENSE);
    int rawCurr = analogRead(PIN_CURR_SENSE);

    // Compute voltage and current metrics
    float battVoltage = (rawVolt / ADC_RES) * V_REF * VOLT_DIVIDER;
    float totalCurrent = (rawCurr / ADC_RES) * MAX_CURRENT;

    bool mediumLoadState = false;
    bool lowLoadState = false;
    bool alarmState = false;

    // --- Dynamic Priority Load Controller ---
    if (totalCurrent > CURRENT_LIMIT) {
        // OVERLOAD STATE: Shed all non-essential loads immediately
        mediumLoadState = false;
        lowLoadState = false;
        alarmState = true;
    } else {
        // VOLTAGE STATE: Analyze battery reserves
        if (battVoltage >= BATT_SUFFICIENT) {
            mediumLoadState = true;  // Low, Medium, and High are ON
            lowLoadState = true;
        } else if (battVoltage >= BATT_MEDIUM_OK) {
            mediumLoadState = true;  // Shed low-priority load
            lowLoadState = false;
        } else {
            // Below 12V: Critical load-shedding stage
            mediumLoadState = false; 
            lowLoadState = false;
            if (battVoltage < BATT_CRITICAL) {
                alarmState = true;   // Low voltage alarm active
            }
        }
    }

    // Write physical states to pins
    digitalWrite(PIN_RELAY_MEDIUM, mediumLoadState ? HIGH : LOW);
    digitalWrite(PIN_RELAY_LOW, lowLoadState ? HIGH : LOW);
    digitalWrite(PIN_ALARM, alarmState ? HIGH : LOW);

    // Telemetry display cycle (once per second)
    if (millis() - lastUpdate >= 1000) {
        lastUpdate = millis();

        // Trace to serial console
        Serial.printf("Batt: %.2fV | Curr: %.2fA | High: ON | Med: %s | Low: %s | State: %s\n",
                      battVoltage, totalCurrent,
                      mediumLoadState ? "ON" : "OFF",
                      lowLoadState ? "ON" : "OFF",
                      alarmState ? "ALARM" : "NORMAL");

        // Write metrics on LCD Screen
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("V:");
        lcd.print(battVoltage, 1);
        lcd.print("V I:");
        lcd.print(totalCurrent, 1);
        lcd.print("A");

        lcd.setCursor(0, 1);
        // Display High-priority (H) as always ON, Medium (M) and Low (L) as ON or OFF
        lcd.print("H:ON M:");
        lcd.print(mediumLoadState ? "ON " : "OFF"); // Prints "ON " (with space) or "OFF" to keep spacing consistent
        lcd.print(" L:");
        lcd.print(lowLoadState ? "ON" : "OFF");
    }
}