#include <FastLED.h>  // https://github.com/FastLED/FastLED
#include <Button.h>   // https://github.com/madleech/Button
#include <EEPROM.h>  // EEPROM für Speichern von Werte

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>


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

// Zeitdauer (in Millisekunden), nach der der Server abgeschaltet wird
#define SERVER_LIFETIME 300000  // 5 Minute
unsigned long sendIntervalws = 500;  // Nachricht alle 500ms an Webseite senden
// Definiere die SSID und das Passwort für den Hotspot
const char *ssid = "Drone_Gate_4";  // Name des Hotspots
const char *password = "123456789";  // Passwort für den Hotspot

//Pins fuer Buttons
#define PIN_BUTTON_PATTERN 18 // D18 GPIO18
#define PIN_BUTTON_PALETTE 99
#define PIN_BUTTON_BRIGHTNESS 19 //D19 GIOP19

//Batterie limit --> es wird ein spezieller Effekt gewahlt der anzeigt das die Batterie leer ist (5 rote leds in der Mitte des Rings)
#define BAT_VOLTAGE_PIN 35 //D35 for ANALOG GIPO35
#define LOW_BATTERY_COUNT 150 //wenn 100x der Wert unter dem Switchoff wert hintereinander ist schaltet das Gate aus

//LED Config
//#define CLK_PIN     2
//#define LED_TYPE    APA102
#define LED_PIN 4 //D4 GIPO4
#define LED_TYPE    WS2812
#define COLOR_ORDER GRB

//Effekte automatisch durchschallten (cyclePalettes) bei 1, Zeitdauer festlegen (paletteDuration)
bool cyclePattern = true;
//Farbpalleten durchschalten (nur bei effekten mit dem Namen: contains_Palette)
bool cyclePalette = true;

//PIN des ESP´s
const uint8_t PIN_LED_STATUS = 2; //D2 --> onboard LED

//Trigger Pin fuer HC-SR04
#define TRIGPIN 22 //D22 GIPO22
#define ECHOPIN 21 //D21 GIPO21

//-------------------------------------------------------
//Config ueber Webserver nur default werte

// Batterie-Konfiguration
float voltageFactor = 3.86;  // Resistor Ratio Factor --> Gate 1
float switchOffVoltage = 11.30; // 11V für 3S
int configNumLeds = 75; // Anzahl LEDs

// LED Update-Rate
int framesPerSecond = 120;

// Effekteinstellungen
uint16_t cyclePatternDuration = 60; // Sekunden
uint16_t paletteDuration = 19; // Sekunden

// Trigger HC-SR04
//wenn eine Drone erkannt wird einene effect durchfuhren, dieser wird dann nach der aufgefuhrten Zeit wieder deaktiviert
uint8_t droneDetectTimeout = 3; // 3 Sekunden

int distanceThreshold = 60;
int gateSwitchOffTime = 180; //3 min (180 sec) nach letzten Dronendruchflug

// EEPROM Konfiguration
#define EEPROM_SIZE 512  
// EEPROM Adressen
#define EEPROM_VOLTAGE_FACTOR_ADDR 0
#define EEPROM_SWITCH_OFF_VOLTAGE_ADDR (EEPROM_VOLTAGE_FACTOR_ADDR + sizeof(float))
#define EEPROM_NUM_LEDS_ADDR (EEPROM_SWITCH_OFF_VOLTAGE_ADDR + sizeof(int))
#define EEPROM_FRAMES_PER_SECOND_ADDR (EEPROM_NUM_LEDS_ADDR + sizeof(int))
#define EEPROM_CYCLE_PATTERN_DURATION_ADDR (EEPROM_FRAMES_PER_SECOND_ADDR + sizeof(int))
#define EEPROM_PALETTE_DURATION_ADDR (EEPROM_CYCLE_PATTERN_DURATION_ADDR + sizeof(uint16_t))
#define EEPROM_DRONEDETECTTIMEOUT_ADDR (EEPROM_PALETTE_DURATION_ADDR + sizeof(uint16_t))
#define EEPROM_DISTANCETHRESHOLD_ADDR (EEPROM_DRONEDETECTTIMEOUT_ADDR + sizeof(uint8_t))
#define EEPROM_GATESWITCHOFFTIME_ADDR (EEPROM_DISTANCETHRESHOLD_ADDR + sizeof(int))

