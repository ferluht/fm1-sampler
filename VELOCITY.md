# FM-1 Sampler v0.37 mit Anschlagdynamik

Die [Sampler-Firmware v0.37](https://github.com/ferluht/fm1-sampler/tree/1de7473e902cd1010982d841cfab2952bb3ba2f9) bleibt die Basis für Display, LEDs, Regler und USB-Audio. Über das bestehende MIDI-Note-On sendet sie jetzt den gemessenen Anschlag. Alle Töne eines Akkords nutzen den Anschlag derselben Taste. Die 14 Bedientasten behalten ihre digitalen MIDI-Meldungen.

Die 20 unteren Spieltasten F3 bis C5 werden über die ADC-Zeilen PA7 und PA8 erfasst. Die sieben höchsten Tasten nutzen ihren digitalen Kontakt und, wenn verfügbar, die experimentelle Messung über die LED-Leitung PA10. Ohne brauchbare Messung senden sie Velocity 100. Für Master-Regler, Batteriespannung und Spieltasten nutzt der Scanner Zeitfenster desselben ADCs. Bei einem Ausfall der ADC-Messung bleiben die Spieltasten digital bedienbar.

## Anschlag einstellen und lernen

Die globale Kurve für F3 bis C5 folgt CTL‑1. Der Bereich von −5 bis +5 ändert den Exponenten der Velocity-Kurve. Bei −5 liefert derselbe Anschlag einen höheren MIDI-Wert, bei +5 einen niedrigeren. 0 entspricht der Ausgangskurve. [CTL‑1-Handbuch](https://github.com/charlesvestal/fm1-ctl/blob/e9c9c62895c48946ec21cbf3905cec1f7be62da9/docs/MANUAL.md#L339-L369) · [Implementierung in fm1-base](https://github.com/charlesvestal/fm1-base/blob/c8e05997ce970875038156d2bbd69e712eac1543/lib/touch.c#L20-L39)

Der optionale Lernmodus aus Velo passt die Grenzen für leise und kräftige Anschläge pro physischer Taste an, während man spielt. Der Lernmodus ist beim Start aus. Speichern hält die Anpassung sofort an und sendet das aktuelle Profil an den Companion. Anwenden lädt ein eingefrorenes Profil. Zurücksetzen stellt Kurve und Tastengrenzen auf die bekannten Startwerte. Die Firmware hält das Profil im Arbeitsspeicher. Die sieben höchsten Tasten nutzen weiterhin ihre separate PA10-Kurve.

Beim ersten physischen Versuch des Velo-Autors führte das Lernen häufiger zu extremen Velocity-Werten. Für die Beurteilung eines Profils sind verschieden starke Anschläge über mehrere Tasten am echten FM‑1 wichtig. [Velo-Laborbericht](https://github.com/charlesvestal/fm1-velocity/blob/260b97850eb5943fd187d3166fc547195de15190/notes/01-lab.md#L245-L279)

## Tests und Build

Den portablen Sensor- und Protokolltest aus dem Repository ausführen.

```sh
velocity_test=$(mktemp -d)
cc -std=c11 -O2 -ffp-contract=off -fwrapv \
  -I firmware/app -I firmware/app/board/velocity \
  firmware/hosttest/velocity.c -o "$velocity_test/test-velocity"
"$velocity_test/test-velocity"
```

Für das Firmware-Paket auf Linux x86‑64 Python 3 und die JieLi-Toolchain `jieli-linux-toolchains-20260730.1` verwenden. Das zweite Argument zeigt auf `pi32v2/bin` der entpackten Toolchain. Das Ausgabeverzeichnis muss vorhanden sein und sollte für jeden Build frisch angelegt werden.

```sh
velocity_build=$(mktemp -d)
python3 tools/build_velocity_firmware.py firmware /pfad/zur/toolchain/pi32v2/bin \
  "$velocity_build/fm1-sampler-velocity.fwsc"
```

## Quellen und Lizenz

- Sensormodell und Kurve stammen aus `fm1-base` von Charles Vestal (MPL-2.0), aufbauend auf Velo, MONK‑1 und CTL‑1. [sense.c](https://github.com/charlesvestal/fm1-base/blob/c8e05997ce970875038156d2bbd69e712eac1543/lib/sense.c) · [touch.c](https://github.com/charlesvestal/fm1-base/blob/c8e05997ce970875038156d2bbd69e712eac1543/lib/touch.c)
- Die Funktionen für den ADC und die drei Abtastphasen stammen aus `felucca-bsp` (MPL-2.0). Die Anpassung verwendet den vorhandenen SPI-Scanner des Samplers. [fm1_input.h](https://github.com/hugelton/felucca-bsp/blob/fbc254dc4b1cbe88dafd3d6437821fe2393ef23b/boards/fm1/fm1_input.h) · [fm1_adc.h](https://github.com/hugelton/felucca-bsp/blob/fbc254dc4b1cbe88dafd3d6437821fe2393ef23b/boards/fm1/fm1_adc.h)
- Der Lizenztext für die übernommenen Dateien liegt in [VELOCITY-LICENSE-MPL-2.0.txt](firmware/VELOCITY-LICENSE-MPL-2.0.txt). Die Änderungen an den Sampler-Dateien verwenden deren GPL-3.0-Lizenz mit Linking Exception. [Sampler-Lizenz](https://github.com/ferluht/fm1-sampler/blob/v0.37/firmware/LICENSE.md)
