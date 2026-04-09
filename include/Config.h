#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =====================   FEATURE TOGGLES  =============
#define USE_SD_CARD true   // Set to true to enable SD Card hardware
#define DEBUG_INPUT true   

// =====================   PIN MAP  =====================
#define ENCODER_CLK 32
#define ENCODER_DT 33
#define ENCODER_SW 35
#define BTN_LED_MODE 5
#define BTN_LED_SPEED 22
#define BTN_TOGGLE 21  
#define LED_PIN 17
#define NUM_LEDS 144

// I²S pins
#define I2S_BCLK 19
#define I2S_LRCK 25
#define I2S_DOUT 26

// =====================  MODE DEFINITIONS  =============
#define MODE_SD 0
#define MODE_BT 1

// =====================  BLE ID  =======================
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR1_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR2_UUID "e3223119-9445-4e96-a4a1-85358c4046a2"

// =====================  TIMING CONSTANTS  =============
#define DC_DELAY 500       
#define DBL_DELAY 400      
#define DC_DELAY_SPEED 400
#define SD_RETRY_COOLDOWN 2000

// =====================  ENUMS  ========================
enum CommandType {
    CMD_LIGHT,
    CMD_SPEED,
    CMD_EFFECT,
    CMD_UNKNOWN
};

enum EffectType {
    EFFECT_STATIC = 0,
    EFFECT_RAINBOW = 1,
    EFFECT_BREATHING = 2,
    EFFECT_SPARKLE = 3,
    EFFECT_COLOR_CYCLE = 4,
    EFFECT_STROBING = 5,
    EFFECT_STARRY_NIGHT = 6,
    EFFECT_MUSIC = 7,
    EFFECT_INVALID
};

#endif