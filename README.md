# FPV Dronen Racing Gate mit LEDs

## Features

- **LED Steuerung**:
  - Nutzt die [FastLED Bibliothek](https://github.com/FastLED/FastLED) für LED-Animationen.
  - Unterstützt die [WS2812 LEDs](https://www.fastled.io/) mit 75 LEDs auf D4 (GPIO2).
  - Verschiedene Animationsmuster und Farbpaletten werden unterstützt und können durch Knopfdruck geändert werden.
  - Automatische Umschaltung der Farbpaletten und Muster nach einer bestimmten Zeit.
  
- **Tastensteuerung**:
  - Tasten zur Steuerung von:
    - **Helligkeit** der LEDs.
    - **Muster** (Animationen).
    - **Paletten** (Farbpaletten).
  
- **Batterieüberwachung**:
  - Überwacht die Spannung der Batterie mit einem Analogwert von A0.
  - Wenn die Spannung unter einen bestimmten Schwellenwert fällt, wird der Zustand "Low Battery" angezeigt (mit roten LEDs).
  
- **Ultraschall-Sensor**:
  - Verwendet einen HC-SR04 Ultraschallsensor zur Erkennung von Distanzen (z. B. für das Durchfliegen von Drohnen).
  - Setzt einen Timeout, wenn keine Drohne innerhalb eines festgelegten Zeitrahmens erkannt wird.
  
- **Zustandsverwaltung**:
  - Unterstützt mehrere Zustände:
    - **Normalbetrieb**: Standardmuster und Animationen.
    - **Niedrige Batterie**: Anzeige eines speziellen Musters für niedrige Batterie.
    - **Drohne erkannt**: Starten eines speziellen Effekts, wenn eine Drohne erkannt wird.
    - **Schlafmodus**: Setzt den ESP in den Tiefschlaf, um Strom zu sparen.
    - **Ready**: Setzt eines speziellen Effekts, der anzeigt, dass das Gate Ready ist
  
- **Automatische Effekte**:
  - Wenn keine Eingabe erfolgt, werden Muster und Farbpaletten automatisch rotiert.
  
- **Energieverwaltung**:
  - Geht in den **Deep Sleep** Modus, wenn keine Aktivität erkannt wird und kann durch Tasten oder Interrupts wieder aufgeweckt werden.
  
- **Zustandswechsel**:
  - Der ESP wechselt automatisch zwischen den verschiedenen Zuständen je nach Eingabe/Durchflug und Batterielevel.

## Installation

1. Installiere die [FastLED Bibliothek](https://github.com/FastLED/FastLED) über den Arduino Library Manager.
2. Schließe deine WS2812 LED-Streifen an den D4 Pin (GPIO2) an.
3. Verbinde den HC-SR04 Ultraschallsensor mit den Pins `TRIGPIN` (D2) und `ECHOPIN` (D1).
4. Verwende die Tasten für die Steuerung der Helligkeit, Muster und Paletten:
   - **PIN_BUTTON_PATTERN** (D3): Zum Wechseln des Musters.
   - **PIN_BUTTON_BRIGHTNESS** (D8): Zum Anpassen der Helligkeit.
   - **PIN_BUTTON_PALETTE** (D99): Zum Wechseln der Farbpalette.
   
## Pinbelegung

| Funktion                  | Pin       |
|---------------------------|-----------|
| **Button für Muster**      | GPIO0 (D3)|
| **Button für Helligkeit**  | GPIO15 (D8)|
| **Button für Palette**     | GPIO99    |
| **Batterieanzeige**        | A0        |
| **LEDs**                   | GPIO2 (D4)|
| **HC-SR04 Trigger Pin**    | GPIO4 (D2)|
| **HC-SR04 Echo Pin**       | GPIO5 (D1)|
| **Status-LED**             | GPIO13    |

## Code Funktionen

### `nextBrightness()`
- Wechselt zwischen den Helligkeitsstufen (0, 64, 128, 255) und passt die Helligkeit der LEDs an.

### `nextPattern()`
- Wechselt zum nächsten Animationsmuster.

### `nextPalette()`
- Wechselt die Farbpalette, wenn das aktuelle Muster eine Palette enthält.

### `getBatteryVoltage()`
- Liest den aktuellen Batterie-Spannungswert und berechnet die tatsächliche Batteriespannung.

### `checkDistanceThreshold()`
- Überprüft, ob eine Drohne innerhalb einer definierten Distanz erkannt wurde, basierend auf dem Ultraschallsensor.

### `handleInput()`
- Überwacht die Tasten und führt je nach Eingabe die entsprechenden Funktionen aus (z. B. Wechsel von Muster, Palette, Helligkeit).

### `handleLowBattery()`
- Zeigt den Zustand "Low Battery" an, indem alle LEDs rot leuchten.

### `handleSleep()`
- Versetzt den ESP in den Tiefschlafmodus und kann durch Interrupts geweckt werden.

### `handleDroneDetected()`
- Führt einen speziellen Effekt aus, wenn eine Drohne erkannt wurde.

### `handleGateReady()`
- Führt einen speziellen Effekt aus, wenn das Gate bereit ist.

### `handleNormalOperation()`
- Zeigt das aktuelle Muster und die Farbpalette an und aktualisiert diese regelmäßig.

## Zustände

- **STATE_NORMAL**: Normaler Betriebsmodus.
- **STATE_LOW_BATTERY**: Anzeige eines Low-Battery-Zustands.
- **STATE_DRONE_DETECTED**: Wenn eine Drohne erkannt wird, wird ein spezieller Effekt angezeigt.
- **STATE_SLEEP**: Der ESP geht in den Schlafmodus.
- **STATE_READY**: Das Gate ist bereit, es wird ein spezieller Effekt angezeigt.

## Zusätzliche Hinweise

- Der Code nutzt Deep Sleep, um den Stromverbrauch zu minimieren.
- Tasten und Interrupts ermöglichen eine benutzerfreundliche Steuerung.
- Die Spannung wird kontinuierlich überwacht, um den Zustand "Low Battery" rechtzeitig anzuzeigen.
- Der Ultraschallsensor wird genutzt, um Drohnen zu erkennen und spezielle Effekte zu aktivieren.