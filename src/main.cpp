#include <FastLED.h>  // https://github.com/FastLED/FastLED
#include <Button.h>   // https://github.com/madleech/Button

#include "Palettes.h"
#include "Pattern.h"

FASTLED_USING_NAMESPACE

// FastLED "100-lines-of-code" demo reel, showing just a few
// of the kinds of animation patterns you can quickly and easily
// compose using FastLED.
//
// This example also shows one easy way to define multiple
// animations patterns and have them automatically rotate.
//
// -Mark Kriegsman, December 2014

#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001000)
#warning "Requires FastLED 3.1 or later; check github for latest code."
#endif

#define DATA_PIN 2 //D4
#define PIN_BUTTON_PATTERN 0 // GPIO0 auf NodeMCU ist D3 (PIN 0) --> Flash Button fuer BRIDHTNESS verwenden
#define PIN_BUTTON_PALETTE 99
#define PIN_BUTTON_BRIGHTNESS 5 //D1

#define BAT_VOLTAGE_PIN A0
#define VOLTAGE_FACTOR 5  //Resistors Ration Factor
#define NUM_VOLTAGE_VALUES 10  // Anzahl der gespeicherten Spannungswerte
#define SWITCH_OFF_VOLTAGE 0.000001 //ganz klein sonst leuchtet der Ring rot, wenn alles nur uber usb laeuft

//#define CLK_PIN     2
//#define LED_TYPE    APA102
#define LED_TYPE    WS2812
#define COLOR_ORDER GRB
#define NUM_LEDS    160

#define FRAMES_PER_SECOND  120

uint8_t cyclePattern = 1;
uint16_t cyclePatternDuration = 20; //sec
//Effekte automatisch durchschallten (cyclePalettes) bei 1, Zeitdauer festlegen (paletteDuration)
uint8_t cyclePalette = 1;
uint16_t paletteDuration = 7; //sec

const uint8_t PIN_LED_STATUS = 13;

uint8_t brightnesses[] = { 255, 128, 64, 1 };
uint8_t currentBrightnessIndex = 0;

uint8_t currentPatternIndex = 0; // Index number of which pattern is current

unsigned long cyclePatternTimeout = 0;
uint8_t currentPaletteIndex = 0;
unsigned long paletteTimeout = 0;

bool ledStatus = false;

float voltageBuffer[NUM_VOLTAGE_VALUES] = {0};  // Array für die letzten 10 Spannungswerte
int voltageBufferIndex = 0;                    // Aktueller Index im Array
float voltageSum = 0;                          // Laufende Summe der Spannungswerte

Button buttonBrightness(PIN_BUTTON_BRIGHTNESS);
Button buttonPattern(PIN_BUTTON_PATTERN);
Button buttonPalette(PIN_BUTTON_PALETTE);

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

void wakeUp()
{
  Serial.println("Waking up...");
}

void nextBrightness()
{
  // add one to the current brightness number, and wrap around at the end
  currentBrightnessIndex = (currentBrightnessIndex + 1) % ARRAY_SIZE(brightnesses);
  FastLED.setBrightness(brightnesses[currentBrightnessIndex]);
  Serial.print("brightness: ");
  Serial.println(brightnesses[currentBrightnessIndex]);
}

void nextPattern()
{
  // add one to the current pattern number, and wrap around at the end
  currentPatternIndex = (currentPatternIndex + 1) % patternsCount;
  Serial.print("Pattern: ");
  //Serial.println(currentPatternIndex);
  Serial.println(patternNames[currentPatternIndex]);
}

void nextPalette()
{
  // add one to the current palette number, and wrap around at the end
  currentPaletteIndex = (currentPaletteIndex + 1) % paletteCount;
  Serial.print("Palette: ");
  Serial.println(paletteNames[currentPaletteIndex]);
}