// Structure example to send data
// Must match the receiver structure
/*typedef struct struct_message {
  uint8_t gateNumber;   // Gatenummer zwischen 1-100
  char statusName[4];   // STATUSNAME mit 3 Buchstaben (+1 für Nullterminierung)
} struct_message;

struct_message recData;   //data received*/

AsyncWebServer server(80);
// WebSocket-Handler hinzufügen
AsyncWebSocket ws("/ws");  // WebSocket-Server unter /ws
unsigned long serverStartTime;
bool serverActive = true;
unsigned long lastSentws = 0;

// JSON-Objekt erstellen
DynamicJsonDocument doc(1024);

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
static int lowBatteryCount = 0; // Zähler für aufeinanderfolgende niedrige Spannungen

// Sende den neuen Status an WebSocket-Clients
String hotspot_current_state_info;
String hotspot_runtime_info;
String hotspot_voltage_info;
String hotspot_distanceCm_info;
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
// Debug function to print the EEPROM values
void debugEEPROMConfig() {
  Serial.println("EEPROM Configuration Loaded:");
  Serial.print("Voltage Factor: ");
  Serial.println(voltageFactor);
  
  Serial.print("Switch Off Voltage: ");
  Serial.println(switchOffVoltage);
  
  Serial.print("Number of LEDs: ");
  Serial.println(configNumLeds);
  
  Serial.print("Frames per Second: ");
  Serial.println(framesPerSecond);
  
  Serial.print("Cycle Pattern Duration: ");
  Serial.println(cyclePatternDuration);
  
  Serial.print("Palette Duration: ");
  Serial.println(paletteDuration);
  
  Serial.print("Drone Detect Timeout: ");
  Serial.println(droneDetectTimeout);
  
  Serial.print("Distance Threshold: ");
  Serial.println(distanceThreshold);
  
  Serial.print("Gate Switch Off Time: ");
  Serial.println(gateSwitchOffTime);
}

void validateEEPROMValues() {
  // Batterie-Konfiguration
  if (isnan(voltageFactor) || voltageFactor < 0.0f || voltageFactor > 10.0f) {
    Serial.println("Ungültiger Wert für Voltage Factor! Setze auf Standardwert.");
    voltageFactor = 3.86f;  // Standardwert für Voltage Factor
  }

  if (switchOffVoltage < 0 || switchOffVoltage > 45) {
    Serial.println("Ungültiger Wert für Switch Off Voltage! Setze auf Standardwert.");
    switchOffVoltage = 11.30f;  // Standardwert für Switch Off Voltage
  }

  if (configNumLeds < 1 || configNumLeds > 1000) {
    Serial.println("Ungültiger Wert für Number of LEDs! Setze auf Standardwert.");
    configNumLeds = 75;  // Standardwert für Number of LEDs
  }

  // LED Update-Rate
  if (framesPerSecond < 1 || framesPerSecond > 240) {
    Serial.println("Ungültiger Wert für Frames per Second! Setze auf Standardwert.");
    framesPerSecond = 120;  // Standardwert für Frames per Second
  }

  // Effekteinstellungen
  if (cyclePatternDuration < 1 || cyclePatternDuration > 3600) {
    Serial.println("Ungültiger Wert für Cycle Pattern Duration! Setze auf Standardwert.");
    cyclePatternDuration = 60;  // Standardwert für Cycle Pattern Duration
  }

  if (paletteDuration < 1 || paletteDuration > 3600) {
    Serial.println("Ungültiger Wert für Palette Duration! Setze auf Standardwert.");
    paletteDuration = 19;  // Standardwert für Palette Duration
  }

  // Trigger HC-SR04
  if (droneDetectTimeout < 1 || droneDetectTimeout > 60) {
    Serial.println("Ungültiger Wert für Drone Detect Timeout! Setze auf Standardwert.");
    droneDetectTimeout = 3;  // Standardwert für Drone Detect Timeout
  }

  if (distanceThreshold < 1 || distanceThreshold > 1000) {
    Serial.println("Ungültiger Wert für Distance Threshold! Setze auf Standardwert.");
    distanceThreshold = 60;  // Standardwert für Distance Threshold
  }

  if (gateSwitchOffTime < 1 || gateSwitchOffTime > 3600) {
    Serial.println("Ungültiger Wert für Gate Switch Off Time! Setze auf Standardwert.");
    gateSwitchOffTime = 180;  // Standardwert für Gate Switch Off Time
  }
}

void loadConfigFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(EEPROM_VOLTAGE_FACTOR_ADDR, voltageFactor);
  EEPROM.get(EEPROM_SWITCH_OFF_VOLTAGE_ADDR, switchOffVoltage);
  EEPROM.get(EEPROM_NUM_LEDS_ADDR, configNumLeds);
  EEPROM.get(EEPROM_FRAMES_PER_SECOND_ADDR, framesPerSecond);
  EEPROM.get(EEPROM_CYCLE_PATTERN_DURATION_ADDR, cyclePatternDuration);
  EEPROM.get(EEPROM_PALETTE_DURATION_ADDR, paletteDuration);
  EEPROM.get(EEPROM_DRONEDETECTTIMEOUT_ADDR, droneDetectTimeout);
  EEPROM.get(EEPROM_DISTANCETHRESHOLD_ADDR, distanceThreshold);
  EEPROM.get(EEPROM_GATESWITCHOFFTIME_ADDR, gateSwitchOffTime);

  // Werte nach dem Laden validieren
  validateEEPROMValues();

  // Call debug function to output values to Serial Monitor
  debugEEPROMConfig();
}

void saveConfigToEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(EEPROM_VOLTAGE_FACTOR_ADDR, voltageFactor);
  EEPROM.put(EEPROM_SWITCH_OFF_VOLTAGE_ADDR, switchOffVoltage);
  EEPROM.put(EEPROM_NUM_LEDS_ADDR, configNumLeds);
  EEPROM.put(EEPROM_FRAMES_PER_SECOND_ADDR, framesPerSecond);
  EEPROM.put(EEPROM_CYCLE_PATTERN_DURATION_ADDR, cyclePatternDuration);
  EEPROM.put(EEPROM_PALETTE_DURATION_ADDR, paletteDuration);
  EEPROM.put(EEPROM_DRONEDETECTTIMEOUT_ADDR, droneDetectTimeout);
  EEPROM.put(EEPROM_DISTANCETHRESHOLD_ADDR, distanceThreshold);
  EEPROM.put(EEPROM_GATESWITCHOFFTIME_ADDR, gateSwitchOffTime);

  // Werte nach dem Laden validieren
  validateEEPROMValues();

  // Call debug function to output values to Serial Monitor
  debugEEPROMConfig();

  EEPROM.commit();

  //einmal kurz alle LEds ausmachen, damit wenn die Led anzahl umgestellt wird ersichtlich ist was umgestellt wurde
  setNumLeds(configNumLeds);
  Serial.flush();
  fill_solid(leds, configNumLeds, CRGB::Black);
  FastLED.show();
}

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
      Serial.println("WebSocket-Client verbunden");
  } else if (type == WS_EVT_DISCONNECT) {
      Serial.println("WebSocket-Client getrennt");
  }
}

// WebSocket-Initialisierung im Setup
void setupWebSocket() {
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);
}

