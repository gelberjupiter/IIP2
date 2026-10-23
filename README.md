# IIP2 – RFID/NFC-Projekt (HSLU)

Firmware für die zwei ESP32 im HSLU-IIP-Projekt. Das Projekt ist edukativ und defensiv gedacht: Es werden nur eigene Karten verwendet.

| Ordner | Gerät | Was es macht |
|---|---|---|
| [`toolkit/`](toolkit/) | **ESP32 #1 – Toolkit** | Handgerät mit OLED-Menü und Drehencoder. Der PN532 liest die UID von MIFARE-Karten und zeigt sie an. |
| [`terminal/`](terminal/) | **ESP32 #2 – Referenz-Terminal** | Nachbau eines Türlesers. Der RC522 liest eine Karte und vergleicht die UID mit einer Liste erlaubter Karten. Bei einem Treffer zieht das Relais an („Tür auf“). |

Beide Ordner sind eigenständige PlatformIO-Projekte. Die genaue Verkabelung und die Anleitung zum Hochladen stehen im README des jeweiligen Ordners.

## Kurzanleitung

```bash
brew install platformio            # einmalig (sonst: pip install platformio)
git clone git@github.com:gelberjupiter/IIP2.git
cd IIP2/toolkit                    # oder: cd IIP2/terminal
pio run -t upload                  # bauen und flashen
pio device monitor -b 115200       # serielle Ausgabe ansehen
```
