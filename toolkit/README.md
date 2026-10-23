# RFID/NFC-Toolkit – ESP32 #1

Firmware für das RFID/NFC-Toolkit aus dem HSLU-IIP-Projekt. Das Projekt ist edukativ und defensiv gedacht: Es werden nur eigene Karten gelesen.

## Was das Gerät macht

Ein ESP32 mit OLED-Display, Drehencoder, NFC-Leser und RGB-LED. Bedient wird es über ein Menü auf dem Display:

| Menüpunkt | Funktion |
|---|---|
| **Karte lesen** | Der PN532 sucht nach einer 13,56-MHz-Karte (MIFARE / ISO14443A). Wird eine gefunden, erscheint ihre UID auf dem OLED und im seriellen Monitor, und die LED leuchtet kurz grün. |
| **LED-Test** | Die LED zeigt nacheinander Rot, Grün, Blau und Weiss, danach geht es zurück ins Menü. |
| **Info** | Zeigt an, ob der PN532 und das OLED beim Start gefunden wurden. |

**Bedienung:** Drehen wählt einen Menüpunkt aus, Drücken bestätigt. In „Karte lesen“ und „Info“ führt ein Druck auf den Knopf zurück ins Menü.

**Farben der LED:** Blau beim Start, schwaches Blau, während auf eine Karte gewartet wird, Grün, wenn eine Karte gelesen wurde.

Der RDM6300 (125 kHz) ist in dieser Version noch nicht eingebaut.

## Hardware und Verkabelung

| Bauteil | Anschluss am ESP32 |
|---|---|
| **PN532** (I2C, Adresse 0x24) | VCC → 3V3, GND → GND, SDA → GPIO21, SCL → GPIO22 |
| **OLED SSD1306** 128×64 (I2C, Adresse 0x3C) | VDD → 3V3, GND → GND, SDA → GPIO21, SCK → GPIO22 |
| **Drehencoder KY-040** | CLK → GPIO25, DT → GPIO26, SW → GPIO33, VCC → 3V3, GND → GND |
| **RGB-LED WS2812** (1 Pixel) | Di → GPIO27, 5V → 5V, GND → GND |

PN532 und OLED hängen am selben I2C-Bus. Die Pins IRQ und RESET des PN532 sind nicht verdrahtet.

> **Wichtig, DIP-Schalter am PN532:** Für I2C muss **Schalter 1 auf ON** und **Schalter 2 auf OFF** stehen. Der PN532 liest die Schalter nur beim Einschalten. Nach dem Umstellen also das USB-Kabel kurz abziehen und wieder einstecken.

Im Betrieb wird das Gerät über eine Powerbank per USB versorgt, zum Flashen hängt es per USB am PC.

## Firmware hochladen

Das Projekt ist ein [PlatformIO](https://platformio.org/)-Projekt. Die Bibliotheken lädt PlatformIO beim ersten Build automatisch herunter.

### 1. PlatformIO installieren (einmalig)

```bash
# macOS
brew install platformio

# andere Systeme
pip install platformio
```

Alternativ geht auch die PlatformIO-Erweiterung für VS Code.

### 2. Projekt holen

```bash
git clone git@github.com:gelberjupiter/IIP2.git
cd IIP2/toolkit
```

### 3. Bauen, flashen, Monitor öffnen

ESP32 per USB anschliessen, dann:

```bash
pio device list                 # Port des ESP32 finden, z. B. /dev/cu.usbserial-0001
pio run                         # bauen
pio run -t upload               # flashen (Port wird meist automatisch erkannt)
pio device monitor -b 115200    # serielle Ausgabe ansehen (Beenden: Ctrl+C)
```

Wird der Port nicht automatisch erkannt, gibst du ihn mit `--upload-port` beim Flashen bzw. `-p` beim Monitor an.

Nach einem erfolgreichen Start erscheint im Monitor:

```
=== Toolkit startet ===
PN532 gefunden, FW 1.6
```

Die zwei Zeilen `__pinMode(): Invalid pin selected` beim Start sind harmlos. Sie kommen daher, dass IRQ und RESET des PN532 auf `-1` stehen, weil sie nicht verdrahtet sind.

### So wurde es hochgeladen

Getestet mit einem ESP32-DevKit an einem Mac, PlatformIO Core 6.2.0, Plattform `espressif32@^6`. Das Board erschien als `/dev/cu.usbserial-0001` und wurde mit `pio run -t upload --upload-port /dev/cu.usbserial-0001` geflasht. Den Boot-Knopf musste man dabei nicht drücken.

## Fehlersuche

| Meldung / Symptom | Ursache und Lösung |
|---|---|
| `i2cRead returned Error 263`, danach `FEHLER: PN532 nicht gefunden.` | Der PN532 ist nicht im I2C-Modus und blockiert den Bus. DIP-Schalter prüfen (1 = ON, 2 = OFF) und das Gerät einmal komplett vom Strom trennen. |
| `FEHLER: OLED (0x3C) nicht gefunden.` | Verkabelung von SDA/SCK und die Stromversorgung des OLED prüfen. |
| Upload hängt bei `Connecting...` | Den **BOOT**-Knopf am ESP32 gedrückt halten, bis das Schreiben beginnt. |
| Port fehlt | Anderes USB-Kabel versuchen, manche Kabel können nur laden. Eventuell fehlt der Treiber für den USB-Seriell-Chip (CP210x oder CH340). |
| LED flackert oder zeigt falsche Farben | Die WS2812 läuft an 5 V, das Datensignal kommt aber mit 3,3 V vom ESP32. Das ist grenzwertig. Abhilfe schafft ein Pegelwandler oder eine Versorgung der LED mit 3,3 V. |

## Bibliotheken

Die Abhängigkeiten stehen in `platformio.ini` und werden automatisch installiert:

- Adafruit PN532
- Adafruit SSD1306 + Adafruit GFX
- Adafruit NeoPixel
- ESP32Encoder
