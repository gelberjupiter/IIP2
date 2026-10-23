/*
 * Referenz-Terminal (Tuerleser-Nachbau) – ESP32 #2
 * ============================================================
 * HSLU IIP-Projekt, edukativ/defensiv – nur eigene Karten.
 *
 * Funktion:
 *   - RC522 liest eine MIFARE-Karte (13,56 MHz) und holt die UID
 *   - UID wird mit einer Liste erlaubter Karten verglichen
 *   - ERLAUBT  -> LED gruen + Relais zieht an ("Tuer auf") fuer ein paar Sekunden
 *   - VERWEIGERT-> LED rot
 *   - Ruhezustand -> LED dezent blau, wartet auf Karte
 *   - Jede gelesene UID wird im seriellen Monitor ausgegeben, damit man
 *     seine eigene Karte leicht in die Liste unten eintragen kann.
 *
 * ------------------------------------------------------------
 * VERKABELUNG (so ist es angeschlossen):
 *   RC522 (SPI, NUR 3,3 V!):
 *     3,3V -> 3V3 | GND -> GND
 *     SDA/SS -> GPIO5 | SCK -> GPIO18 | MOSI -> GPIO23 | MISO -> GPIO19 | RST -> GPIO4
 *   Relais-Modul:
 *     DC+ -> 5V | DC- -> GND | IN1 -> GPIO26
 *     Kontaktseite COM/NO/NC = potentialfrei (Tueroeffner-Dummy, KEINE Netzspannung)
 *   RGB-LED WS2812 (1 Pixel):
 *     Di -> GPIO32 | 5V -> 5V | GND -> GND
 *
 * Strom: USB-C-Netzteil direkt am ESP32.
 * ============================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_NeoPixel.h>

// ---------- Pins ----------
#define RC522_SS    5
#define RC522_RST   4
#define RC522_SCK   18
#define RC522_MISO  19
#define RC522_MOSI  23
#define RELAY_PIN   26
#define LED_DI      32
#define NUM_PIXELS  1

// Die meisten Relais-Module ziehen bei LOW an (active-low). Falls deins
// umgekehrt reagiert, einfach auf false stellen.
#define RELAY_ACTIVE_LOW  true
#define DOOR_OPEN_MS      3000     // wie lange die "Tuer" offen bleibt

// ============================================================
//  ERLAUBTE KARTEN
//  UID als Grossbuchstaben-Hex OHNE Leerzeichen eintragen.
//  Deine Karte dranhalten, im seriellen Monitor die UID ablesen
//  und hier eintragen (mehrere moeglich).
// ============================================================
const char* allowedUIDs[] = {
  "DEADBEEF",      // <-- Beispiel: durch deine echte UID ersetzen
};
const int allowedCount = sizeof(allowedUIDs) / sizeof(allowedUIDs[0]);

// ---------- Objekte ----------
MFRC522 rfid(RC522_SS, RC522_RST);
Adafruit_NeoPixel pixel(NUM_PIXELS, LED_DI, NEO_GRB + NEO_KHZ800);

// ---------- Hilfsfunktionen ----------
void setLed(uint8_t r, uint8_t g, uint8_t b) {
  pixel.setPixelColor(0, pixel.Color(r, g, b));
  pixel.show();
}

void lockDoor() {   // Relais AUS (Tuer zu)
  digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? HIGH : LOW);
}
void unlockDoor() { // Relais AN (Tuer auf)
  digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? LOW : HIGH);
}

// UID in Grossbuchstaben-Hex ohne Leerzeichen (max. 10 Byte -> 20 Zeichen)
void uidToHex(const uint8_t* uid, uint8_t len, char* out) {
  int p = 0;
  for (uint8_t i = 0; i < len; i++) p += sprintf(&out[p], "%02X", uid[i]);
  out[p] = '\0';
}

bool isAllowed(const char* uidHex) {
  for (int i = 0; i < allowedCount; i++) {
    if (strcmp(uidHex, allowedUIDs[i]) == 0) return true;
  }
  return false;
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println(F("\n=== Terminal startet ==="));

  // Relais zuerst sicher auf "zu" setzen, dann als Ausgang
  // (umgekehrt wuerde pinMode den Pin kurz auf LOW treiben -> Relais klickt)
  lockDoor();
  pinMode(RELAY_PIN, OUTPUT);

  // RGB-LED
  pixel.begin();
  pixel.setBrightness(60);
  setLed(0, 0, 15);          // dezentes Blau = bereit

  // RC522 / SPI
  SPI.begin(RC522_SCK, RC522_MISO, RC522_MOSI, RC522_SS);
  rfid.PCD_Init();
  delay(50);
  byte v = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  if (v == 0x00 || v == 0xFF) {
    Serial.println(F("FEHLER: RC522 nicht gefunden."));
    Serial.println(F("-> NUR 3,3 V? SPI-Pins 5/18/23/19 + RST 4 korrekt?"));
  } else {
    Serial.print(F("RC522 gefunden (VersionReg 0x"));
    Serial.print(v, HEX); Serial.println(F(")"));
  }

  Serial.print(F("Erlaubte Karten: ")); Serial.println(allowedCount);
  Serial.println(F("Bereit. Karte an das Terminal halten..."));
}

// ---------- Loop ----------
void loop() {
  // Nur reagieren, wenn eine neue Karte da ist
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    delay(40);
    return;
  }

  // UID holen (uid.size ist 4, 7 oder 10)
  char uidHex[21];
  uidToHex(rfid.uid.uidByte, rfid.uid.size, uidHex);
  Serial.print(F("Karte: ")); Serial.print(uidHex);

  if (isAllowed(uidHex)) {
    Serial.println(F("  -> ZUTRITT"));
    setLed(0, 80, 0);        // gruen
    unlockDoor();            // Relais "klick"
    delay(DOOR_OPEN_MS);
    lockDoor();
  } else {
    Serial.println(F("  -> VERWEIGERT"));
    setLed(80, 0, 0);        // rot
    delay(1500);
  }

  setLed(0, 0, 15);          // zurueck in den Ruhezustand

  // Karte sauber abschliessen, damit sie nicht sofort erneut gelesen wird
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}
