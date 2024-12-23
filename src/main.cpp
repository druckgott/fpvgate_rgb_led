#include <FastLED.h>  // https://github.com/FastLED/FastLED
#include <Button.h>   // https://github.com/madleech/Button

#include "Palettes.h"

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
CRGB leds[NUM_LEDS];

#define FRAMES_PER_SECOND  120

uint8_t cyclePattern = 1;
uint16_t cyclePatternDuration = 60; //sec
//Effekte automatisch durchschallten (cyclePalettes) bei 1, Zeitdauer festlegen (paletteDuration)
uint8_t cyclePalette = 1;
uint16_t paletteDuration = 23; //sec

const uint8_t PIN_LED_STATUS = 13;

uint8_t brightnesses[] = { 255, 128, 64, 1 };
uint8_t currentBrightnessIndex = 0;

uint8_t currentPatternIndex = 0; // Index number of which pattern is current
uint8_t hue = 0; // rotating "base color" used by many of the patterns
uint8_t hueFast = 0; // faster rotating "base color" used by many of the patterns

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

// returns an 8-bit value that
// rises and falls in a sawtooth wave, 'BPM' times per minute,
// between the values of 'low' and 'high'.
//
//       /|      /|
//     /  |    /  |
//   /    |  /    |
// /      |/      |
//
uint8_t beatsaw8( accum88 beats_per_minute, uint8_t lowest = 0, uint8_t highest = 255,
                  uint32_t timebase = 0, uint8_t phase_offset = 0)
{
  uint8_t beat = beat8( beats_per_minute, timebase);
  uint8_t beatsaw = beat + phase_offset;
  uint8_t rangewidth = highest - lowest;
  uint8_t scaledbeat = scale8( beatsaw, rangewidth);
  uint8_t result = lowest + scaledbeat;
  return result;
}

void rainbow()
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, hue, 7);
}

void addGlitter( fract8 chanceOfGlitter)
{
  if ( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

void rainbowWithGlitter()
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
}

void confetti()
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( hue + random8(64), 200, 255);
}

// Updated sinelon (no visual gaps) by Mark Kriegsman
// https://gist.github.com/kriegsman/261beecba85519f4f934
void sinelon()
{
  // a colored dot sweeping
  // back and forth, with
  // fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  CHSV color = CHSV(hue, 220, 255);
  //int pos = beatsin16(120, 0, NUM_LEDS - 1);
  int pos = beatsin16(80, 0, NUM_LEDS - 1);
  static int prevpos = 0;
  if ( pos < prevpos ) {
    fill_solid(leds + pos, (prevpos - pos) + 1, color);
  } else {
    fill_solid(leds + prevpos, (pos - prevpos) + 1, color);
  }
  prevpos = pos;
}

void juggle() {
  const uint8_t dotCount = 3;
  uint8_t step = 192 / dotCount;
  // colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for (int i = 0; i < dotCount; i++) {
    CRGB color = CHSV(dothue, 255, 255);
    if (dotCount == 3) {
      if (i == 1) {
        color = CRGB::Green;
      }
      else if (i == 2) {
        color = CRGB::Blue;
      }
    }
    leds[beatsin16(i + 15, 0, NUM_LEDS)] += color;
    dothue += step;
  }
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = RainbowColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for ( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, hue + (i * 2), beat - hue + (i * 10));
  }
}

void solidRainbow()
{
  fill_solid(leds, NUM_LEDS, CHSV(hue, 255, 255));
}

void solidPalette()
{
  fill_solid(leds, NUM_LEDS, ColorFromPalette(currentPalette, hue));
}

void chaseRainbow() {
  static uint8_t h = 0;

  for (uint16_t i = NUM_LEDS - 1; i > 0; i--) {
    leds[i] = leds[i - 1];
  }
  leds[0] = ColorFromPalette(RainbowStripeColors_p, h++);
}

void chaseRainbow2() {
  const uint8_t step = 255 / NUM_LEDS;

  for ( int i = 0; i < NUM_LEDS; i++) {
    leds[i] = ColorFromPalette(RainbowStripeColors_p, hueFast + i * step);
  }
}

void chasePalette() {
  for (uint16_t i = NUM_LEDS - 1; i > 0; i--) {
    leds[i] = leds[i - 1];
  }
  leds[0] = ColorFromPalette(currentPalette, hue);
}

void chasePalette2() {
  const uint8_t step = 255 / NUM_LEDS;

  for ( int i = 0; i < NUM_LEDS; i++) {
    leds[i] = ColorFromPalette(currentPalette, hueFast + i * step);
  }
}

// Pride2015 by Mark Kriegsman: https://gist.github.com/kriegsman/964de772d64c502760e5
// This function draws rainbows with an ever-changing,
// widely-varying set of parameters.
void pride()
{
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 1, 3000);

  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5, 9);
  uint16_t brightnesstheta16 = sPseudotime;

  for ( uint16_t i = 0 ; i < NUM_LEDS; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    CRGB newcolor = CHSV( hue8, sat8, bri8);

    uint16_t pixelnumber = i;
    pixelnumber = (NUM_LEDS - 1) - pixelnumber;

    nblend( leds[pixelnumber], newcolor, 64);
  }
}

