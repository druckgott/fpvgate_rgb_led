#ifndef PATTERN_H
#define PALETTES_H

#include <FastLED.h>  // Oder deine Palette-Bibliothek

void setNumLeds(int num);

// Muster-Funktionsprototypen
void pride();
void colorWaves();
void chaseRainbow();
void chaseRainbow2();
void chasePalette();
void chasePalette2();
void solidPalette();
void solidRainbow();
void rainbow();
void rainbowWithGlitter();
void confetti();
void sinelon();
void juggle();
void bpm();
void runningLights();
void hyperSparkle();
void cometEffect();
void fireworkEffect();
void fireChaseEffect();

//extern const uint8_t paletteCount;  // Deklaration von paletteCount
extern uint8_t hue; // rotating "base color" used by many of the patterns
extern uint8_t hueFast; // faster rotating "base color" used by many of the patterns
extern uint8_t numleds;
//extern CRGB leds[];
extern CRGB* leds;
extern uint8_t patternsCount;
extern const char* patternNames[];
typedef void (*SimplePatternList[])();
extern SimplePatternList patterns ;

#endif