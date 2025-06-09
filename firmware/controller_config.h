/*
 * ESP32-S3 Unrailed Controller Configuration
 * Hardware: NanoS3/Tiny Pico Nano
 */

#ifndef CONTROLLER_CONFIG_H
#define CONTROLLER_CONFIG_H

// Hardware version
#define HARDWARE_VERSION "NanoS3/Tiny Pico Nano"
#define FIRMWARE_VERSION "1.0.0"

// Pin definitions for ESP32-S3
#define LED_LV_PIN      35  // Status LED
#define BAT_STAT_PIN    9   // Battery Status Input
#define BAT_SENSE_PIN   1   // Battery Voltage ADC (100k/100k divider)
#define USE_PIN         21  // Button A (USE)
#define DASH_PIN        18  // Button B (DASH)
#define SEL_PIN         17  // Select Button
#define USB_5V_SENSE    16  // USB 5V Detection
#define BOOST_EN_PIN    15  // 5V Boost Enable
#define DIR_H_PIN       6   // Joystick Horizontal (X-axis)
#define DIR_V_PIN       5   // Joystick Vertical (Y-axis)

// Controller settings
#define CONTROLLER_NAME "Unrailed Controller"
#define MANUFACTURER    "ESP32"

// Joystick configuration
#define JOYSTICK_DEADZONE     100   // ADC units
#define JOYSTICK_CENTER       2048  // 12-bit ADC center
#define JOYSTICK_MAX_RANGE    2048  // Maximum deviation from center

// Battery configuration
#define BATTERY_CHECK_INTERVAL 5000  // milliseconds
#define BATTERY_MIN_VOLTAGE    3.0   // Volts
#define BATTERY_MAX_VOLTAGE    4.2   // Volts (fully charged Li-Po)
#define VOLTAGE_DIVIDER_RATIO  2.0   // 100k/100k = 2:1 ratio

// LED patterns
#define LED_BLINK_INTERVAL    500   // milliseconds (when not connected)
#define LED_STARTUP_BLINKS    3     // Number of blinks on startup

// Deep sleep configuration
#define SLEEP_TIMEOUT_MS         300000  // 5 minutes of inactivity before sleep
#define LOW_BATTERY_SLEEP_MS     30000   // 30 seconds between wake cycles when low battery
#define ACTIVITY_THRESHOLD       50      // Joystick movement threshold to detect activity
#define LOW_BATTERY_THRESHOLD    15      // Battery percentage threshold for extended sleep

// Wake up sources - Require ALL 3 buttons pressed simultaneously
#define WAKE_BUTTON_BITMASK      0x2A0000  // USE (GPIO21), DASH (GPIO18), and SEL (GPIO17) buttons
// Bitmask calculation: (1ULL << USE_PIN) | (1ULL << DASH_PIN) | (1ULL << SEL_PIN) 
// = (1ULL << 21) | (1ULL << 18) | (1ULL << 17) = 0x2A0000

// Button mapping for Unrailed game
typedef enum {
    GAMEPAD_BUTTON_A = 1,      // USE button -> A
    GAMEPAD_BUTTON_B = 2,      // DASH button -> B
    GAMEPAD_BUTTON_SELECT = 8  // SEL button -> Select
} GamepadButtons;

// Joystick mapping
typedef enum {
    JOYSTICK_LEFT_X = 0,   // DIR_H -> Left stick X
    JOYSTICK_LEFT_Y = 1    // DIR_V -> Left stick Y (inverted)
} JoystickAxes;

#endif // CONTROLLER_CONFIG_H