void setupServer() {

  WiFi.disconnect(true);
  delay(1000);
 
  // Starte den ESP32 im Access Point-Modus
  WiFi.softAP(ssid, password);
  delay(1000);
  IPAddress IP = WiFi.softAPIP();
  // Gib die IP-Adresse des Access Points aus
  Serial.println("Hotspot gestartet!");
  Serial.print("IP-Adresse des Hotspots: ");
  Serial.println(IP);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<html><head>";
    html += "<script>";
      // Zeit umrechenen f
    html += "function formatTime(millis) {";
    html += "  let seconds = Math.floor(millis / 1000);";  // Millisekunden in Sekunden umwandeln
    html += "  let h = Math.floor(seconds / 3600);";
    html += "  let m = Math.floor((seconds % 3600) / 60);";
    html += "  let s = seconds % 60;";
    html += "  return h.toString().padStart(2, '0') + ':' + m.toString().padStart(2, '0') + ':' + s.toString().padStart(2, '0');";
    html += "}";
    html += "var socket = new WebSocket('ws://' + window.location.host + '/ws');";
    html += "socket.onmessage = function(event) {";
    // Hier wird erwartet, dass die Nachricht JSON ist                                                      
    html += "  var data = JSON.parse(event.data);";
    html += "  document.getElementById('hotspot_current_state_info').value = data.current_state;";
    html += "  document.getElementById('hotspot_runtime_info').value = formatTime(data.runtime_info);"; 
    html += "  document.getElementById('hotspot_voltage_info').value = data.voltage;";
    html += "  document.getElementById('hotspot_distanceCm_info').value = data.distanceCm;";    
    html += " };";
    html += "function validateForm() {";
    html += "    var valid = true;";
    html += "    var fields = ['voltageFactor', 'switchOffVoltage', 'configNumLeds', 'framesPerSecond', 'cyclePatternDuration', 'paletteDuration', 'droneDetectTimeout', 'distanceThreshold', 'gateSwitchOffTime'];";
    html += "    fields.forEach(function(field) {";
    html += "        var value = document.getElementById(field).value;";
    html += "        if (!/^[0-9]+(\\.[0-9]+)?$/.test(value)) {";  
    html += "            document.getElementById(field).style.backgroundColor = 'red';";
    html += "            valid = false;";
    html += "        } else {";
    html += "            document.getElementById(field).style.backgroundColor = '';";
    html += "        }";
    html += "    });";
    html += "    return valid;";
    html += "}";
    html += "</script></head><body>";
    html += "<h2>ESP Gate Einstellungen</h2>";
    html += "<form action='/save' method='POST' onsubmit='return validateForm()'>";
    html += "<table>";
    
    // Eingabefelder als Tabellenzeilen
    html += "<tr><td>Voltage Factor [-]:</td><td><input type='number' step='0.01' name='voltageFactor' value='" + String(voltageFactor) + "' id='voltageFactor'></td></tr>";
    html += "<tr><td>Switch Off Voltage [V]:</td><td><input type='number' step='0.01' name='switchOffVoltage' value='" + String(switchOffVoltage) + "' id='switchOffVoltage'></td></tr>";
    html += "<tr><td>LEDs [-]:</td><td><input type='number' name='configNumLeds' value='" + String(configNumLeds) + "' id='configNumLeds'></td></tr>";
    html += "<tr><td>FPS [1/s]:</td><td><input type='number' name='framesPerSecond' value='" + String(framesPerSecond) + "' id='framesPerSecond'></td></tr>";
    html += "<tr><td>Cycle Pattern Duration [s]:</td><td><input type='number' name='cyclePatternDuration' value='" + String(cyclePatternDuration) + "' id='cyclePatternDuration'></td></tr>";
    html += "<tr><td>Palette Duration [s]:</td><td><input type='number' name='paletteDuration' value='" + String(paletteDuration) + "' id='paletteDuration'></td></tr>";
    html += "<tr><td>Drone Detect Timeout [s]:</td><td><input type='number' name='droneDetectTimeout' value='" + String(droneDetectTimeout) + "' id='droneDetectTimeout'></td></tr>";
    html += "<tr><td>Distance Threshold [cm]:</td><td><input type='number' name='distanceThreshold' value='" + String(distanceThreshold) + "' id='distanceThreshold'></td></tr>";
    html += "<tr><td>Gate Switch Off Time [s]:</td><td><input type='number' name='gateSwitchOffTime' value='" + String(gateSwitchOffTime) + "' id='gateSwitchOffTime'></td></tr>";
    html += "<tr><td colspan='2'><input type='submit' value='Speichern'></td></tr>";
    html += "</table>";

    html += "<h2>ESP Gate Status</h2>";
    html += "<table>";
    html += "<tr><td>Current State [-]:</td><td><input type='text' name='hotspot_current_state_info' value='" + String(hotspot_current_state_info) + "' id='hotspot_current_state_info' readonly></td></tr>";
    html += "<tr><td>Current Voltage [V]:</td><td><input type='text' name='hotspot_voltage_info' value='" + String(hotspot_voltage_info) + "' id='hotspot_voltage_info' readonly></td></tr>";
    html += "<tr><td>Current distance [cm]:</td><td><input type='text' name='hotspot_distanceCm_info' value='" + String(hotspot_distanceCm_info) + "' id='hotspot_distanceCm_info' readonly></td></tr>";
    html += "<tr><td>Hotspot remaining time [s]:</td><td><input type='text' name='hotspot_runtime_info' value='" + String(hotspot_runtime_info) + "' id='hotspot_runtime_info' readonly></td></tr>";
    html += "</table>";
    html += "</form></body></html>";

    request->send(200, "text/html", html);
  });

  // Android Captive Portal Request
  server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "You were sent here by a captive portal after requesting generate_204");
    Serial.println("requested /generate_204");
  });

  // Apple Captive Portal Request
  server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "You were sent here by a captive portal after requesting hotspot-detect.html");
    Serial.println("requested /hotspot-detect.html");
  });

  // Alternative für einige Apple-Geräte (iOS/macOS)
  server.on("/library/test/success.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "Apple captive portal check.");
    Serial.println("requested /library/test/success.html");
  });

  //This is an example of a redirect type response.  onNotFound acts as a catch-all for any request not defined above
  server.onNotFound([](AsyncWebServerRequest *request){
    request->redirect("/");
    Serial.print("server.notfound triggered: ");
    Serial.println(request->url());       //This gives some insight into whatever was being requested
  });

  //server.on("/hotspot-detect.html", HTTP_GET, [&](AsyncWebServerRequest *request) {
  //  request->send(200, "text/html", "<meta http-equiv='refresh' content='0; url=" + localIP + "'>");
  //});

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request){
      if (request->hasParam("voltageFactor", true)) voltageFactor = request->getParam("voltageFactor", true)->value().toFloat();
      if (request->hasParam("switchOffVoltage", true)) switchOffVoltage = request->getParam("switchOffVoltage", true)->value().toFloat();
      if (request->hasParam("configNumLeds", true)) configNumLeds = request->getParam("configNumLeds", true)->value().toInt();
      if (request->hasParam("framesPerSecond", true)) framesPerSecond = request->getParam("framesPerSecond", true)->value().toInt();
      if (request->hasParam("cyclePatternDuration", true)) cyclePatternDuration = request->getParam("cyclePatternDuration", true)->value().toInt();
      if (request->hasParam("paletteDuration", true)) paletteDuration = request->getParam("paletteDuration", true)->value().toInt();
      if (request->hasParam("droneDetectTimeout", true)) droneDetectTimeout = request->getParam("droneDetectTimeout", true)->value().toInt();
      if (request->hasParam("distanceThreshold", true)) distanceThreshold = request->getParam("distanceThreshold", true)->value().toInt();
      if (request->hasParam("gateSwitchOffTime", true)) gateSwitchOffTime = request->getParam("gateSwitchOffTime", true)->value().toInt();
      
      saveConfigToEEPROM();
      lowBatteryCount = 0; // Zähler zurücksetzen, damit beim unstellen der switchOffVoltage die batterie wieder neu geladen wird
      isBatteryLow = false;
      //request->send(200, "text/html", "<h3>Gespeichert! <a href='/'>Zur&uuml;ck</a></h3>");
      request->redirect("/");

  });

  server.begin();
}

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
    float batteryVoltage = newVoltageValue * voltageFactor; // Tatsächliche Batteriespannung
    hotspot_voltage_info = batteryVoltage;
    

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
  duration = pulseIn(ECHOPIN, HIGH);
  // Calculate the distance in cm
  distanceCm = (duration/2) / 29.1;

  //Serial.print("distanceCm: ");
  //Serial.println(distanceCm);

  if (distanceCm > 0){
    isUltrasonicSensorHealthy = 0;
  }
  // Check if the distance is greater than the threshold
  if (distanceCm > 0 && distanceCm <= distanceThreshold) {
    gateOnTime = millis();
  } else if (distanceCm == 0) {
    isUltrasonicSensorHealthy++;
    if(isUltrasonicSensorHealthy == 10) {
      Serial.println("HC-SR04 deaktiviert");
      isUltrasonicSensorHealthy = 20;
    }
  }
  //info fuer hotspot
  hotspot_distanceCm_info = distanceCm;
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
  FastLED.delay(1000 / framesPerSecond);  
}