float getBatteryVoltage() {
    // Neuen Spannungswert lesen
    float newVoltageValue = analogRead(BAT_VOLTAGE_PIN);  // Analogen Spannungswert lesen
    // Ältesten Spannungswert von der Summe abziehen
    voltageSum -= voltageBuffer[voltageBufferIndex];
    // Neuen Spannungswert in das Array speichern
    voltageBuffer[voltageBufferIndex] = newVoltageValue;
    // Neuen Spannungswert zur Summe hinzufügen
    voltageSum += newVoltageValue;
    // Index auf den nächsten Platz verschieben (Ringpuffer)
    voltageBufferIndex = (voltageBufferIndex + 1) % NUM_VOLTAGE_VALUES;
    // Durchschnitt berechnen
    float averageVoltage = voltageSum / NUM_VOLTAGE_VALUES;
    // Spannung umrechnen
    float convertedVoltage = (averageVoltage / 1024.0) * 5.0;  // In 5V-Skala umrechnen
    float batteryVoltage = convertedVoltage * VOLTAGE_FACTOR; // Tatsächliche Batteriespannung
    // Debug-Ausgabe (optional)
    //Serial.print("batteryVoltage: ");
    //Serial.println(batteryVoltage);
    return batteryVoltage;
}

void setup() {
  Serial.begin(115200);
  Serial.println("setup");
  
  delay(3000); // 3 second delay for recovery

  setNumLeds(NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);

  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  //  FastLED.addLeds<LED_TYPE,DATA_PIN,CLK_PIN,COLOR_ORDER,DATA_RATE_MHZ(12)>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // set master brightness control
  FastLED.setBrightness(brightnesses[currentBrightnessIndex]);

  FastLED.show(CRGB::Black);

  buttonPattern.begin();
  buttonPalette.begin();
  buttonBrightness.begin();

  pinMode(PIN_LED_STATUS, OUTPUT);
  digitalWrite(PIN_LED_STATUS, LOW);
}

void handleInput() {
  if (buttonPattern.pressed()) {
    digitalWrite(PIN_LED_STATUS, HIGH);
  }
  else if (buttonPattern.released())
  {
    //autoplay cyclePattern deaktivieren
    cyclePattern = 0;
    ledStatus = !ledStatus;
    digitalWrite(PIN_LED_STATUS, LOW);
    nextPattern();
  }

  if (buttonPalette.pressed()) {
    digitalWrite(PIN_LED_STATUS, HIGH);
  }
  else if (buttonPalette.released())
  {
    //autoplay cyclePalette deaktivieren
    cyclePalette = 0;
    ledStatus = !ledStatus;
    digitalWrite(PIN_LED_STATUS, LOW);
    nextPalette();
  }

  if (buttonBrightness.pressed()) {
    digitalWrite(PIN_LED_STATUS, HIGH);
  }
  else if (buttonBrightness.released()) {
    ledStatus = !ledStatus;
    digitalWrite(PIN_LED_STATUS, LOW);
    nextBrightness();
  }
}

void loop()
{
   if(getBatteryVoltage() <= SWITCH_OFF_VOLTAGE){
    Serial.flush();
    fill_solid(leds, NUM_LEDS, CRGB::Red);
    FastLED.show();
  }else{
    // Call the current pattern function once, updating the 'leds' array
    patterns[currentPatternIndex]();
    currentPalette = palettes[currentPaletteIndex];

    // send the 'leds' array out to the actual LED strip
    FastLED.show();
    // insert a delay to keep the framerate modest
    FastLED.delay(1000 / FRAMES_PER_SECOND);

    // do some periodic updates
    EVERY_N_MILLISECONDS( 40 ) {
      hue++;  // slowly cycle the "base color" through the rainbow

      // slowly blend the current palette to the next
      nblendPaletteTowardPalette(currentPalette, targetPalette, 8);
    }

    EVERY_N_MILLISECONDS( 4 ) {
      hueFast++;  // slowly cycle the "base color" through the rainbow
    }

    if (cyclePattern == 1 && (millis() > cyclePatternTimeout)) {
      nextPattern();
      cyclePatternTimeout = millis() + (cyclePatternDuration * 1000);
    }

    if (cyclePalette == 1 && (millis() > paletteTimeout)) {
      nextPalette();
      paletteTimeout = millis() + (paletteDuration * 1000);
    }

    handleInput();
  }
 

  if (brightnesses[currentBrightnessIndex] == 0) {  
    Serial.println("Going down...");
    Serial.flush();
  
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    
    // Allow wake up pin to trigger interrupt on low.
    attachInterrupt(0, wakeUp, LOW);
    attachInterrupt(1, wakeUp, LOW);

    // Disable external pin interrupt on wake up pin.
    detachInterrupt(0);
    detachInterrupt(1);
  
    Serial.println("I have awoken!");
    
    nextBrightness();
  }
}



