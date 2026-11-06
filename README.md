# STM32F103RB C++ Template (CMake + libopencm3)

## Projektziel

Dieses Projekt ist ein **minimaler, sauberer Startpunkt** für Embedded-C++-Entwicklung auf dem **STM32 Nucleo-F103RB** mit:

- **CMake** Buildsystem
- **Debug/Release** Build-Typen
- **libopencm3** als Open-Source-Hardware-Abstraktion
- **OpenOCD** für Flashing
- **VS Code Debugging** über ST-LINK

Der aktuelle Stand implementiert als ersten Schritt:

- LED-Blink auf **LD2 (PA5)**
- UART-Ausgabe über **USART2 (PA2/PA3, ST-LINK Virtual COM Port)**
- Debug-Setup für Breakpoints und Step-Debugging
- **Speicherlayout-Demo** für `.data`, `.bss`, `.rodata`, Stack und Heap

---

## Struktur

- [CMakeLists.txt](/workspaces/tea24-stm32/CMakeLists.txt): Firmware-Target, Compiler/Linker-Flags, Flash-Target
- [CMakePresets.json](/workspaces/tea24-stm32/CMakePresets.json): `debug`/`release` Configure- und Build-Presets
- [cmake/arm-none-eabi-gcc.cmake](/workspaces/tea24-stm32/cmake/arm-none-eabi-gcc.cmake): ARM-Toolchain
- [cmake/libopencm3.cmake](/workspaces/tea24-stm32/cmake/libopencm3.cmake): Fetch + Build von libopencm3
- [ld/stm32f103rb.ld](/workspaces/tea24-stm32/ld/stm32f103rb.ld): Linker Script für STM32F103RB (128 KB Flash / 20 KB RAM)
- [src/startup_stm32f103rb.c](/workspaces/tea24-stm32/src/startup_stm32f103rb.c): Vector Table + Reset-Startup
- [src/main.cpp](/workspaces/tea24-stm32/src/main.cpp): Blinky + UART Demo
- [.vscode/launch.json](/workspaces/tea24-stm32/.vscode/launch.json): Cortex-Debug Launch-Konfiguration

---

## Voraussetzungen

Im Dev-Container sollten verfügbar sein:

- `cmake` (>= 3.21)
- `ninja`
- `arm-none-eabi-gcc` Toolchain
- `openocd`

Zusätzlich in VS Code:

- CMake Tools
- Cortex-Debug (bereits in [devcontainer.json](/workspaces/tea24-stm32/.devcontainer/devcontainer.json) ergänzt)

---

## Schnellstart für Einsteiger (mit oder ohne Hardware)

Wenn du neu im STM32-Umfeld bist, folge genau diesen Schritten:

1. **Debug-Build erzeugen**
   ```bash
   cmake --preset debug
   cmake --build --preset debug
   ```
2. **Entscheiden, wie du starten willst**
   - **Mit echter Hardware** → Abschnitt **Flashen** + **Debuggen in VS Code**
   - **Ohne Hardware (Emulation)** → Abschnitt **Emulation mit Renode**
3. **UART-Ausgabe prüfen** (egal ob Board oder Renode), damit du sofort siehst,
   dass Firmware und Toolchain korrekt laufen.

---

## Build

### Debug

```bash
cmake --preset debug
cmake --build --preset debug
```

### Release

```bash
cmake --preset release
cmake --build --preset release
```

Build-Artefakte liegen unter:

- [build/debug/](/workspaces/tea24-stm32/build/debug)
- [build/release/](/workspaces/tea24-stm32/build/release)

Nach dem Build werden zusätzlich erzeugt:

- `stm32f103rb-app.hex`
- `stm32f103rb-app.bin`

---

## Flashen

Nucleo per USB anschließen, dann:

```bash
cmake --build --preset debug --target flash
```

Das Flash-Target nutzt OpenOCD mit `board/st_nucleo_f103rb.cfg`.

---

## Debuggen in VS Code

1. Debug-Build ausführen.
2. In Run & Debug die Konfiguration **"STM32F103RB Debug (OpenOCD)"** wählen.
3. Starten, Breakpoints setzen, `main` wird als Entry genutzt.

---

## Emulation mit Renode (ohne Board)

Wenn du noch kein Nucleo-Board angeschlossen hast, kannst du dieselbe Firmware
im Emulator starten:

1. Debug-Build ausführen:
   ```bash
   cmake --preset debug
   cmake --build --preset debug
   ```
2. Renode mit dem Projektskript starten:
   ```bash
   renode --disable-xwt --console renode/nucleo.resc
   ```
3. Im Renode-Monitor `start` eingeben.
4. Optional in VS Code die Konfiguration **"STM32F103RB Debug (Renode)"** nutzen.

Die UART-Ausgabe erscheint über `showAnalyzer sysbus.usart2`.

Eine ausführliche Schritt-für-Schritt-Anleitung findest du in
[doc/renode.md](/workspaces/tea24-stm32/doc/renode.md).

---

## UART testen

Die Demo sendet bei 115200 Baud:

- Initial: Startup-Meldung + Adressen/Sizes der Speicherbereiche
- Ein Heap-Allocationsbeispiel (`_sbrk(64)`)
- Danach periodisch: `tick` inkl. `.data`/`.bss`-Zähler

Auf dem Host den seriellen Port des ST-LINK VCP öffnen (z. B. mit `screen`, `minicom` oder VS Code Serial Monitor).

---

## Speicherverwaltung im Beispiel

### Speicherkarte (vereinfacht)

```text
FLASH 0x0800_0000 .. 0x0801_FFFF (128 KiB)
+--------------------------------------------------------------+
| .isr_vector | .text/.init/.fini | .rodata | .ARM.extab/exidx|
+--------------------------------------------------------------+
                        |
                        | Startup kopiert Initialwerte von .data
                        v
RAM   0x2000_0000 .. 0x2000_4FFF (20 KiB)
+--------------------------------------------------------------+
| .data | .bss | .noinit |        HEAP ->         | <- STACK  |
|       |      |         | _heap_start .. _heap_end|          |
+--------------------------------------------------------------+
                                   ^                ^
                              _heap_end =      _stack_top
                              _stack_limit
```

### `.rodata`

Konstante Daten (z. B. Stringliterale wie `kRodataBanner`) liegen in Flash.

### `.data`

Initialisierte globale/statische Variablen (z. B. `g_data_counter`) liegen zur Laufzeit in RAM, ihr Initialwert wird aber aus Flash kopiert.

### `.bss`

Null-initialisierte globale/statische Variablen (z. B. `g_bss_counter`, `g_bss_buffer`) liegen in RAM und werden beim Reset auf `0` gesetzt.

### Stack

Der Main-Stack-Pointer startet bei `_stack_top` (RAM-Ende) und wächst nach unten. Das Linker-Script reserviert eine Mindestgröße über `_Min_Stack_Size`.

### Heap

Der Heap startet bei `_heap_start` und wächst nach oben bis `_heap_end` (= `_stack_limit`).  
In [src/syscalls.c](/workspaces/tea24-stm32/src/syscalls.c) ist `_sbrk` implementiert, damit `malloc/new` bzw. die Demo-Allocation funktionieren.

### `_sbrk`-Doku (aktueller Stand)

`_sbrk` ist die primitive Speicher-Erweiterungsfunktion, die newlib intern für `malloc`/`new` nutzt.
Unsere Implementierung in [src/syscalls.c](/workspaces/tea24-stm32/src/syscalls.c) arbeitet als **einfacher linearer Bump-Allocator**:

1. Beim ersten Aufruf wird `current_heap_end` auf `_heap_start` gesetzt.
2. Für `increment < 0` wird sofort mit Fehler (`errno = ENOMEM`, Rückgabe `(void*)-1`) abgebrochen.
3. Es wird geprüft, ob `current_heap_end + increment > _heap_end`.
4. Wenn genug Platz da ist:
   - alter Heap-Zeiger wird gemerkt
   - `current_heap_end` um `increment` erhöht
   - alter Zeiger wird zurückgegeben

Kurzform:

```c
old = current_heap_end;
current_heap_end += increment;
return old;
```

#### Eigenschaften des aktuellen Algorithmus

- **Monoton wachsend**: Heap wächst nur nach oben.
- **Kein Freigeben auf `_sbrk`-Ebene**: negative Inkremente sind absichtlich nicht erlaubt.
- **Grenzschutz gegen Stack-Kollision**: Grenze ist `_heap_end` (aus dem Linker-Script, identisch zu `_stack_limit`).
- **Deterministisch und klein**: gut für die erste Bare-Metal-Demo.

#### Wichtige Grenzen

- **Nicht thread-safe/reentrant** (kein Locking).
- Kein Schutz gegen Fragmentierung auf `malloc`-Ebene (das ist Aufgabe der libc-Allocator-Logik, nicht von `_sbrk` selbst).
- Bei Heap-Erschöpfung liefern `malloc/new` am Ende Fehler, weil `_sbrk` `ENOMEM` signalisiert.

---

## Relevante Dateien für das Thema

- [ld/stm32f103rb.ld](/workspaces/tea24-stm32/ld/stm32f103rb.ld): Platzierung und Grenzen aller Bereiche
- [src/startup_stm32f103rb.c](/workspaces/tea24-stm32/src/startup_stm32f103rb.c): `.data` kopieren, `.bss` nullen, C/C++ Init
- [src/main.cpp](/workspaces/tea24-stm32/src/main.cpp): sichtbare Runtime-Demo per UART
- [src/syscalls.c](/workspaces/tea24-stm32/src/syscalls.c): `_sbrk` und minimale Syscall-Stubs

---

## Nächste sinnvolle Ausbaustufen

- UART RX mit Interrupts/Ringbuffer
- `printf`-Retargeting auf USART2
- Trennung in Board Support Package (BSP) + App-Layer
- Unit-Tests für host-seitige Logik
- Optional FreeRTOS Integration
