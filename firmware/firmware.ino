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

BleGamepad bleGamepad(CONTROLLER_NAME, MANUFACTURER, 100);

void setup()
{
    Serial.begin(115200);
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
    
    // Calibrate joystick center position
    calibrateJoystick();
    
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