void handleLowBattery() {
  u_int8_t specialpatternsnumber = 4; //batteryLow();
  // Call the current pattern function once, updating the 'leds' array
  specialpatterns[specialpatternsnumber]();
  currentPalette = palettes[currentPaletteIndex];
  FastLED.show();
  FastLED.delay(1000 / framesPerSecond);  
}

void handleSleep() {
  Serial.println("Going down...");
  Serial.flush();
  fill_solid(leds, configNumLeds, CRGB::Black);
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
  FastLED.delay(1000 / framesPerSecond);
}

void handleGateReady() {
  u_int8_t specialpatternsnumber = 2; //gateReady();
  // Call the current pattern function once, updating the 'leds' array
  specialpatterns[specialpatternsnumber]();
  currentPalette = palettes[currentPaletteIndex];
  FastLED.show();
  FastLED.delay(1000 / framesPerSecond);
}

/*void handleGame1() {
  u_int8_t specialpatternsnumber = 3; //gateReady();
  // Call the current pattern function once, updating the 'leds' array
  specialpatterns[specialpatternsnumber]();
  currentPalette = palettes[currentPaletteIndex];
  FastLED.show();
  FastLED.delay(1000 / framesPerSecond);
}*/

void handleNormalOperation() {
  // Call the current pattern function once, updating the 'leds' array
    // Call the current pattern function once, updating the 'leds' array
  patterns[currentPatternIndex]();
  currentPalette = palettes[currentPaletteIndex];
  FastLED.show();
  FastLED.delay(1000 / framesPerSecond);

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
        hotspot_current_state_info = "STATE_EMPTY_BATTERY";
        break;
      case STATE_SLEEP:
        Serial.println("STATE_SLEEP");
        hotspot_current_state_info = "STATE_SLEEP";
        break;
      case STATE_READY:
        Serial.println("STATE_READY");
        hotspot_current_state_info = "STATE_READY";
        break;
      case STATE_DRONE_DETECTED:
        Serial.println("STATE_DRONE_DETECTED");
        hotspot_current_state_info = "STATE_DRONE_DETECTED";
        break;
      /*case STATE_GAME1:
        Serial.println("STATE_GAME1");
        hotspot_current_state_info = "STATE_GAME1";
        break;*/
      case STATE_NORMAL:
      default:
        Serial.println("STATE_NORMAL");
        hotspot_current_state_info = "STATE_NORMAL";
        break;
      // Weitere Fälle nach Bedarf hinzufügen
    }
  }
  return newState;
}



