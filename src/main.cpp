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

#define PIN_BUTTON_PATTERN 0 // GPIO0 auf NodeMCU ist D3 (PIN 0) --> Flash Button fuer BRIDHTNESS verwenden
#define PIN_BUTTON_PALETTE 99
#define PIN_BUTTON_BRIGHTNESS 15 //D8

#define BAT_VOLTAGE_PIN A0
#define VOLTAGE_FACTOR 5  //Resistors Ration Factor
#define NUM_VOLTAGE_VALUES 10  // Anzahl der gespeicherten Spannungswerte
#define SWITCH_OFF_VOLTAGE 0.000001 //ganz klein sonst leuchtet der Ring rot, wenn alles nur uber usb laeuft

//#define CLK_PIN     2
//#define LED_TYPE    APA102
#define LED_PIN 2 //D4
#define LED_TYPE    WS2812
#define COLOR_ORDER GRB
#define NUM_LEDS    75 //10mm breite Leds

#define FRAMES_PER_SECOND  120

uint8_t cyclePattern = 1;
uint16_t cyclePatternDuration = 20; //sec
//Effekte automatisch durchschallten (cyclePalettes) bei 1, Zeitdauer festlegen (paletteDuration)
uint8_t cyclePalette = 1;
uint16_t paletteDuration = 7; //sec

const uint8_t PIN_LED_STATUS = 13;

#define TRIGPIN 4 //D2
#define ECHOPIN 5 //D1
#define DRONEDETECTTIMEOUT 5 //5 sec

//#define GATESWITCHOFFTIME 30 //30 sec nach letzten Dronendruchflug
//-------------------------------------------------------

uint8_t brightnesses[] = { 255, 128, 64, 0 };
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

enum State {
  STATE_NORMAL,
  STATE_LOW_BATTERY,
  STATE_DRONE_DETECTED,
  STATE_SLEEP
};

State currentState = STATE_NORMAL;

unsigned long stateStartTime = 0;
unsigned long stateDuration = 0; // Dauer des aktuellen Zustands in Millisekunden

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

// ISR-Funktion im IRAM speichern
void IRAM_ATTR wakeUp() {
  // Code, der beim Interrupt ausgeführt wird
  // Zum Beispiel einen Zustand setzen oder einen anderen Mechanismus auslösen
  Serial.println("Wakeup triggered!");
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
  Serial.print("#Pattern#: ");
  //Serial.println(currentPatternIndex);
  Serial.println(patternNames[currentPatternIndex]);
}

