# Renode + STM32F103 Nucleo (Devcontainer, ohne Docker-in-Docker)

Zusammenfassung für das Projekt
[graugans/tea24-stm32f103](https://github.com/graugans/tea24-stm32f103)
(CMake + libopencm3, Nucleo-F103RB, 128 KB Flash / 20 KB RAM).

---

## 1. Was ist Renode?

Renode ist ein Open-Source-Simulationsframework für eingebettete Systeme
(Antmicro). Im Gegensatz zu QEMU simuliert es ganze Boards samt Peripherie
(UART, GPIO, Timer, SPI/I²C, Sensoren).

- Boards werden in `.repl`-Dateien beschrieben (CPU, Speicher, Peripherie).
- Die unveränderte Firmware (ELF) läuft direkt, auch mit libopencm3/HAL.
- GDB-Anbindung über `machine StartGdbServer 3334`.
- Automatisierung per `.resc`-Skript, Tests per Robot Framework (`renode-test`).

**QEMU-Alternative:** Für reine Core/RAM-Experimente (z. B. Stackframes)
reicht `qemu-system-arm -M mps2-an385 -cpu cortex-m3`. Den STM32F103 selbst
emuliert QEMU upstream nicht.

---

## 2. Renode im Devcontainer installieren

Kein Docker-in-Docker nötig: Renode wird direkt im Devcontainer installiert,
am besten in der `Dockerfile`, damit es beim Image-Build erledigt ist.

```dockerfile
ARG RENODE_VERSION=<VERSION>
RUN wget -q https://github.com/renode/renode/releases/download/v${RENODE_VERSION}/renode-${RENODE_VERSION}.linux-portable.tar.gz \
 && tar xf renode-${RENODE_VERSION}.linux-portable.tar.gz -C /opt \
 && mv /opt/renode_${RENODE_VERSION}_portable /opt/renode \
 && ln -s /opt/renode/renode /usr/local/bin/renode \
 && rm renode-*.tar.gz
```

Hinweise:

- `<VERSION>` von der Releases-Seite (`github.com/renode/renode/releases`) eintragen.
- Nach dem Build mit `renode --version` prüfen, ob Pfad und Ordnername des
  entpackten Tarballs stimmen (können je Release abweichen).
- Alternative: `.deb`-Paket mit `apt install ./renode_<VERSION>_amd64.deb`.
- Für `renode-test` zusätzlich: `pip install -r /opt/renode/tests/requirements.txt`.

---

## 3. Headless starten

Im Devcontainer/Codespace gibt es kein Display:

```bash
renode --disable-xwt --console script.resc
```

GDB läuft im selben Container wie Renode, daher genügt
`target remote :3334` ohne Port-Forwarding. Weiterleitung ist nur nötig,
wenn du von außerhalb des Containers verbindest:

```json
"forwardPorts": [3334],
"portsAttributes": { "3334": { "label": "Renode GDB" } }
```

---

## 4. Renode-Skript für das Nucleo-F103RB

Datei z. B. `renode/nucleo.resc` im Projekt:

```
:name: Nucleo F103RB
using sysbus
mach create "nucleo"
machine LoadPlatformDescription @platforms/boards/nucleo_f103rb.repl
sysbus LoadELF @build/debug/stm32f103rb-app.elf

showAnalyzer sysbus.usart2
machine StartGdbServer 3334
```

- Den ELF-Dateinamen nach dem Build unter `build/debug/` prüfen.
- Gibt es `nucleo_f103rb.repl` in deiner Renode-Version nicht, nimm
  `platforms/cpus/stm32f103.repl`. Vorhandene Plattformen findest du im
  Renode-Ordner unter `platforms/boards/`.
- Start: `renode --disable-xwt --console renode/nucleo.resc`, dann im
  Renode-Monitor `start`.

### Peripherie des Nucleo-F103RB

| Funktion                     | Pin / Peripherie        |
|------------------------------|-------------------------|
| User-LED LD2                 | PA5                     |
| User-Taster B1               | PC13                    |
| ST-LINK Virtual COM Port     | USART2 (PA2/PA3)        |

`showAnalyzer sysbus.usart2` zeigt die UART-Ausgabe der Firmware im Terminal.

---

## 5. Debuggen in VS Code (Cortex-Debug)

Zusätzlich zur vorhandenen OpenOCD-Konfiguration eine zweite Konfiguration
in `.vscode/launch.json`, die sich mit Renode verbindet:

```json
{
  "name": "STM32F103RB Debug (Renode)",
  "type": "cortex-debug",
  "request": "launch",
  "servertype": "external",
  "gdbTarget": "localhost:3334",
  "executable": "${workspaceFolder}/build/debug/stm32f103rb-app.elf",
  "cwd": "${workspaceFolder}",
  "runToEntryPoint": "main"
}
```

Renode muss dafür vorher laufen (GDB-Server aktiv). Flashen entfällt, der
ELF wird per `sysbus LoadELF` geladen.

Alternativ rein per Kommandozeile:

```bash
gdb-multiarch build/debug/stm32f103rb-app.elf -ex "target remote :3334"
```

Falls `arm-none-eabi-gdb` in deiner Umgebung fehlt (z. B. Debian/Ubuntu),
ist `gdb-multiarch` der empfohlene Ersatz.

---

## 6. Stolpersteine bei libopencm3 in Renode

- **Clock-Setup hängt:** Wartet der Code beim PLL/HSE-Hochfahren in einer
  Schleife (z. B. auf `HSERDY`), ist meist ein RCC-Flag nicht modelliert.
  Zuerst mit dem internen HSI-Takt testen oder
  `logLevel -1 sysbus.rcc` nutzen, um die Zugriffe zu sehen.
- **LED/Taster:** Ob die `.repl` LD2 und B1 schon als `Led`/`Button`
  anbindet, steht in der Datei. Sonst lassen sie sich selbst ergänzen.
- **Plattformname:** Siehe Abschnitt 4 (Fallback auf `stm32f103.repl`).

---

## 7. Stackframes am Cortex-M3 erklären (Demo-Idee)

Das Projekt bringt dafür schon alles mit: Der Stack startet bei `_stack_top`
(RAM-Ende) und wächst nach unten Richtung `_stack_limit`. Der Heap wächst
von unten bis `_heap_end` (= `_stack_limit`).

Vorschlag für `main.cpp`: eine kleine rekursive, nicht inlinebare Funktion,
die pro Ebene Frame-Adresse und SP über USART2 ausgibt.

```cpp
__attribute__((noinline))
static void recurse(unsigned depth) {
    volatile unsigned marker = depth;
    // Pro Ebene ausgeben: depth, __builtin_frame_address(0), aktuellen SP
    if (depth < 8) {
        recurse(depth + 1);
    }
    (void)marker;
}
```

Gebaut mit `-O0 -g`, sieht man in der UART-Ausgabe, wie der Stack Ebene für
Ebene Richtung `_stack_limit` wächst.

Im GDB denselben Ablauf nachvollziehen:

```
x/16wx $sp      # Stack-Inhalt
info frame      # Frame-Infos
bt              # Backtrace
p $sp           # SP nach jedem stepi
stepi           # Prolog/Epilog einzeln durchgehen
```

Typischer Prolog: `push {r7, lr}`, `sub sp, #N`, `add r7, sp, #0`.

Konzepte, die sich daran gut zeigen lassen:

- **AAPCS:** Argumente in r0-r3, Rücksprungadresse im LR, r4-r11 callee-saved.
- **Exception-Frame:** Bei einer Exception stackt die Hardware automatisch
  8 Wörter (r0-r3, r12, LR, PC, xPSR). Das erklärt auch `EXC_RETURN`,
  MSP vs. PSP und Hard-Fault-Analyse.
- **Stack Overflow:** Rekursion ohne Abbruch zeigt, wie SP in den Heap bzw.
  in `.bss`/`.data` läuft.

---

## 8. Kurz-Checkliste

1. Renode per `Dockerfile` im Devcontainer installieren, `renode --version` prüfen.
2. Debug-Build: `cmake --preset debug && cmake --build --preset debug`.
3. `renode/nucleo.resc` anlegen, ELF-Pfad und `.repl`-Namen prüfen.
4. `renode --disable-xwt --console renode/nucleo.resc`, im Monitor `start`.
5. In VS Code die Renode-Debug-Konfiguration starten oder per GDB auf `:3334` verbinden.
6. Bei Hängern im Startup: RCC/Clock-Setup prüfen (Abschnitt 6).
