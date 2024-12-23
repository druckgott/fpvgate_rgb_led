// From ColorWavesWithPalettes by Mark Kriegsman: https://gist.github.com/kriegsman/8281905786e8b2632aeb

// Gradient Color Palette definitions for 33 different cpt-city color palettes.
//    956 bytes of PROGMEM for all of the palettes together,
//   +618 bytes of PROGMEM for gradient palette code (AVR).
//  1,494 bytes total for all 34 color palettes and associated code.

// Gradient palette "ib_jul01_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/ing/xmas/tn/ib_jul01.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 16 bytes of program space.

#ifndef PALETTES_H
#define PALETTES_H

#include <FastLED.h>  // Oder deine Palette-Bibliothek

// Deklariere alle externen Paletten-Pointer
extern const TProgmemRGBGradientPaletteRef Sunset_Real_gp;
extern const TProgmemRGBGradientPaletteRef es_rivendell_15_gp;
extern const TProgmemRGBGradientPaletteRef es_ocean_breeze_036_gp;
extern const TProgmemRGBGradientPaletteRef rgi_15_gp;
extern const TProgmemRGBGradientPaletteRef retro2_16_gp;
extern const TProgmemRGBGradientPaletteRef Analogous_1_gp;
extern const TProgmemRGBGradientPaletteRef es_pinksplash_08_gp;
extern const TProgmemRGBGradientPaletteRef Coral_reef_gp;
extern const TProgmemRGBGradientPaletteRef es_ocean_breeze_068_gp;
extern const TProgmemRGBGradientPaletteRef es_pinksplash_07_gp;
extern const TProgmemRGBGradientPaletteRef es_vintage_01_gp;
extern const TProgmemRGBGradientPaletteRef departure_gp;
extern const TProgmemRGBGradientPaletteRef es_landscape_64_gp;
extern const TProgmemRGBGradientPaletteRef es_landscape_33_gp;
extern const TProgmemRGBGradientPaletteRef rainbowsherbet_gp;
extern const TProgmemRGBGradientPaletteRef gr65_hult_gp;
extern const TProgmemRGBGradientPaletteRef gr64_hult_gp;
extern const TProgmemRGBGradientPaletteRef GMT_drywet_gp;
extern const TProgmemRGBGradientPaletteRef ib_jul01_gp;
extern const TProgmemRGBGradientPaletteRef es_vintage_57_gp;
extern const TProgmemRGBGradientPaletteRef ib15_gp;
extern const TProgmemRGBGradientPaletteRef Fuschia_7_gp;
extern const TProgmemRGBGradientPaletteRef es_emerald_dragon_08_gp;
extern const TProgmemRGBGradientPaletteRef lava_gp;
extern const TProgmemRGBGradientPaletteRef fire_gp;
extern const TProgmemRGBGradientPaletteRef Colorfull_gp;
extern const TProgmemRGBGradientPaletteRef Magenta_Evening_gp;
extern const TProgmemRGBGradientPaletteRef Pink_Purple_gp;
extern const TProgmemRGBGradientPaletteRef es_autumn_19_gp;
extern const TProgmemRGBGradientPaletteRef BlacK_Blue_Magenta_White_gp;
extern const TProgmemRGBGradientPaletteRef BlacK_Magenta_Red_gp;
extern const TProgmemRGBGradientPaletteRef BlacK_Red_Magenta_Yellow_gp;
extern const TProgmemRGBGradientPaletteRef Blue_Cyan_Yellow_gp;

// Die Palette-Liste
extern TProgmemRGBGradientPaletteRef palettes[];
extern char* paletteNames[];

//extern const uint8_t paletteCount;  // Deklaration von paletteCount
extern uint8_t paletteCount; // Globale Variable deklarieren
extern CRGBPalette16 currentPalette;
extern CRGBPalette16 targetPalette;

#endif