void nextPalette()
{
  if (strstr(patternNames[currentPatternIndex], "contains_Palette") != NULL) {
    // add one to the current palette number, and wrap around at the end
    currentPaletteIndex = (currentPaletteIndex + 1) % paletteCount;
    Serial.print("*Palette*: ");
    Serial.println(paletteNames[currentPaletteIndex]);
  } else {
    Serial.println("*Palette* wird nicht umgeschalten, da aktuell kein Effekt mit einer aktiven Palette vorhanden ist.");
  }
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

bool checkDistanceThreshold(float threshold) {
  // Define sound velocity in cm/us and conversion factor for cm to inches
  float SOUND_VELOCITY = 0.034;
  long duration;
  float distanceCm;

  // Clears the trigPin
  digitalWrite(TRIGPIN, LOW);
  delayMicroseconds(2);

  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(TRIGPIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGPIN, LOW);

  // Reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(ECHOPIN, HIGH);
  // Calculate the distance in cm
  distanceCm = duration * SOUND_VELOCITY / 2;

  // Check if the distance is greater than the threshold
  if (distanceCm <= threshold && distanceCm > 0 ) {
    //Serial.print("Distance (cm): ");
    //Serial.println(distanceCm);
    return true;  // True if the distance is less than or equal to the threshold
  } else if (distanceCm == 0) {
    Serial.println("HC-SR04 not working");
    return false; // False if the distance is greater than the threshold
  } else {
    return false; // False if the distance is greater than the threshold
  }
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

void handleLowBattery() {
  Serial.flush();
  fill_solid(leds, NUM_LEDS, CRGB::Red);
  FastLED.show();
}

void handleSleep() {
  Serial.println("Going down...");
  Serial.flush();

  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

// Interrupts für Wakeup einrichten
  pinMode(PIN_BUTTON_PATTERN, INPUT_PULLUP);  
  pinMode(PIN_BUTTON_BRIGHTNESS, INPUT_PULLUP);  
  attachInterrupt(PIN_BUTTON_PATTERN, wakeUp, FALLING);   // Flash-Button für Pattern-Wakeup
  attachInterrupt(PIN_BUTTON_BRIGHTNESS, wakeUp, FALLING); // Brightness-Button für Wakeup
  //attachInterrupt(PIN_BUTTON_PALETTE, wakeUp, FALLING);  // Optional: Palette-Button für Wakeup

  // Deep Sleep einleiten (oder ein ähnlicher Stromsparmodus)
  ESP.deepSleep(0); // Deep Sleep-Modus ohne Timer, wird durch Interrupt geweckt

  // WICHTIG: Der Code nach deepSleep wird nur ausgeführt, wenn der ESP geweckt wird
  Serial.println("I have awoken!");

  // Interrupts deaktivieren, wenn sie nicht mehr benötigt werden
  detachInterrupt(PIN_BUTTON_PATTERN);
  detachInterrupt(PIN_BUTTON_BRIGHTNESS);
  //detachInterrupt(PIN_BUTTON_PALETTE);

  Serial.println("I have awoken!");
  nextBrightness();
}

void handleDroneDetected() {
  u_int8_t specialpatternsnumber = 0; //droneDetected();
  // Call the current pattern function once, updating the 'leds' array
  specialpatterns[specialpatternsnumber]();
  currentPalette = palettes[currentPaletteIndex];
  FastLED.show();
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}

void handleGateSwitchOff() {
  u_int8_t specialpatternsnumber = 2; //gateReady();
  // Call the current pattern function once, updating the 'leds' array
  specialpatterns[specialpatternsnumber]();
  currentPalette = palettes[currentPaletteIndex];
  FastLED.show();
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}

void handleNormalOperation() {
  // Call the current pattern function once, updating the 'leds' array
    // Call the current pattern function once, updating the 'leds' array
  patterns[currentPatternIndex]();
  currentPalette = palettes[currentPaletteIndex];
  FastLED.show();
  FastLED.delay(1000 / FRAMES_PER_SECOND);

  // Update the hue and blend palettes every 40ms
  EVERY_N_MILLISECONDS(40) {
    hue++;
    nblendPaletteTowardPalette(currentPalette, targetPalette, 8);
  }

  // Increment the fast hue every 4ms
  EVERY_N_MILLISECONDS(4) {
    hueFast++;
  }

  if (cyclePattern == 1 && (millis() > cyclePatternTimeout)) {
    nextPattern();
    cyclePatternTimeout = millis() + (cyclePatternDuration * 1000);
  }

  if (cyclePalette == 1 && (millis() > paletteTimeout)) {
    nextPalette();
    paletteTimeout = millis() + (paletteDuration * 1000);
  }

}

State changeState(State newState, unsigned long duration = 0) {
  // Wenn sich der Zustand geändert hat, setze den neuen Zustand und gebe eine Nachricht aus
  if (currentState != newState) {
    currentState = newState;
    stateStartTime = millis(); // Zeitstempel setzen
    stateDuration = duration*1000;  // Dauer speichern

    // Ausgabe der entsprechenden Nachricht basierend auf dem neuen Zustand
    switch (newState) {
      case STATE_LOW_BATTERY:
        Serial.println("STATE_LOW_BATTERY");
        break;
      case STATE_SLEEP:
        Serial.println("STATE_SLEEP");
        break;
      case STATE_DRONE_DETECTED:
        Serial.println("STATE_DRONE_DETECTED");
        break;
      case STATE_NORMAL:
      default:
        Serial.println("STATE_NORMAL");
        break;
      // Weitere Fälle nach Bedarf hinzufügen
    }
  }
  return newState;
}

void updateState() {
  // Zustandswechsel nur prüfen, wenn keine Zeitbegrenzung aktiv ist
  if (stateDuration == 0 || millis() - stateStartTime >= stateDuration) {
    // Prüfen, ob die Batterie zu niedrig ist
    if (getBatteryVoltage() <= SWITCH_OFF_VOLTAGE) {
      changeState(STATE_LOW_BATTERY);
    } 
    // Überprüfen, ob eine Drohne erkannt wurde
    else if (checkDistanceThreshold(30)) {
      changeState(STATE_DRONE_DETECTED, DRONEDETECTTIMEOUT);
    } 
    // Wenn die Helligkeit 0 ist, in den Schlafmodus wechseln
    else if (brightnesses[currentBrightnessIndex] == 0) {
      changeState(STATE_SLEEP);
    } else {
      changeState(STATE_NORMAL);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("setup");
  
  delay(3000); // 3 second delay for recovery

  pinMode(TRIGPIN, OUTPUT); // Sets the trigPin as an Output
  pinMode(ECHOPIN, INPUT); // Sets the echoPin as an Input

  setNumLeds(NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);

  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // set master brightness control
  FastLED.setBrightness(brightnesses[currentBrightnessIndex]);

  FastLED.show(CRGB::Black);

  buttonPattern.begin();
  buttonPalette.begin();
  buttonBrightness.begin();

  pinMode(PIN_LED_STATUS, OUTPUT);
  digitalWrite(PIN_LED_STATUS, LOW);
}

void loop()
{
  updateState();

  switch (currentState) {
    case STATE_LOW_BATTERY:
      handleLowBattery();
      break;
    case STATE_SLEEP:
      handleSleep();
      break;
    case STATE_DRONE_DETECTED:
      handleDroneDetected();
      break;
    case STATE_NORMAL:
    default:
      handleNormalOperation();
      break;
  }

  // Verarbeite Benutzereingaben, falls der Zustand dies zulässt
  handleInput();
}