void updateState() {

  // Zustandswechsel nur prüfen, wenn keine Zeitbegrenzung aktiv ist
  if (stateDuration == 0 || millis() - stateStartTime >= stateDuration) {

    if (getBatteryVoltage() <= switchOffVoltage) {
      lowBatteryCount++; // Zähler erhöhen, wenn die Spannung zu niedrig ist
      if (!isBatteryLow && lowBatteryCount >= LOW_BATTERY_COUNT) {
        // Batteriespannung ist LOW_BATTERY_COUNT hintereinander zu niedrig
        isBatteryLow = true;
        batteryLowStartTime = millis();
      }
    } else {
      lowBatteryCount = 0; // Zähler zurücksetzen, wenn die Spannung wieder über dem Grenzwert ist
    }


    /*Serial.print(lowBatteryCount); // Ausgabe des Zählers
    Serial.print("\t"); // Ausgabe des Zählers*/
    // Prüfen, ob der Zustand auf STATE_EMPTY_BATTERY geändert werden muss
    if (isBatteryLow) {
      changeState(STATE_EMPTY_BATTERY);
    } else if (brightnesses[currentBrightnessIndex] == 0) {  // Wenn die Helligkeit 0 ist, in den Schlafmodus wechseln
      changeState(STATE_SLEEP);
    } else if (distanceCm <= distanceThreshold && distanceCm > 1 && isUltrasonicSensorHealthy < 10) { // Überprüfen, ob eine Drohne erkannt wurde und sicherstellen, dass der sensor nicht defekt ist
      changeState(STATE_DRONE_DETECTED, droneDetectTimeout);
    } else if (millis() - gateOnTime >= gateSwitchOffTime*1000 && isUltrasonicSensorHealthy < 10) { //wenn eine drone gefunden wurde dann auch noch prüfen das das Gate dann irgendwann ausgeht und sicherstellen, dass der sensor nicht defekt ist
      changeState(STATE_READY);
    /*} else if (gameMode == 1) {
      changeState(STATE_GAME1);*/
    } else {
      changeState(STATE_NORMAL);
    }
  }

  if ((millis() - lastSentws >= sendIntervalws) && serverActive) {
    //werte auf Hotspot anpassen
    doc["current_state"] = hotspot_current_state_info;
    doc["runtime_info"] = hotspot_runtime_info;    
    doc["voltage"] = hotspot_voltage_info;
    doc["distanceCm"] = hotspot_distanceCm_info;
    // WebSocket-Nachricht als JSON senden
    String jsonString;
    serializeJson(doc, jsonString);
    // Nachricht über WebSocket senden
    ws.textAll(jsonString);
    lastSentws = millis();
  }

}

