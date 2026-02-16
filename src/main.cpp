#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define I2C_ADDR 0x18
#define DS18B20_PIN 29

// Tableau fixe des registres MCP9808
uint16_t registers[0x09]; // 0x00 à 0x08

volatile uint8_t currentRegister = 0;

// Timer pour mise à jour température DS18B20
unsigned long lastTempUpdate = 0;
const unsigned long TEMP_UPDATE_MS = 120000;

OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

// NeoPixel
#include <FastLED.h>
#define NEOPIXEL_PIN 16
#define NUMPIXELS 1
CRGB pixels[NUMPIXELS];
uint32_t ledUntil = 0; // timestamp until which LED stays green
uint8_t ledState = 0; // 0=off,1=green,2=purple
const uint32_t LED_PULSE_MS = 1000;
const uint32_t MEASURE_PULSE_MS = 1000;
const uint32_t ERROR_BLINK_MS = 500;
bool errorActive = false;
uint32_t lastErrorBlink = 0;
bool errorBlinkOn = false;
uint32_t measureUntil = 0; // timestamp until which LED stays purple

static void setLedColor(const CRGB &color, uint8_t state) {
  if (ledState != state) {
    pixels[0] = color;
    FastLED.show();
    ledState = state;
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // Initialisation des registres
  registers[0x01] = 0x0000;                 // CONFIG
  // valeur initiale provisoire, sera remplacée par un random au démarrage
  registers[0x05] = 0x0000;                 // AMBIENT TEMP (raw)
  registers[0x06] = 0x0054;                 // MANUF_ID
  registers[0x07] = 0x0400;                 // DEVICE_ID
  registers[0x08] = 0x0003;                 // RESOLUTION (0x03 -> 0.0625°C)

  // Initialise la librairie Wire en mode esclave
  Wire.begin(I2C_ADDR);

  Wire.onReceive([](int len){
    if(len >= 1){
      int reg = Wire.read();
      // s'assurer que c'est un registre valide
      if(reg < (int)(sizeof(registers)/sizeof(registers[0])))
        currentRegister = reg;
    }
  });

  Wire.onRequest([](){
    uint16_t val = 0;
    if(currentRegister < (int)(sizeof(registers)/sizeof(registers[0])))
      val = registers[currentRegister];
    Wire.write((uint8_t)(val >> 8));    // MSB
    Wire.write((uint8_t)(val & 0xFF));  // LSB
    // Si on a lu le registre de température (0x05), allumer LED verte brièvement
    if (currentRegister == 0x05) {
      ledUntil = millis() + LED_PULSE_MS;
    }
  });

  // Initialiser DS18B20
  sensors.begin();
  sensors.setResolution(12);
  sensors.setWaitForConversion(true);

  // Initialiser NeoPixel
  FastLED.addLeds<WS2812, NEOPIXEL_PIN, GRB>(pixels, NUMPIXELS);
  pixels[0] = CRGB::Yellow;
  FastLED.show();
  delay(1000);
  pixels[0] = CRGB::Black;
  FastLED.show();

  Serial.println("MCP9808 Emulator (DS18B20 temps)");
}

void loop() {
  unsigned long now = millis();

  // Mettre à jour la température DS18B20 indépendamment des requêtes
  if (now - lastTempUpdate >= TEMP_UPDATE_MS) {
    lastTempUpdate = now;

    measureUntil = now + MEASURE_PULSE_MS;
    sensors.requestTemperatures();
    float temp = sensors.getTempCByIndex(0);

    if (temp != DEVICE_DISCONNECTED_C && temp > -100.0f && temp < 125.0f) {
      errorActive = false;
      // Conversion au format MCP9808 (0.0625°C par LSB, bit 12 = signe)
      int16_t raw = (int16_t)lroundf(temp / 0.0625f); // temp * 16
      if (raw < 0) {
        raw = (int16_t)(4096 + raw); // encode négatif sur 12 bits
      }
      registers[0x05] = (uint16_t)(raw & 0x0FFF); // AMBIENT_TEMP (sans flags)

      Serial.print("DS18B20 temp: "); Serial.print(temp, 2); Serial.print(" C (raw=0x");
      Serial.print(registers[0x05], HEX); Serial.println(")");
    } else {
      errorActive = true;
      Serial.println("DS18B20 non detecte / temperature invalide");
    }
  }

  // Gérer la LED : erreur rouge clignotante, sinon violet pendant la mesure,
  // sinon verte pendant la fenêtre ledUntil, sinon éteinte
  if (errorActive) {
    if (now - lastErrorBlink >= ERROR_BLINK_MS) {
      lastErrorBlink = now;
      errorBlinkOn = !errorBlinkOn;
      setLedColor(errorBlinkOn ? CRGB::Red : CRGB::Black, errorBlinkOn ? 3 : 0);
    }
  } else if (now < measureUntil) {
    setLedColor(CRGB::Purple, 2);
  } else if (millis() < ledUntil) {
    setLedColor(CRGB::Green, 1);
  } else {
    setLedColor(CRGB::Black, 0);
  }

  delay(10);
}