// ColorWavesWithPalettes by Mark Kriegsman: https://gist.github.com/kriegsman/8281905786e8b2632aeb
// This function draws color waves with an ever-changing,
// widely-varying set of parameters, using a color palette.
void colorwaves( CRGB* ledarray, uint16_t numleds, CRGBPalette16& palette)
{
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  // uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 300, 1500);

  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5, 9);
  uint16_t brightnesstheta16 = sPseudotime;

  for ( uint16_t i = 0 ; i < numleds; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;
    uint16_t h16_128 = hue16 >> 7;
    if ( h16_128 & 0x100) {
      hue8 = 255 - (h16_128 >> 1);
    } else {
      hue8 = h16_128 >> 1;
    }

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    uint8_t index = hue8;
    //index = triwave8( index);
    index = scale8( index, 240);

    CRGB newcolor = ColorFromPalette( palette, index, bri8);

    uint16_t pixelnumber = i;
    pixelnumber = (numleds - 1) - pixelnumber;

    nblend( ledarray[pixelnumber], newcolor, 128);
  }
}

void colorWaves()
{
  colorwaves(leds, NUM_LEDS, currentPalette);
}

// Zeitsteuerung
unsigned long lastUpdate = 0;
unsigned long updateInterval = 30; // Interval für alle Effekte

// Sine-Wave Berechnung
uint8_t sineWave(uint8_t x) {
  return (sin((x * 3.14159) / 128) + 1) * 127.5;  // Mapping der Sine-Welle auf 0-255
}

uint8_t runningLightsCounter = 0;  // Schrittzähler für Running Lights

// Running Lights-Effekt
void runningLights() {
    uint8_t size = 10;  // Adjust for smoothness
    uint8_t sineIncr = (255 / NUM_LEDS) * size;
    sineIncr = sineIncr > 1 ? sineIncr : 1;

    for (int i = 0; i < NUM_LEDS; i++) {
      uint8_t lum = sineWave((i + runningLightsCounter) * sineIncr);
      CRGB color = ColorFromPalette(RainbowColors_p, lum);
      leds[i] = color;
    }

    runningLightsCounter++;
    if (runningLightsCounter == 255) runningLightsCounter = 0;

    FastLED.show();
}

// Hyper Sparkle-Effekt
void hyperSparkle() {
  CRGB color = ColorFromPalette(currentPalette, hue);
    fill_solid(leds, NUM_LEDS, color);
    uint8_t size = 1;
    for (int i = 0; i < 8; i++) {
      int pos = random(NUM_LEDS - size);
      fill_solid(leds + pos, size, CRGB::White);
    }
    FastLED.show();
}

uint8_t cometPosition = 0;  // Position des Kometen
// Comet-Effekt
void cometEffect() {
    uint8_t cometSpeed = 6;     // Geschwindigkeit des Kometen
    CRGB color = ColorFromPalette(currentPalette, hue);
    lastUpdate = millis();
    fadeToBlackBy(leds, NUM_LEDS, 2);  // Fade with a value of 20 (can be adjusted)
    leds[cometPosition] = color;
    cometPosition = (cometPosition + cometSpeed) % NUM_LEDS;
    FastLED.show();
}

// Funktion, um Feuerwerks-Explosionen zu simulieren
void fireworkEffect() {
    CRGB color = ColorFromPalette(currentPalette, hue);
    lastUpdate = millis();
    // Fade des gesamten LED-Strips
    fadeToBlackBy(leds, NUM_LEDS, 20);  // Fade mit einem Wert von 10 (kann angepasst werden)
    // Bestimme eine zufällige Anzahl an Funken
    uint8_t numSparks = random(10, 30);  // Anzahl der Funken
    // Erstelle Funken an zufälligen Positionen
    for (int i = 0; i < numSparks; i++) {
      int pos = random(NUM_LEDS);  // Zufällige Position für den Funken
      leds[pos] = color;           // Setze die Farbe an der Position
    }
    FastLED.show();  // LEDs anzeigen
}

void fireChaseEffect() {
  const uint8_t step = 255 / NUM_LEDS; // Schrittweite für Farbverlauf
  static uint8_t hueOffset = 0;        // Offset für Farbverschiebung
  for (int i = 0; i < NUM_LEDS / 2; i++) {
    // Effekt von der Mitte nach außen (links und rechts)
    leds[i] = ColorFromPalette(HeatColors_p, hueOffset + i * step);
    leds[NUM_LEDS - 1 - i] = ColorFromPalette(HeatColors_p, hueOffset + i * step);
  }
  // Mitte füllen, falls ungerade Anzahl LEDs
  if (NUM_LEDS % 2 == 1) {
    leds[NUM_LEDS / 2] = ColorFromPalette(HeatColors_p, hueOffset);
  }
  hueOffset += 2; // Farbe animieren (je nach Geschwindigkeit anpassen)
  FastLED.show();
}

// List of patterns to cycle through.  Each is defined as a separate function below.
typedef void (*SimplePatternList[])();
SimplePatternList patterns = {
  pride,
  colorWaves,
  chaseRainbow,
  chaseRainbow2,
  chasePalette,
  chasePalette2,
  solidPalette,
  solidRainbow,
  rainbow,
  rainbowWithGlitter,
  confetti,
  sinelon,
  juggle,
  bpm,
  runningLights,
  hyperSparkle,
  cometEffect,
  fireworkEffect,
  fireChaseEffect
};

// Array mit den Namen der Muster (manuell zugeordnet)
const char* patternNames[] = {
  "Pride",
  "Color Waves currentPalette",
  "Chase Rainbow",
  "Chase Rainbow 2",
  "Chase Palette currentPalette",
  "Chase Palette 2 currentPalette",
  "Solid Palette",
  "Solid Rainbow currentPalette",
  "Rainbow",
  "Rainbow with Glitter",
  "Confetti",
  "Sinelon",
  "Juggle",
  "BPM",
  "RunningLights",
  "HyperSparkle currentPalette",
  "CometEffect currentPalette",
  "FireworkEffect currentPalette",
  "FireChaseEffect"
};


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
  currentPatternIndex = (currentPatternIndex + 1) % ARRAY_SIZE(patterns);
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
  
  // delay(3000); // 3 second delay for recovery

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



