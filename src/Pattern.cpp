#include "Palettes.h"

uint8_t hue = 0; // rotating "base color" used by many of the patterns
uint8_t hueFast = 0; // faster rotating "base color" used by many of the patterns
uint8_t numleds = 10;
CRGB* leds = nullptr;  // Pointer für das LED-Array

void setNumLeds(int num) {
    numleds = num;  // Aktualisiere die Anzahl der LEDs

    // Falls bereits Speicher allokiert wurde, freigeben
    if (leds != nullptr) {
        delete[] leds;
    }

    // Dynamisch Speicher für das LED-Array allokieren
    leds = new CRGB[numleds];
    Serial.print("Led Anzahl angepasst: ");
    Serial.println(numleds);
}

//CRGB leds[numleds];
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
  fill_rainbow(leds, numleds, hue, 7);
}

void addGlitter( fract8 chanceOfGlitter)
{
  if ( random8() < chanceOfGlitter) {
    leds[ random16(numleds) ] += CRGB::White;
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
  fadeToBlackBy( leds, numleds, 10);
  int pos = random16(numleds);
  leds[pos] += CHSV( hue + random8(64), 200, 255);
}

// Updated sinelon (no visual gaps) by Mark Kriegsman
// https://gist.github.com/kriegsman/261beecba85519f4f934
void sinelon()
{
  // a colored dot sweeping
  // back and forth, with
  // fading trails
  fadeToBlackBy( leds, numleds, 20);
  CHSV color = CHSV(hue, 220, 255);
  //int pos = beatsin16(120, 0, numleds - 1);
  int pos = beatsin16(80, 0, numleds - 1);
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
  fadeToBlackBy( leds, numleds, 20);
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
    leds[beatsin16(i + 15, 0, numleds)] += color;
    dothue += step;
  }
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = RainbowColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for ( int i = 0; i < numleds; i++) { //9948
    leds[i] = ColorFromPalette(palette, hue + (i * 2), beat - hue + (i * 10));
  }
}

void solidRainbow()
{
  fill_solid(leds, numleds, CHSV(hue, 255, 255));
}

void solidPalette()
{
  fill_solid(leds, numleds, ColorFromPalette(currentPalette, hue));
}

void chaseRainbow() {
  static uint8_t h = 0;

  for (uint16_t i = numleds - 1; i > 0; i--) {
    leds[i] = leds[i - 1];
  }
  leds[0] = ColorFromPalette(RainbowStripeColors_p, h++);
}

void chaseRainbow2() {
  const uint8_t step = 255 / numleds;

  for ( int i = 0; i < numleds; i++) {
    leds[i] = ColorFromPalette(RainbowStripeColors_p, hueFast + i * step);
  }
}

void chasePalette() {
  for (uint16_t i = numleds - 1; i > 0; i--) {
    leds[i] = leds[i - 1];
  }
  leds[0] = ColorFromPalette(currentPalette, hue);
}

