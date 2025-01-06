#include <FastLED.h>  // https://github.com/FastLED/FastLED
#include <Button.h>   // https://github.com/madleech/Button

/*#ifdef ESP32
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#else
#include <ESP8266WiFi.h>
#include <espnow.h>
#endif*/

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

//Pins fuer Buttons
#define PIN_BUTTON_PATTERN 18 // D18 GPIO18
#define PIN_BUTTON_PALETTE 99
#define PIN_BUTTON_BRIGHTNESS 19 //D19 GIOP19

//Batterie limit --> es wird ein spezieller Effekt gewahlt der anzeigt das die Batterie leer ist (5 rote leds in der Mitte des Rings)
#define BAT_VOLTAGE_PIN 35 //D35 for ANALOG GIPO35
#define VOLTAGE_FACTOR 4.06  //Resistors Ration Factor
#define SWITCH_OFF_VOLTAGE 11.3 //11V fuer 3S
//#define BATTERYCHECKINVERTVAL 2000 //so lange wird der status Batterie Low mindestens gehalten
#define LOW_BATTERY_COUNT 150 //wenn 100x der Wert unter dem Switchoff wert hintereinander ist schaltet das Gate aus

//LED Config
//#define CLK_PIN     2
//#define LED_TYPE    APA102
#define LED_PIN 4 //D4 GIPO4
#define LED_TYPE    WS2812
#define COLOR_ORDER GRB
#define NUM_LEDS    75 //10mm breite Leds

//Updaterate der LEDS
#define FRAMES_PER_SECOND  120

//Effekte automatisch durchschallten (cyclePalettes) bei 1, Zeitdauer festlegen (paletteDuration)
bool cyclePattern = true;
uint16_t cyclePatternDuration = 20; //sec
//Farbpalleten durchschalten (nur bei effekten mit dem Namen: contains_Palette)
bool cyclePalette = true;
uint16_t paletteDuration = 7; //sec

//PIN des ESP´s
const uint8_t PIN_LED_STATUS = 2; //D2 --> onboard LED

//Trigger Pin fuer HC-SR04
#define TRIGPIN 22 //D22 GIPO22
#define ECHOPIN 21 //D21 GIPO21
//wenn eine Drone erkannt wird einene effect durchfuhren, dieser wird dann nach der aufgefuhrten Zeit wieder deaktiviert
#define DRONEDETECTTIMEOUT 3 //5 sec

#define DISTANCETHRESHOLD 60
#define GATESWITCHOFFTIME 300 //300 sec nach letzten Dronendruchflug

//-------------------------------------------------------

// Structure example to send data
// Must match the receiver structure
/*typedef struct struct_message {
  uint8_t gateNumber;   // Gatenummer zwischen 1-100
  char statusName[4];   // STATUSNAME mit 3 Buchstaben (+1 für Nullterminierung)
} struct_message;

struct_message recData;   //data received*/

//bei dem Wert 0 wird der ESP in den Deepsleep ueberfuert
uint8_t brightnesses[] = { 255, 128, 64, 0 };
uint8_t currentBrightnessIndex = 0;
uint8_t currentPatternIndex = random(0, patternsCount-1); // Zufälliger Index von 0 bis patternsCount-1

unsigned long cyclePatternTimeout = 0;
uint8_t currentPaletteIndex = 0;
unsigned long paletteTimeout = 0;

bool ledStatus = false;

unsigned long gateOnTime = 0;
uint8_t isUltrasonicSensorHealthy = 0;
float distanceCm = -1;

Button buttonBrightness(PIN_BUTTON_BRIGHTNESS);
Button buttonPattern(PIN_BUTTON_PATTERN);
Button buttonPalette(PIN_BUTTON_PALETTE);

enum State {
  STATE_NORMAL,
  STATE_DRONE_DETECTED,
  STATE_SLEEP,
  STATE_READY,
  STATE_EMPTY_BATTERY/*,
  STATE_GAME1*/
};

State currentState = STATE_NORMAL;

unsigned long stateStartTime = 0;
unsigned long stateDuration = 0; // Dauer des aktuellen Zustands in Millisekunden

unsigned long batteryLowStartTime = 0;  // Zeitpunkt, ab dem die Batteriespannung zu niedrig war
bool isBatteryLow = false;             // Flag, ob die Batteriespannung niedrig ist

/*uint8_t gameMode = 0;*/

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

