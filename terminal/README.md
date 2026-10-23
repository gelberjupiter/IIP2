# Referenz-Terminal (Türleser-Nachbau) – ESP32 #2

Firmware für das Referenz-Terminal aus dem HSLU-IIP-Projekt. Das Projekt ist edukativ und defensiv gedacht: Es werden nur eigene Karten verwendet.

## Was das Gerät macht

Das Terminal ist ein einfacher Nachbau eines Türlesers:

1. Der **RC522** liest eine MIFARE-Karte (13,56 MHz) und holt deren UID.
2. Die UID wird mit der Liste `allowedUIDs[]` in `src/main.cpp` verglichen.
3. Ist die Karte erlaubt, leuchtet die LED **grün**, und das **Relais zieht 3 Sekunden an** („Tür auf“).
   Ist sie nicht erlaubt, leuchtet die LED **1,5 Sekunden rot**.
4. Danach geht das Terminal zurück in den Ruhezustand und leuchtet **dezent blau**.

Jede gelesene UID erscheint im seriellen Monitor:

```
=== Terminal startet ===
RC522 gefunden (VersionReg 0x92)
Erlaubte Karten: 1
Bereit. Karte an das Terminal halten...
Karte: 04A1B2C3  -> VERWEIGERT
Karte: A1B2C3D4  -> ZUTRITT
```

> **Bewusste Schwachstelle:** Das Terminal prüft nur die UID der Karte. Die lässt sich mit wenig Aufwand auslesen und auf eine andere Karte klonen. Genau das soll das Projekt zeigen. Echte Zutrittssysteme sollten sich deshalb nicht allein auf die UID verlassen.

## Hardware und Verkabelung

| Bauteil | Anschluss am ESP32 |
|---|---|
| **RC522** (SPI, **nur 3,3 V!**) | 3,3V → 3V3, GND → GND, SDA/SS → GPIO5, SCK → GPIO18, MOSI → GPIO23, MISO → GPIO19, RST → GPIO4 |
| **Relais-Modul** (Active-Low) | DC+ → 5V, DC− → GND, IN1 → GPIO26 |
| **RGB-LED WS2812** (1 Pixel) | Di → GPIO32, 5V → 5V, GND → GND |

Die Kontakte des Relais (COM/NO/NC) sind potentialfrei. Sie steuern nur einen Türöffner-Dummy. **Keine Netzspannung anschliessen.**

Versorgt wird das Terminal über ein USB-C-Netzteil direkt am ESP32.

> **Relais muss ein 5-V-Typ sein.** Ein 24-V-Relais (z. B. „SRD-24VDC-…“) zieht mit den ca. 5 V vom ESP32 nicht an, man hört dann kein Klicken. Gesucht ist ein „1-Kanal 5V Relais Modul, Optokoppler, Low-Level-Trigger“, idealerweise eines mit Jumper JD-VCC oder dem Hinweis „3.3V kompatibel“.

Schaltet dein Modul bei HIGH statt bei LOW, stellst du in `src/main.cpp` `RELAY_ACTIVE_LOW` auf `false` oder steckst den Jumper H/L des Moduls auf **L**.

## Eigene Karte eintragen

1. Firmware flashen (siehe unten) und den seriellen Monitor öffnen.
2. Die Karte an den Leser halten. Im Monitor erscheint z. B. `Karte: 04A1B2C3  -> VERWEIGERT`.
3. Die UID in Grossbuchstaben und ohne Leerzeichen in `src/main.cpp` eintragen:

   ```cpp
   const char* allowedUIDs[] = {
     "04A1B2C3",      // meine Karte
     "11223344556677",// weitere Karte, je eine Zeile, Komma nicht vergessen
   };
   ```

   Zeigt ein anderes Lesegerät die UID mit Leerzeichen und in Kleinbuchstaben an (z. B. `a1 b2 c3 d4`), wird daraus `"A1B2C3D4"`.
4. Neu flashen. Danach zeigt dieselbe Karte `-> ZUTRITT`.

> **Echte UIDs nicht ins öffentliche Repo committen.** Im Repo steht nur der Platzhalter `"DEADBEEF"`. Trag deine Karte nur lokal ein, sonst kann jeder sie klonen.

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
cd IIP2/terminal
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

> **Achtung, wenn Toolkit und Terminal am selben PC hängen:** Beide Boards erscheinen als `/dev/cu.usbserial-…`. Vor dem Flashen mit `pio device monitor` prüfen, welche Firmware gerade darauf läuft („Toolkit startet“ oder „Terminal startet“), damit du nicht das falsche Board überschreibst.

### So wurde es hochgeladen

Getestet mit einem ESP32-WROOM-32 (ESP32-D0WD-V3, Revision v3.1) an einem Mac, PlatformIO Core 6.2.0, Plattform `espressif32@^6.9.0`. Ab Werk lief auf dem Board die Espressif-AT-Firmware. Das Board erschien als `/dev/cu.usbserial-0001` und wurde mit `pio run -t upload --upload-port /dev/cu.usbserial-0001` geflasht. Den Boot-Knopf musste man dabei nicht drücken.

## Fehlersuche

| Meldung / Symptom | Ursache und Lösung |
|---|---|
| `FEHLER: RC522 nicht gefunden.` | Verkabelung der SPI-Pins (5/18/23/19) und von RST (4) prüfen. Der RC522 darf **nur an 3,3 V** hängen, 5 V kann ihn zerstören. |
| Karte wird nicht gelesen | Die Karte flach und nah (unter 2 cm) an die Antenne halten. Der RC522 liest nur 13,56-MHz-Karten, keine 125-kHz-Badges. |
| `ZUTRITT`, aber das Relais klickt nicht | Prüfen, ob es ein **5-V-Relais** ist (siehe oben). Danach die LEDs am Modul ansehen: Leuchtet keine, fehlt der Strom. Leuchtet nur Power, kommt das Signal nicht an (Pin oder Jumper H/L prüfen). Leuchtet der Kanal, aber nichts klickt, fehlt meist der Jumper VCC–JD-VCC. |
| Relais bleibt dauernd an | Das Modul schaltet bei HIGH. Jumper auf **L** stecken oder `RELAY_ACTIVE_LOW` auf `false` setzen. |
| Upload hängt bei `Connecting...` | Den **BOOT**-Knopf am ESP32 gedrückt halten, bis das Schreiben beginnt. |

## Bibliotheken

Die Abhängigkeiten stehen in `platformio.ini` und werden automatisch installiert:

- MFRC522 (miguelbalboa)
- Adafruit NeoPixel
