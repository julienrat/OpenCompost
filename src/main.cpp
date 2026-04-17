#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <FastLED.h>

#define BMP280_ADDR 0x76
#define ONE_WIRE_BUS 29
#define LED_PIN 16
#define NUM_LEDS 1

volatile uint8_t registers[256];
volatile uint8_t current_reg = 0;
CRGB leds[NUM_LEDS];
volatile bool i2c_activity = false;
bool sensor_error = false;
unsigned long lastMeasureTime = 0;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void setup_registers() {
    memset((void*)registers, 0, 256);
    registers[0xD0] = 0x58; // ID BMP280

    // --- NOUVELLE CALIBRATION DE TEMPÉRATURE ---
    // Formule maître : T = [(ADC/16384 - T1/1024) * T2] / 5120
    // Avec T1=32768 et T2=20480, on obtient : T = ADC/4096 - 128
    registers[0x88] = 0x00; registers[0x89] = 0x80; // T1 = 32768 (Unsigned)
    registers[0x8A] = 0x00; registers[0x8B] = 0x50; // T2 = 20480 (Signed)
    registers[0x8C] = 0x00; registers[0x8D] = 0x00; // T3 = 0 (Désactivé)

    // --- CALIBRATION DE PRESSION (Fixe) ---
    registers[0x8E] = 0xE8; registers[0x8F] = 0x03; // P1 = 1000
    // P2 à P9 restent à 0 pour figer la pression
    for (int i = 0x90; i <= 0x9F; i++) registers[i] = 0x00; 

    // Pression brute fixe
    registers[0xF7] = 0x80; 
    registers[0xF8] = 0x00; 
    registers[0xF9] = 0x00; 
}

void update_sensor_data() {
    leds[0] = CRGB::Purple; // Indicateur de lecture DS18B20
    FastLED.show();
    delay(500); // Petit délai pour visualiser la lecture
    leds[0] = CRGB::Black;
    FastLED.show();

    sensors.requestTemperatures();
    float tempC = sensors.getTempCByIndex(0);

    if (tempC != DEVICE_DISCONNECTED_C) {
        sensor_error = false;
        // On sature la valeur pour rester dans la plage de calibration
        tempC = constrain(tempC, -128.0f, 127.0f);
        
        // Calcul inverse : ADC_T = (TempC + 128) * 4096
        uint32_t raw = (uint32_t)((tempC + 128.0f) * 4096.0f);
        
        registers[0xFA] = (raw >> 12) & 0xFF; // MSB
        registers[0xFB] = (raw >> 4) & 0xFF;  // LSB
        registers[0xFC] = (raw << 4) & 0xF0;  // XLSB
        leds[0] = CRGB::Black;
    } else {
        sensor_error = true;
    }
}

void onReceive(int len) {
    if (len > 0) {
        current_reg = Wire.read();
        // Consommer le reste si nécessaire
        while (Wire.available()) Wire.read();
    }
}

void onRequest() {
    i2c_activity = true;
    // Calcul du nombre d'octets restants pour éviter de lire hors du tableau
    int available = 256 - current_reg;
    int to_send = (available < 6) ? available : 6;
    
    if (to_send > 0) {
        Wire.write((const uint8_t*)&registers[current_reg], to_send);
    }
}

void setup() {
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(50);
    leds[0] = CRGB::Black;
    FastLED.show();
    
    Wire.setSDA(0);
    Wire.setSCL(1);
    setup_registers();
    sensors.begin();
    sensors.setWaitForConversion(false); // Mode non-bloquant
    Wire.begin(BMP280_ADDR);
    Wire.onReceive(onReceive);
    Wire.onRequest(onRequest);
    
    update_sensor_data(); // Première lecture au démarrage
}

void loop() {
    // Lecture toutes les 2 minutes (120000 ms)
    if (millis() - lastMeasureTime >= 5000) {
        lastMeasureTime = millis();
        update_sensor_data();
    }

    // Gestion de l'affichage LED hors interruption
    if (i2c_activity) {
        i2c_activity = false;
        leds[0] = CRGB::Green;
        FastLED.show();
        delay(10); // Petit délai visuel
        leds[0] = CRGB::Black;
        FastLED.show();
    } else if (sensor_error) {
        leds[0] = CRGB::Red;
        FastLED.show();
    }
}