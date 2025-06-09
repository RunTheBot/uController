/*
 * ESP32-S3 Bluetooth Controller Firmware for Unrailed
 * NanoS3/Tiny Pico Nano Compatible
 * 
 * Pin Configuration:
 * LED_LV      IO35 - Status LED
 * BAT_STAT    IO9  - Battery Status
 * BAT_SENSE   IO1  - Battery Voltage (Analog, 100k/100k divider)
 * USE         IO21 - Button A
 * DASH        IO18 - Button B
 * SEL         IO17 - Select Button
 * 5V_SENSE    IO16 - USB 5V Detection
 * BOOST_EN    IO15 - 5V Boost Enable
 * DIR_H       IO6  - Analog Joystick Horizontal (Left Stick X)
 * DIR_V       IO5  - Analog Joystick Vertical (Left Stick Y)
 * 
 * Controller Mapping:
 * - Analog joystick -> Left stick (X, Y)
 * - USE button -> A button (BUTTON_1)
 * - DASH button -> B button (BUTTON_2)
 * - SEL button -> Select
 */

#include <Arduino.h>
#include <BleGamepad.h>
#include "controller_config.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"

// Pin definitions - now using config file
// (All pin definitions moved to controller_config.h)

// Button states
bool useButtonState = false;
bool dashButtonState = false;
bool selButtonState = false;
bool lastUseButtonState = false;
bool lastDashButtonState = false;
bool lastSelButtonState = false;

// Joystick calibration
int16_t joystickCenterX = JOYSTICK_CENTER;
int16_t joystickCenterY = JOYSTICK_CENTER;
int16_t joystickDeadzone = JOYSTICK_DEADZONE;

// Battery monitoring
float batteryVoltage = 0.0;
uint8_t batteryPercentage = 100;
unsigned long lastBatteryCheck = 0;
const unsigned long batteryCheckInterval = BATTERY_CHECK_INTERVAL;

// Sleep management
unsigned long lastActivityTime = 0;
bool lowBatteryMode = false;
int16_t lastJoystickX = 0;
int16_t lastJoystickY = 0;
RTC_DATA_ATTR int bootCount = 0;

BleGamepad bleGamepad(CONTROLLER_NAME, MANUFACTURER, 100);

void setup()
{
    Serial.begin(115200);
    
    // Check wake up reason
    bootCount++;
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    
    Serial.printf("Boot count: %d\n", bootCount);
    Serial.printf("Wake up reason: %d\n", wakeup_reason);
    
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
        Serial.println("Woke up from button press");
    } else {
        Serial.println("Fresh boot or unknown wake reason");
    }
    
    Serial.println("Starting Unrailed Controller...");
    
    // Initialize pins
    pinMode(LED_LV_PIN, OUTPUT);
    pinMode(BAT_STAT_PIN, INPUT);
    pinMode(USE_PIN, INPUT_PULLUP);
    pinMode(DASH_PIN, INPUT_PULLUP);
    pinMode(SEL_PIN, INPUT_PULLUP);
    pinMode(USB_5V_SENSE, INPUT);
    pinMode(BOOST_EN_PIN, OUTPUT);
    
    // Enable boost for 5V output when on battery
    digitalWrite(BOOST_EN_PIN, HIGH);
    
    // LED startup sequence
    for(int i = 0; i < LED_STARTUP_BLINKS; i++) {
        digitalWrite(LED_LV_PIN, HIGH);
        delay(200);
        digitalWrite(LED_LV_PIN, LOW);
        delay(200);
    }
    
    // Configure wake up sources for deep sleep
    configureSleepWakeup();
    
    // Calibrate joystick center position
    calibrateJoystick();
    
    // Initialize activity timer
    lastActivityTime = millis();
    
    // Start BLE gamepad
    bleGamepad.begin();
    Serial.println("Controller initialized!");
}