void chasePalette2() {
  const uint8_t step = 255 / numleds;

  for ( int i = 0; i < numleds; i++) {
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

  for ( uint16_t i = 0 ; i < numleds; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    CRGB newcolor = CHSV( hue8, sat8, bri8);

    uint16_t pixelnumber = i;
    pixelnumber = (numleds - 1) - pixelnumber;

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
  colorwaves(leds, numleds, currentPalette);
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
    uint8_t size = 1;  // Adjust for smoothness
    uint8_t sineIncr = (255 / numleds) * size;
    sineIncr = sineIncr > 1 ? sineIncr : 1;

    for (int i = 0; i < numleds; i++) {
      uint8_t lum = sineWave((i + runningLightsCounter) * sineIncr);
      CRGB color = ColorFromPalette(RainbowColors_p, lum);
      leds[i] = color;
    }

    runningLightsCounter++;
    if (runningLightsCounter == 255) runningLightsCounter = 0;
}

// Hyper Sparkle-Effekt
void hyperSparkle() {
  CRGB color = ColorFromPalette(currentPalette, hue);
    fill_solid(leds, numleds, color);
    uint8_t size = 1;
    for (uint8_t i = 0; i < 8; i++) {
      uint8_t pos = random(numleds - size);
      fill_solid(leds + pos, size, CRGB::White);
    }
}

uint8_t cometPosition = 0;  // Position des Kometen
// Comet-Effekt
void cometEffect() {
    uint8_t cometSpeed = 6;     // Geschwindigkeit des Kometen
    CRGB color = ColorFromPalette(currentPalette, hue);
    lastUpdate = millis();
    fadeToBlackBy(leds, numleds, 2);  // Fade with a value of 20 (can be adjusted)
    leds[cometPosition] = color;
    cometPosition = (cometPosition + cometSpeed) % numleds;
}

// Funktion, um Feuerwerks-Explosionen zu simulieren
void fireworkEffect() {
    CRGB color = ColorFromPalette(currentPalette, hue);
    lastUpdate = millis();
    // Fade des gesamten LED-Strips
    fadeToBlackBy(leds, numleds, 20);  // Fade mit einem Wert von 10 (kann angepasst werden)
    // Bestimme eine zufällige Anzahl an Funken
    uint8_t numSparks = random(10, 30);  // Anzahl der Funken
    // Erstelle Funken an zufälligen Positionen
    for (uint8_t i = 0; i < numSparks; i++) {
      uint8_t pos = random(numleds);  // Zufällige Position für den Funken
      leds[pos] = color;           // Setze die Farbe an der Position
    }
}

void fireChaseEffect() {
  const uint8_t step = 255 / numleds; // Schrittweite für Farbverlauf
  static uint8_t hueOffset = 0;        // Offset für Farbverschiebung
  for (uint8_t i = 0; i < numleds / 2; i++) {
    // Effekt von der Mitte nach außen (links und rechts)
    leds[i] = ColorFromPalette(HeatColors_p, hueOffset + i * step);
    leds[numleds - 1 - i] = ColorFromPalette(HeatColors_p, hueOffset + i * step);
  }
  // Mitte füllen, falls ungerade Anzahl LEDs
  if (numleds % 2 == 1) {
    leds[numleds / 2] = ColorFromPalette(HeatColors_p, hueOffset);
  }
  hueOffset += 2; // Farbe animieren (je nach Geschwindigkeit anpassen)
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
  "Color Waves contains_Palette",
  "Chase Rainbow",
  "Chase Rainbow 2",
  "Chase Palette contains_Palette",
  "Chase Palette 2 contains_Palette",
  "Solid Palette",
  "Solid Rainbow contains_Palette",
  "Rainbow",
  "Rainbow with Glitter",
  "Confetti",
  "Sinelon",
  "Juggle",
  "BPM",
  "RunningLights",
  "HyperSparkle contains_Palette",
  "CometEffect contains_Palette",
  "FireworkEffect contains_Palette",
  "FireChaseEffect"
};

uint8_t patternsCount = sizeof(patterns) / sizeof(patterns[0]);


uint8_t runningLightsCounterx = 0;  // Schrittzähler für Running Lights

void droneDetected() {
    uint8_t numBeams = 4;        // Anzahl der Strahlen
    uint8_t beamWidth = 4;       // Breite jedes Strahls (in LEDs)
    uint8_t rotationSpeed = 1;   // Geschwindigkeit der Rotation
    uint8_t angleOffset = numleds / numBeams; // Gleichmäßiger Abstand zwischen den Strahlen

    // Neon-Style-Farben
    CRGB colors[] = {CRGB::Cyan, CRGB::Magenta, CRGB::Lime, CRGB::Orange};

    // LEDs löschen, damit alte Effekte verschwinden
    fill_solid(leds, numleds, CRGB::Black);

    // Berechne Position und zeichne Strahlen
    for (uint8_t beam = 0; beam < numBeams; beam++) {
        int startPos = (runningLightsCounterx + beam * angleOffset) % numleds;

        for (int i = 0; i < beamWidth; i++) {
            int ledIndex = (startPos + i) % numleds; // Stelle sicher, dass Index gültig ist
            leds[ledIndex] = colors[beam % numBeams]; // Wähle passende Neonfarbe
        }
    }

    // Animation weiterdrehen
    runningLightsCounterx += rotationSpeed;
    if (runningLightsCounterx >= numleds) runningLightsCounterx = 0;
}

/*void droneDetected() {
    uint8_t size = 2;  // Adjust for smoothness
    uint8_t sineIncr = (255 / numleds) * size;
    sineIncr = sineIncr > 1 ? sineIncr : 1;

    for (int i = 0; i < numleds; i++) {
      uint8_t lum = sineWave((i + runningLightsCounterx) * sineIncr);
      CRGB color = ColorFromPalette(RainbowColors_p, lum);
      leds[i] = color;
    }

    runningLightsCounterx++;
    if (runningLightsCounterx == 255) runningLightsCounterx = 0;
}*/

void batteryEmpty() {
  uint8_t numMiddleLEDs = 4;
  // Alle LEDs ausschalten
  fill_solid(leds, numleds, CRGB::Black);

  // Berechnung der mittleren LEDs
  uint8_t midStart = (numleds / 2) - (numMiddleLEDs / 2); // Index der ersten mittleren LED
  uint8_t midEnd = midStart + numMiddleLEDs;              // Index der letzten mittleren LED

  // Mittlere LEDs rot leuchten lassen
  for (uint8_t i = midStart; i < midEnd && i < numleds; i++) {
    leds[i] = CRGB::Red;
  }
}

void batteryLow() {
  uint8_t numMiddleLEDs = 4;
  // Alle LEDs ausschalten
  fill_solid(leds, numleds, CRGB::Black);

  // Berechnung der mittleren LEDs
  uint8_t midStart = (numleds / 2) - (numMiddleLEDs / 2); // Index der ersten mittleren LED
  uint8_t midEnd = midStart + numMiddleLEDs;              // Index der letzten mittleren LED

  // Mittlere LEDs rot leuchten lassen
  for (uint8_t i = midStart; i < midEnd && i < numleds; i++) {
    leds[i] = CRGB::Orange;
  }
}

void gateReady() {
  uint8_t numMiddleLEDs = 4;
  // Alle LEDs ausschalten
  fill_solid(leds, numleds, CRGB::Black);

  // Berechnung der mittleren LEDs
  uint8_t midStart = (numleds / 2) - (numMiddleLEDs / 2); // Index der ersten mittleren LED
  uint8_t midEnd = midStart + numMiddleLEDs;              // Index der letzten mittleren LED

  // Mittlere LEDs rot leuchten lassen
  for (uint8_t i = midStart; i < midEnd && i < numleds; i++) {
    leds[i] = CRGB::Green;
  }
}

void game1() {
  uint8_t numMiddleLEDs = 10;
  // Alle LEDs ausschalten
  fill_solid(leds, numleds, CRGB::Black);

  // Berechnung der mittleren LEDs
  uint8_t midStart = (numleds / 2) - (numMiddleLEDs / 2); // Index der ersten mittleren LED
  uint8_t midEnd = midStart + numMiddleLEDs;              // Index der letzten mittleren LED

  // Mittlere LEDs rot leuchten lassen
  for (uint8_t i = midStart; i < midEnd && i < numleds; i++) {
    leds[i] = CRGB::Blue;
  }
}

// List of patterns to cycle through.  Each is defined as a separate function below.
typedef void (*SpecialPatternList[])();
SpecialPatternList specialpatterns = {
  droneDetected,
  batteryEmpty,
  gateReady,
  game1,
  batteryLow
};

// Array mit den Namen der Muster (manuell zugeordnet)
const char* specialpatternNames[] = {
  "droneDetected",
  "batteryEmpty",
  "gateReady",
  "Game1",
  "batteryLow"
};

uint8_t specialpatternsCount = sizeof(specialpatterns) / sizeof(specialpatterns[0]);