void setup() {
  Serial.begin(115200);
  Serial.println("");
  Serial.println("setup");

  loadConfigFromEEPROM(); // Lade gespeicherte Konfiguration
  delay(3000); // 3 second delay for recovery
  
  setupWebSocket();
  setupServer();
  serverStartTime = millis();

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

  setNumLeds(configNumLeds);
  // limit my draw to 3A at 5v of power draw
  FastLED.setMaxPowerInVoltsAndMilliamps(5,5000);
  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, configNumLeds).setCorrection(TypicalLEDStrip);
  
  // set master brightness control
  FastLED.setBrightness(brightnesses[currentBrightnessIndex]);

  FastLED.show(CRGB::Black);

  buttonPattern.begin();
  buttonPalette.begin();
  buttonBrightness.begin();

  pinMode(PIN_LED_STATUS, OUTPUT);
  digitalWrite(PIN_LED_STATUS, LOW);

  //initialstate festlegen
  changeState(STATE_NORMAL);
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

  if (serverActive) {
    hotspot_runtime_info = SERVER_LIFETIME - (millis() - serverStartTime);
    ws.cleanupClients();  // WebSocket-Verbindungen verwalten
  }

  if ((millis() - serverStartTime > SERVER_LIFETIME) && serverActive) {
    server.end();
    WiFi.softAPdisconnect(true);  // Deaktiviert den SoftAP und trennt alle verbundenen Clients
    WiFi.mode(WIFI_OFF);          // Schaltet Wi-Fi vollständig aus
    serverActive = false;
    Serial.println("Webserver abgeschalten");
  }
}



