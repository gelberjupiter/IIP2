/*
 * RFID/NFC-Toolkit – ESP32 #1 (Toolkit)
 * ============================================================
 * HSLU IIP-Projekt, edukativ/defensiv – nur eigene Karten.
 *
 * Funktion dieser Version:
 *   - Menue auf dem OLED, bedient ueber den Drehencoder
 *     (drehen = Auswahl, Knopf druecken = bestaetigen / zurueck)
 *   - "Karte lesen": PN532 liest eine MIFARE-Karte, zeigt die UID
 *     auf OLED + seriellem Monitor an, RGB-LED wird gruen
 *   - "LED-Test" und "Info" als kleine Extras
 *
 * Der RDM6300 (125 kHz) ist hier noch NICHT drin – kommt spaeter.
 *
 * ------------------------------------------------------------
 * VERKABELUNG (so ist es angeschlossen):
 *   PN532 (I2C, DIP-Schalter auf I2C):
 *     VCC -> 3V3 | GND -> GND | SDA -> GPIO21 | SCL -> GPIO22   (Adr. 0x24)
 *   OLED SSD1306 (I2C, selber Bus):
 *     GND -> GND | VDD -> 3V3 | SCK -> GPIO22 | SDA -> GPIO21   (Adr. 0x3C)
 *   Drehencoder KY-040:
 *     SA(CLK) -> GPIO25 | SB(DT) -> GPIO26 | SW -> GPIO33 | VCC -> 3V3 | GND -> GND
 *   RGB-LED WS2812 (1 Pixel):
 *     Di -> GPIO27 | 5V -> 5V | GND -> GND
 *
 * Strom im Betrieb: Powerbank per USB. Zum Flashen: USB am PC.
 * ============================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32Encoder.h>

// ---------- Pin-Definitionen ----------
#define PIN_SDA     21
#define PIN_SCL     22
#define ENC_SA      25     // CLK
#define ENC_SB      26     // DT
#define ENC_SW      33     // Taster im Drehknopf
#define LED_DI      27     // WS2812 Data In
#define NUM_PIXELS  1

// PN532 im I2C-Modus: IRQ/RESET sind am roten Board nicht verdrahtet.
#define PN532_IRQ   (-1)
#define PN532_RESET (-1)

// OLED
#define OLED_W      128
#define OLED_H      64
#define OLED_ADDR   0x3C

// ---------- Objekte ----------
Adafruit_PN532   nfc(PN532_IRQ, PN532_RESET);
Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, -1);
Adafruit_NeoPixel pixel(NUM_PIXELS, LED_DI, NEO_GRB + NEO_KHZ800);
ESP32Encoder     encoder;

// ---------- Menue / Zustand ----------
enum State { STATE_MENU, STATE_READ, STATE_LEDTEST, STATE_INFO };
State state = STATE_MENU;

const char* menuItems[] = { "Karte lesen", "LED-Test", "Info" };
const int   menuCount   = 3;
int         menuIndex   = 0;

bool pn532Ok = false;      // wurde der PN532 gefunden?
bool oledOk  = false;      // wurde das OLED gefunden?
long lastDetents = 0;      // fuer die Encoder-Auswertung

// ---------- Hilfsfunktionen ----------
void setLed(uint8_t r, uint8_t g, uint8_t b) {
  pixel.setPixelColor(0, pixel.Color(r, g, b));
  pixel.show();
}

// Drehrichtung seit letztem Aufruf: -1, 0 oder +1 (pro Rastung)
int readEncoderDelta() {
  long detents = encoder.getCount() / 2;   // HalfQuad -> 2 Schritte pro Rastung
  long diff = detents - lastDetents;
  if (diff == 0) return 0;
  lastDetents = detents;
  return (diff > 0) ? 1 : -1;
}

// Drehungen ausserhalb des Menues verwerfen (sonst springt die Auswahl)
void syncEncoder() {
  lastDetents = encoder.getCount() / 2;
}

// Entprellter Tastendruck (true genau einmal pro Druck)
bool buttonPressed() {
  static bool     lastStable = HIGH;
  static bool     lastRead   = HIGH;
  static uint32_t lastChange = 0;
  bool now = digitalRead(ENC_SW);          // LOW = gedrueckt (INPUT_PULLUP)

  if (now != lastRead) { lastRead = now; lastChange = millis(); }
  if (millis() - lastChange > 30) {        // 30 ms Entprellzeit
    if (now != lastStable) {
      lastStable = now;
      if (lastStable == LOW) return true;  // nur die fallende Flanke zaehlt
    }
  }
  return false;
}

// ---------- OLED-Anzeigen ----------
void drawMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("TOOLKIT  -  Menue"));
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  for (int i = 0; i < menuCount; i++) {
    int y = 16 + i * 14;
    if (i == menuIndex) {
      display.fillRect(0, y - 2, 128, 13, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(4, y);
    display.println(menuItems[i]);
  }
  display.display();
}

void drawReadScreen(const char* line1, const char* line2) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Karte lesen"));
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
  display.setCursor(0, 20);
  display.println(line1);
  if (line2 && line2[0]) {
    // Gross nur, wenn es in eine Zeile passt (128 px / 12 px = 10 Zeichen)
    display.setTextSize(strlen(line2) <= 10 ? 2 : 1);
    display.setCursor(0, 34);
    display.println(line2);
  }
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print(F("[Knopf] = zurueck"));
  display.display();
}

void drawInfo() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Info"));
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
  display.setCursor(0, 16);
  display.print(F("PN532: "));
  display.println(pn532Ok ? F("OK (0x24)") : F("FEHLT!"));
  display.setCursor(0, 28);
  display.println(oledOk ? F("OLED : OK (0x3C)") : F("OLED : FEHLT!"));
  display.setCursor(0, 40);
  display.println(F("13,56 MHz MIFARE"));
  display.setCursor(0, 56);
  display.print(F("[Knopf] = zurueck"));
  display.display();
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println(F("\n=== Toolkit startet ==="));

  // I2C auf GPIO21/22
  Wire.begin(PIN_SDA, PIN_SCL);

  // RGB-LED
  pixel.begin();
  pixel.setBrightness(60);       // nicht zu grell
  setLed(0, 0, 20);              // dezentes Blau = "boot"

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("FEHLER: OLED (0x3C) nicht gefunden."));
  } else {
    oledOk = true;
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 24);
    display.println(F("  RFID-Toolkit"));
    display.display();
  }

  // PN532
  nfc.begin();
  uint32_t ver = nfc.getFirmwareVersion();
  if (ver) {
    pn532Ok = true;
    Serial.print(F("PN532 gefunden, FW "));
    Serial.print((ver >> 16) & 0xFF); Serial.print('.');
    Serial.println((ver >> 8) & 0xFF);
    nfc.SAMConfig();
  } else {
    Serial.println(F("FEHLER: PN532 nicht gefunden."));
    Serial.println(F("-> DIP auf I2C? VCC an 3V3? SDA=21/SCL=22?"));
  }

  // Drehencoder (KY-040 hat eigene Pull-ups -> keine internen noetig;
  // ESP32Encoder schaltet sonst standardmaessig Pulldowns ein)
  ESP32Encoder::useInternalWeakPullResistors = puType::none;
  encoder.attachHalfQuad(ENC_SA, ENC_SB);
  encoder.clearCount();
  lastDetents = 0;
  pinMode(ENC_SW, INPUT_PULLUP);

  delay(800);
  setLed(0, 0, 0);
  drawMenu();
}

// ---------- Hauptschleife ----------
void handleMenu() {
  int d = readEncoderDelta();
  if (d != 0) {
    menuIndex = (menuIndex + d + menuCount) % menuCount;
    drawMenu();
  }
  if (buttonPressed()) {
    switch (menuIndex) {
      case 0: state = STATE_READ;    drawReadScreen("Karte dranhalten...", ""); setLed(0,0,10); break;
      case 1: state = STATE_LEDTEST; break;
      case 2: state = STATE_INFO;    drawInfo(); break;
    }
  }
}

void handleRead() {
  // Zurueck ins Menue?
  if (buttonPressed()) { state = STATE_MENU; setLed(0,0,0); syncEncoder(); drawMenu(); return; }

  if (!pn532Ok) { drawReadScreen("PN532 fehlt!", ""); return; }

  // Kurzer, nicht-blockierender Leseversuch
  uint8_t uid[10] = {0};     // ISO14443A: UID bis 10 Byte (triple size)
  uint8_t uidLen = 0;
  bool found = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, 80);

  if (found) {
    if (uidLen > sizeof(uid)) uidLen = sizeof(uid);
    // UID als Hex-String bauen
    char hex[3 * 10 + 1] = {0};
    int p = 0;
    for (uint8_t i = 0; i < uidLen; i++) {
      p += sprintf(&hex[p], "%02X ", uid[i]);
    }
    if (p > 0) hex[p - 1] = '\0';   // letztes Leerzeichen weg
    Serial.print(F("Karte: ")); Serial.println(hex);
    drawReadScreen("UID:", hex);
    setLed(0, 60, 0);          // gruen = gelesen
    delay(900);
    setLed(0, 0, 10);          // zurueck auf "warten"
    drawReadScreen("Karte dranhalten...", "");
  }
}

void handleLedTest() {
  // einmal durch die Farben, dann zurueck ins Menue
  uint32_t cols[] = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFFFF};
  const char* names[] = {"ROT", "GRUEN", "BLAU", "WEISS"};
  for (int i = 0; i < 4; i++) {
    setLed((cols[i] >> 16) & 0xFF, (cols[i] >> 8) & 0xFF, cols[i] & 0xFF);
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 20);
    display.println(names[i]);
    display.display();
    delay(500);
  }
  setLed(0, 0, 0);
  state = STATE_MENU;
  syncEncoder();
  drawMenu();
}

void handleInfo() {
  if (buttonPressed()) { state = STATE_MENU; syncEncoder(); drawMenu(); }
}

void loop() {
  switch (state) {
    case STATE_MENU:    handleMenu();    break;
    case STATE_READ:    handleRead();    break;
    case STATE_LEDTEST: handleLedTest(); break;
    case STATE_INFO:    handleInfo();    break;
  }
  delay(5);
}