/*
// callback function that will be executed when data is received
#ifdef ESP32
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
#else
void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
#endif
  memcpy(&recData, incomingData, sizeof(recData));
  // Debug-Ausgabe: Anzahl der empfangenen Bytes
  Serial.print("Bytes empfangen: ");
  Serial.println(len);
  // Empfange und zeige die Gatenummer an
  Serial.print("Gatenummer: ");
  Serial.println(recData.gateNumber);
  // Empfange und zeige den Statusnamen an
  Serial.print("STATUSNAME: ");
  Serial.println(recData.statusName);
  if (strcmp(recData.statusName, "GA1") == 0) {
    gameMode = 1;
  }
  if (strcmp(recData.statusName, "GA0") == 0) {
    gameMode = 0;
  }
}*/

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
    const float referenceVoltage = 3.3;  // Betriebsspannung des ESP32 in Volt (max. 3.3V)
    //const int adcResolution = 4096;  // 12-Bit-Auflösung (2^12 = 4096)
    const int adcResolution = 256;  // 8-Bit-Auflösung (2^8 = 256)

    int adcValue = analogRead(BAT_VOLTAGE_PIN);
    // Neuen Spannungswert lesen
    float newVoltageValue = (adcValue / (float)adcResolution) * referenceVoltage; // Analogen Spannungswert lesen
    float batteryVoltage = newVoltageValue * VOLTAGE_FACTOR; // Tatsächliche Batteriespannung
    // Debug-Ausgabe (optional)

      // Umwandlung der Werte in Strings und Ersetzen von Punkten durch Kommas
    /*String adcValueStr = String(adcValue);
    String newVoltageValueStr = String(newVoltageValue);
    String batteryVoltageStr = String(batteryVoltage);  

    adcValueStr.replace(".", ",");
    newVoltageValueStr.replace(".", ",");
    batteryVoltageStr.replace(".", ",");

    // Debug-Ausgabe mit Komma als Dezimaltrennzeichen
    Serial.print("adcValue: ;");
    Serial.print(adcValueStr);
    Serial.print("\t;newVoltageValue: ;");
    Serial.print(newVoltageValueStr);
    Serial.print("\t;batteryVoltage: ;");
    Serial.println(batteryVoltageStr);*/

    return batteryVoltage;
}

void checkDistanceThreshold() {

  //wenn der Sensor defekt ist nach 20 abfragen abbrechen
  if(isUltrasonicSensorHealthy >= 10) {
    distanceCm = -1; 
  }

  // Define sound velocity in cm/us and conversion factor for cm to inches
  long duration;

  // Clears the trigPin
  digitalWrite(TRIGPIN, LOW);
  delayMicroseconds(2);

  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(TRIGPIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGPIN, LOW);

  // Reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(ECHOPIN, HIGH, 20000);
  // Calculate the distance in cm
  distanceCm = (duration/2) / 29.1;

  //Serial.print("distanceCm: ");
  //Serial.println(distanceCm);

  if (distanceCm > 0){
    isUltrasonicSensorHealthy = 0;
  }
  // Check if the distance is greater than the threshold
  if (distanceCm > 0 && distanceCm <= DISTANCETHRESHOLD) {
    gateOnTime = millis();
  } else if (distanceCm == 0) {
    isUltrasonicSensorHealthy++;
    if(isUltrasonicSensorHealthy == 10) {
      Serial.println("HC-SR04 deaktiviert");
    }
  }
}