void loop()
{
    // Read button states (active low with pullup)
    useButtonState = !digitalRead(USE_PIN);
    dashButtonState = !digitalRead(DASH_PIN);
    selButtonState = !digitalRead(SEL_PIN);
    
    // Handle button presses/releases
    if (useButtonState != lastUseButtonState) {
        if (useButtonState) {
            bleGamepad.press(GAMEPAD_BUTTON_A);
            Serial.println("A button pressed");
            updateActivityTime();
        } else {
            bleGamepad.release(GAMEPAD_BUTTON_A);
            Serial.println("A button released");
        }
        lastUseButtonState = useButtonState;
    }
    
    if (dashButtonState != lastDashButtonState) {
        if (dashButtonState) {
            bleGamepad.press(GAMEPAD_BUTTON_B);
            Serial.println("B button pressed");
            updateActivityTime();
        } else {
            bleGamepad.release(GAMEPAD_BUTTON_B);
            Serial.println("B button released");
        }
        lastDashButtonState = dashButtonState;
    }
    
    if (selButtonState != lastSelButtonState) {
        if (selButtonState) {
            bleGamepad.pressSelect();
            Serial.println("Select button pressed");
            updateActivityTime();
        } else {
            bleGamepad.releaseSelect();
            Serial.println("Select button released");
        }
        lastSelButtonState = selButtonState;
    }
    
    // Read and process joystick
    processJoystick();
    
    // Battery monitoring
    if (millis() - lastBatteryCheck > batteryCheckInterval) {
        checkBattery();
        lastBatteryCheck = millis();
    }
    
    // Update LED status
    updateStatusLED();
    
    // Check for sleep conditions
    checkSleepConditions();
    
    delay(10); // Small delay for stability
}

void calibrateJoystick() {
    Serial.println("Calibrating joystick center position...");
    
    // Take multiple readings to get stable center values
    long totalX = 0, totalY = 0;
    int samples = 100;
    
    for (int i = 0; i < samples; i++) {
        totalX += analogRead(DIR_H_PIN);
        totalY += analogRead(DIR_V_PIN);
        delay(10);
    }
    
    joystickCenterX = totalX / samples;
    joystickCenterY = totalY / samples;
    
    Serial.printf("Joystick center: X=%d, Y=%d\n", joystickCenterX, joystickCenterY);
}

void processJoystick() {
    // Read raw ADC values (0-4095 for 12-bit)
    int rawX = analogRead(DIR_H_PIN);
    int rawY = analogRead(DIR_V_PIN);
    
    // Calculate relative position from center
    int deltaX = rawX - joystickCenterX;
    int deltaY = rawY - joystickCenterY;
    
    // Apply deadzone
    if (abs(deltaX) < joystickDeadzone) deltaX = 0;
    if (abs(deltaY) < joystickDeadzone) deltaY = 0;
    
    // Convert to gamepad range (-32768 to 32767)
    int16_t gamepadX = map(deltaX, -2048, 2047, -32768, 32767);
    int16_t gamepadY = map(deltaY, -2048, 2047, -32768, 32767);
    
    // Invert Y axis (joystick up = negative Y in gamepad coordinates)
    gamepadY = -gamepadY;
    
    // Check for joystick activity
    if (abs(gamepadX - lastJoystickX) > ACTIVITY_THRESHOLD || 
        abs(gamepadY - lastJoystickY) > ACTIVITY_THRESHOLD) {
        updateActivityTime();
        lastJoystickX = gamepadX;
        lastJoystickY = gamepadY;
    }
    
    // Set left stick position
    bleGamepad.setLeftThumb(gamepadX, gamepadY);
}

void checkBattery() {
    // Read battery voltage through voltage divider
    int adcValue = analogRead(BAT_SENSE_PIN);
    
    // Convert ADC to voltage (3.3V reference, 12-bit ADC, voltage divider)
    batteryVoltage = (adcValue * 3.3 * VOLTAGE_DIVIDER_RATIO) / 4095.0;
    
    // Convert voltage to percentage (approximate for Li-Po)
    batteryPercentage = constrain(map(batteryVoltage * 100, 
                                     BATTERY_MIN_VOLTAGE * 100, 
                                     BATTERY_MAX_VOLTAGE * 100, 
                                     0, 100), 0, 100);
    
    // Update gamepad battery level
    bleGamepad.setBatteryLevel(batteryPercentage);
    
    // Check for low battery mode
    if (batteryPercentage <= LOW_BATTERY_THRESHOLD && !digitalRead(USB_5V_SENSE)) {
        lowBatteryMode = true;
        Serial.println("Entering low battery mode");
    } else {
        lowBatteryMode = false;
    }
    
    // Check if USB power is available
    bool usbPowered = digitalRead(USB_5V_SENSE);
    
    // Enable/disable boost based on USB power
    if (usbPowered) {
        digitalWrite(BOOST_EN_PIN, LOW);  // Disable boost when USB powered
    } else {
        digitalWrite(BOOST_EN_PIN, HIGH); // Enable boost on battery
    }
    
    Serial.printf("Battery: %.2fV (%d%%) USB: %s\n", 
                  batteryVoltage, batteryPercentage, usbPowered ? "Yes" : "No");
}

void updateStatusLED() {
    static unsigned long lastLedUpdate = 0;
    static bool ledState = false;
    
    if (bleGamepad.isConnected()) {
        // Solid on when connected
        digitalWrite(LED_LV_PIN, HIGH);
    } else {
        // Blink when not connected
        if (millis() - lastLedUpdate > LED_BLINK_INTERVAL) {
            ledState = !ledState;
            digitalWrite(LED_LV_PIN, ledState);
            lastLedUpdate = millis();
        }
    }
}

// Deep sleep and power management functions

void updateActivityTime() {
    lastActivityTime = millis();
}

void configureSleepWakeup() {
    // Configure wake up sources
    // Wake up ONLY when ALL 3 buttons are pressed simultaneously (WAKEUP_ALL_LOW)
    esp_sleep_enable_ext1_wakeup(WAKE_BUTTON_BITMASK, ESP_EXT1_WAKEUP_ALL_LOW);
    
    // Configure RTC IO for wake up buttons (pullup for ALL_LOW mode)
    rtc_gpio_pullup_en((gpio_num_t)USE_PIN);
    rtc_gpio_pullup_en((gpio_num_t)DASH_PIN);
    rtc_gpio_pullup_en((gpio_num_t)SEL_PIN);
    
    Serial.println("Sleep wake-up sources configured - requires all 3 buttons pressed");
}

void checkSleepConditions() {
    unsigned long inactiveTime = millis() - lastActivityTime;
    bool usbConnected = digitalRead(USB_5V_SENSE);
    
    // Don't sleep if USB is connected (for debugging/charging)
    if (usbConnected) {
        return;  
    }
    
    // Check if we should enter deep sleep
    bool shouldSleep = false;
    
    if (lowBatteryMode) {
        // In low battery mode, sleep more aggressively
        if (inactiveTime > 30000) { // 30 seconds
            shouldSleep = true;
            Serial.println("Entering deep sleep - low battery mode");
        }
    } else if (!bleGamepad.isConnected()) {
        // If not connected, sleep after 2 minutes
        if (inactiveTime > 120000) { // 2 minutes
            shouldSleep = true;  
            Serial.println("Entering deep sleep - not connected");
        }
    } else {
        // If connected but inactive, sleep after 5 minutes
        if (inactiveTime > SLEEP_TIMEOUT_MS) {
            shouldSleep = true;
            Serial.println("Entering deep sleep - inactive while connected");
        }
    }
    
    if (shouldSleep) {
        enterDeepSleep();
    }
}

void enterDeepSleep() {
    Serial.println("Preparing for deep sleep...");
    
    // Flash LED to indicate sleep
    for (int i = 0; i < 5; i++) {
        digitalWrite(LED_LV_PIN, HIGH);
        delay(100);
        digitalWrite(LED_LV_PIN, LOW);
        delay(100);
    }
    
    // Disconnect BLE gracefully
    if (bleGamepad.isConnected()) {
        Serial.println("Disconnecting BLE...");
        bleGamepad.end();
        delay(1000); // Give time for disconnection
    }
    
    // Turn off boost converter to save power
    digitalWrite(BOOST_EN_PIN, LOW);
    
    // Turn off LED
    digitalWrite(LED_LV_PIN, LOW);
    
    Serial.println("Entering deep sleep now...");
    Serial.flush(); // Ensure all serial data is sent
    
    // Enter deep sleep
    esp_deep_sleep_start();
}

void handleWakeup() {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    
    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT1:
            Serial.println("Wake up from button press");
            // Reset activity timer since user pressed a button
            updateActivityTime();
            break;
            
        default:
            Serial.println("Wake up from unknown source");
            break;
    }
}