void handleInput() {
  if (buttonPattern.pressed()) {
    digitalWrite(PIN_LED_STATUS, HIGH);
  }
  else if (buttonPattern.released())
  {
    //autoplay cyclePattern deaktivieren
    cyclePattern = false;
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
    cyclePalette = false;
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

void handleEmptyBattery() {
  u_int8_t specialpatternsnumber = 1; //batteryEmpty();
  // Call the current pattern function once, updating the 'leds' array
  specialpatterns[specialpatternsnumber]();
  currentPalette = palettes[currentPaletteIndex];
  FastLED.show();
  FastLED.delay(1000 / FRAMES_PER_SECOND);  
}

void handleLowBattery() {
  u_int8_t specialpatternsnumber = 4; //batteryLow();
  // Call the current pattern function once, updating the 'leds' array
  specialpatterns[specialpatternsnumber]();
  currentPalette = palettes[currentPaletteIndex];
  FastLED.show();
  FastLED.delay(1000 / FRAMES_PER_SECOND);  
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

void handleGateReady() {
  u_int8_t specialpatternsnumber = 2; //gateReady();
  // Call the current pattern function once, updating the 'leds' array
  specialpatterns[specialpatternsnumber]();
  currentPalette = palettes[currentPaletteIndex];
  FastLED.show();
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}

/*void handleGame1() {
  u_int8_t specialpatternsnumber = 3; //gateReady();
  // Call the current pattern function once, updating the 'leds' array
  specialpatterns[specialpatternsnumber]();
  currentPalette = palettes[currentPaletteIndex];
  FastLED.show();
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}*/

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

  if (cyclePattern && (millis() > cyclePatternTimeout)) {
    nextPattern();
    cyclePatternTimeout = millis() + (cyclePatternDuration * 1000);
  }

  if (cyclePalette && (millis() > paletteTimeout)) {
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
      case STATE_EMPTY_BATTERY:
        Serial.println("STATE_EMPTY_BATTERY");
        break;
      case STATE_SLEEP:
        Serial.println("STATE_SLEEP");
        break;
      case STATE_READY:
        Serial.println("STATE_READY");
        break;
      case STATE_DRONE_DETECTED:
        Serial.println("STATE_DRONE_DETECTED");
        break;
      /*case STATE_GAME1:
        Serial.println("STATE_GAME1");
        break;*/
      case STATE_NORMAL:
      default:
        Serial.println("STATE_NORMAL");
        break;
      // Weitere Fälle nach Bedarf hinzufügen
    }
  }
  return newState;
}

  static int lowBatteryCount = 0; // Zähler für aufeinanderfolgende niedrige Spannungen

void updateState() {


  // Zustandswechsel nur prüfen, wenn keine Zeitbegrenzung aktiv ist
  if (stateDuration == 0 || millis() - stateStartTime >= stateDuration) {

    if (getBatteryVoltage() <= SWITCH_OFF_VOLTAGE) {
      lowBatteryCount++; // Zähler erhöhen, wenn die Spannung zu niedrig ist
      if (!isBatteryLow && lowBatteryCount >= LOW_BATTERY_COUNT) {
        // Batteriespannung ist dreimal hintereinander zu niedrig
        isBatteryLow = true;
        batteryLowStartTime = millis();
      }
    } else {
      lowBatteryCount = 0; // Zähler zurücksetzen, wenn die Spannung wieder über dem Grenzwert ist
      /*if (isBatteryLow && millis() - batteryLowStartTime >= BATTERYCHECKINVERTVAL) {
        // Batterie ist nicht mehr niedrig, nach Haltezeit zurücksetzen
        isBatteryLow = false;
      }*/
    }


    /*Serial.print(lowBatteryCount); // Ausgabe des Zählers
    Serial.print("\t"); // Ausgabe des Zählers*/
    // Prüfen, ob der Zustand auf STATE_EMPTY_BATTERY geändert werden muss
    if (isBatteryLow) {
      changeState(STATE_EMPTY_BATTERY);
    } else if (brightnesses[currentBrightnessIndex] == 0) {  // Wenn die Helligkeit 0 ist, in den Schlafmodus wechseln
      changeState(STATE_SLEEP);
    } else if (distanceCm <= DISTANCETHRESHOLD && distanceCm > 1) { // Überprüfen, ob eine Drohne erkannt wurde und sicherstellen, dass der sensor nicht defekt ist
      changeState(STATE_DRONE_DETECTED, DRONEDETECTTIMEOUT);
    } else if (millis() - gateOnTime >= GATESWITCHOFFTIME*1000 && isUltrasonicSensorHealthy < 10) { //wenn eine drone gefunden wurde dann auch noch prüfen das das Gate dann irgendwann ausgeht und sicherstellen, dass der sensor nicht defekt ist
      changeState(STATE_READY);
    /*} else if (gameMode == 1) {
      changeState(STATE_GAME1);*/
    } else {
      changeState(STATE_NORMAL);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("setup");

  delay(3000); // 3 second delay for recovery

  pinMode(BAT_VOLTAGE_PIN, INPUT_PULLUP);
  //analogReadResolution(12);  // Werte können 9, 10, 11 oder 12 sein
  analogReadResolution(8);  // Werte: 8-Bit-Auflösung (256 Stufen)
  analogSetAttenuation(ADC_11db); // Bereich 0 bis ~3.3V (11dB für max. Spannungsbereich)
  //analogReference(INTERNAL);

  /*WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  #ifdef ESP32
  if (esp_now_init() != ESP_OK) {
  #else
  if (esp_now_init() != 0) {
  #endif
   Serial.println("Error initializing ESP-NOW");
    return;
  }

  #ifdef ESP32
  esp_now_register_recv_cb(OnDataRecv);
  #else
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  // Register a callback function to handle received ESP-NOW data
  //esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  esp_now_register_recv_cb(OnDataRecv);
  #endif*/


  pinMode(TRIGPIN, OUTPUT); // Sets the trigPin as an Output
  pinMode(ECHOPIN, INPUT); // Sets the echoPin as an Input

  setNumLeds(NUM_LEDS);
  // limit my draw to 3A at 5v of power draw
  FastLED.setMaxPowerInVoltsAndMilliamps(5,5000);
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
  checkDistanceThreshold();
  updateState();

  switch (currentState) {
    case STATE_EMPTY_BATTERY:
      handleEmptyBattery();
      break;
    case STATE_SLEEP:
      handleSleep();
      break;
    case STATE_READY:
      handleGateReady();
      break;
    case STATE_DRONE_DETECTED:
      handleDroneDetected();
      break;
    /*case STATE_GAME1:
      handleGame1();
      break;*/
    case STATE_NORMAL:
    default:
      handleNormalOperation();
      break;
  }

  // Verarbeite Benutzereingaben, falls der Zustand dies zulässt
  handleInput();
}



