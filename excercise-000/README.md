# Übungsaufgabe: Stack, Speichersegmente, Heap und Startup-Code auf dem STM32F103 – mit Renode und GDB

> **Für wen ist diese Aufgabe?**
> Für alle, die noch nie einen Mikrocontroller „von innen" gesehen haben.
> Sie brauchen **keine Hardware**: Alles läuft in einer Emulation (Renode) im Devcontainer.
> Jeder Befehl wird erklärt. Wenn Sie etwas nicht verstehen, ist das kein Problem – lesen Sie den Abschnitt „Kontext" noch einmal und probieren Sie den Befehl einfach aus. Sie können in der Emulation nichts kaputt machen.

## Notwendige git Kommandos

```sh
git status
git branch -a
git switch main
git fetch --all
# create a new local branch based on exercise-000
# (Branchnamen bitte an Ihre Vorlage anpassen)
git switch -c solution-003 upstream/exercise-000
# perform changes
git add src
git commit -m "feat: solve exercise-000 stack and memory task"
# push the changes to the cloud
git push -u origin solution-003
```

## Programm kompilieren

Das Projekt verwendet CMake (mit Presets) und die ARM-Cross-Toolchain `arm-none-eabi-gcc`:

```sh
# Konfigurieren (nur beim ersten Mal bzw. nach CMake-Änderungen)
cmake --preset debug

# Kompilieren (NUR aus dem Projekt-Hauptverzeichnis)
cmake --build --preset debug
```

Die Ergebnisse liegen in `build/debug/`. Die wichtigste Datei ist die **ELF-Datei** (Details siehe Teil 1).

> **Wichtig:** Wir verwenden in dieser Aufgabe immer den **Debug**-Build. Dort optimiert der Compiler (fast) nichts weg, sodass das, was Sie im Debugger sehen, zu dem passt, was im Quelltext steht.

---

## Überblick: Was Sie in dieser Aufgabe tun

Sie arbeiten mit **drei Terminals** gleichzeitig. Das ist am Anfang ungewohnt, aber sehr hilfreich:

| Terminal | Programm | Aufgabe |
|---|---|---|
| **Terminal 1** | `renode` | Der „virtuelle Mikrocontroller". Hier laufen Renode-Befehle. |
| **Terminal 2** | `gdb-multiarch` | Der Debugger. Hier steuern Sie das Programm. |
| **Terminal 3** | normale Shell | Für `nm`, `size`, `objdump` usw. |

In VS Code öffnen Sie weitere Terminals über das Plus-Symbol im Terminal-Fenster (oder `Terminal → New Terminal`).

---

## Kontext: Grundlagen für Einsteigerinnen

### Was ist ein Mikrocontroller – und was ist anders als auf dem PC?

Ihr PC-Programm läuft unter einem Betriebssystem (Linux, Windows). Das Betriebssystem lädt das Programm in den Speicher, gibt ihm virtuellen Speicher und räumt hinterher auf.

Ein **Mikrocontroller** (hier: STM32F103RB mit einem ARM-Cortex-M3-Kern) hat **kein Betriebssystem**. Ihr Programm ist das Einzige, was läuft („Bare Metal"). Dafür gilt:

- Es gibt nur **wenig Speicher**: 128 KiB Flash (dauerhaft, wie eine Festplatte) und 20 KiB RAM (flüchtig, schnell).
- Es gibt **keinen Speicherschutz**: Wer in falschen Speicher schreibt, bekommt keine freundliche Fehlermeldung, sondern *stilles* Chaos.
- **Sie** (bzw. das Linker-Script und der Startup-Code) müssen alles einrichten, was ein Betriebssystem sonst erledigt.

### Host und Target, Cross-Compiler

- **Host** = der Rechner, auf dem Sie entwickeln (Devcontainer, x86 oder ARM64).
- **Target** = der Rechner, auf dem das Programm laufen soll (STM32F103, ARM Cortex-M3).

Der normale `gcc` erzeugt Code für den Host. Wir brauchen einen **Cross-Compiler**, der Code für das Target erzeugt. Er heißt `arm-none-eabi-gcc`. Die Hilfswerkzeuge (`size`, `nm`, `objdump`, …) gibt es ebenfalls mit diesem Präfix, z. B. `arm-none-eabi-nm`. Eine Ausnahme ist der Debugger: Im Devcontainer heißt er `gdb-multiarch` (ein GDB, der mehrere Prozessorarchitekturen versteht, u. a. ARM).

### Was ist Renode?

**Renode** ist ein Emulator für ganze Embedded-Systeme. Er bildet nicht nur die CPU nach, sondern auch Speicher, Timer, UARTs, GPIOs usw. Ihr **unveränderter Firmware-Code** läuft darin so, als säße er auf dem echten Board.

Vorteile für das Lernen:

- Keine Hardware, kein USB, kein Flashen nötig.
- Man kann das Programm jederzeit anhalten und alles anschauen.
- Man kann beliebig oft von vorne beginnen.

Renode wird durch zwei Arten von Dateien beschrieben:

| Datei | Bedeutung |
|---|---|
| `*.repl` (Platform Description) | **Welche Hardware** gibt es? (CPU, RAM-Größe, Adressen der Peripherie) – z. B. `platforms/cpus/stm32f103.repl` |
| `*.resc` (Script) | **Was soll Renode tun?** (Plattform laden, ELF laden, GDB-Server starten) – hier `renode/nucleo.resc` |

### Was ist GDB – und wie arbeitet er mit Renode zusammen?

**GDB** (GNU Debugger) ist ein Programm, mit dem Sie ein anderes Programm *anhalten, Schritt für Schritt ausführen und untersuchen* können.

Bei Embedded-Systemen läuft GDB **nicht auf dem Target**. Stattdessen spricht GDB über das **Remote Serial Protocol** (über eine TCP-Verbindung) mit einem sogenannten **GDB-Server** (auch „GDB-Stub"). Der Stub steuert die (echte oder emulierte) CPU.

```
 Terminal 2                       Terminal 1
+-----------------+   TCP :3333   +---------------------------+
| gdb-multiarch   | <-----------> | Renode                    |
|                 |  "halte an",  |  +---------------------+  |
|                 |  "lies Adr.", |  | emulierter STM32F103|  |
| kennt: Symbole, |  "setze Break-|  |  CPU + Flash + RAM  |  |
| Quelltext, Typen|   point" ...  |  +---------------------+  |
+-----------------+               +---------------------------+
        ^
        | liest zusätzlich die ELF-Datei (Symbole, Debug-Infos)
        v
   build/debug/stm32f103rb-app
```

Merken Sie sich die **Arbeitsteilung**:

- **GDB** weiß, *wie Ihr Programm aussieht* (Funktionsnamen, Variablennamen, Typen, Quelltextzeilen) – das steht in der ELF-Datei.
- **Renode** weiß, *was die CPU gerade tut* (Registerwerte, Speicherinhalte).
- Beide unterhalten sich über **Port 3333**.

> **Hinweis:** Bei echter Hardware übernimmt `OpenOCD` die Rolle von Renode – und lauscht ebenfalls standardmäßig auf Port 3333. Deshalb funktionieren fast alle GDB-Befehle aus dieser Aufgabe auch auf dem echten Board. Starten Sie aber **nie beide gleichzeitig** (Port belegt!).

### Die Speicherkarte des STM32F103RB

Der Cortex-M3 hat einen **einzigen Adressraum** von 4 GiB. Flash, RAM und Peripherie liegen an festen Adressen:

| Bereich | Adressen | Größe | Inhalt |
|---|---|---|---|
| Flash | `0x0800_0000` – `0x0801_FFFF` | 128 KiB | Programmcode, Konstanten, Startwerte |
| SRAM | `0x2000_0000` – `0x2000_4FFF` | 20 KiB | Variablen, Stack, Heap |
| Peripherie | ab `0x4000_0000` | – | UART, GPIO, Timer, … (Register) |

Die Aufteilung innerhalb von Flash und RAM legt das **Linker-Script** (`ld/stm32f103rb.ld`) fest:

```
FLASH 0x0800_0000 ..
+--------------------------------------------------------------+
| .isr_vector | .text/.init/.fini | .rodata | .ARM.extab/exidx|
+--------------------------------------------------------------+
                        |
                        | Startup-Code kopiert die Startwerte von .data
                        v
RAM   0x2000_0000 .. 0x2000_4FFF
+--------------------------------------------------------------+
| .data | .bss | .noinit |      HEAP ->     |     <- STACK     |
+--------------------------------------------------------------+
niedrige Adressen                                 hohe Adressen
```

### Stack und Heap in fünf Minuten

Beide sind Bereiche im RAM, die zur **Laufzeit** wachsen und schrumpfen.

**Stack (Aufrufstapel)**

- Speichert für jeden **Funktionsaufruf** einen eigenen Abschnitt, den **Stack-Frame**: lokale Variablen, gesicherte Register, Rücksprungadresse.
- Funktioniert nach dem Prinzip **LIFO** (Last In – First Out): Die zuletzt aufgerufene Funktion endet zuerst.
- Wird von der CPU selbst verwaltet, über das Register **SP** (Stack Pointer, `r13`).
- Beim Cortex-M **wächst der Stack nach unten**, also zu *kleineren* Adressen („full descending"): `push` verringert zuerst den SP und schreibt dann.
- Der Stack startet am **Ende des RAMs** (`_stack_top`). Den Startwert holt sich die CPU beim Reset aus dem allerersten Wort der Vektortabelle.

**Heap (dynamischer Speicher)**

- Speicher, den Sie **selbst anfordern** (`malloc`, `new`) und wieder freigeben (`free`, `delete`).
- Wächst in diesem Projekt **nach oben** (zu größeren Adressen), also *dem Stack entgegen*.
- Die C-Bibliothek (newlib) fragt über die Funktion `_sbrk` beim System nach mehr Speicher. In diesem Projekt ist `_sbrk` ein einfacher „Bump-Allocator" (siehe `src/syscalls.c` und README).

**Warum ist das auf dem Mikrocontroller gefährlich?**
Stack und Heap wachsen aufeinander zu. Treffen sie sich, überschreiben sie sich gegenseitig – ohne Fehlermeldung. Deshalb gilt in der Embedded-Welt: Heap sparsam benutzen, Rekursion vermeiden, Stack-Verbrauch kennen.

---

## Lernziele

- Renode im Devcontainer starten und bedienen
- Verstehen, wie GDB und Renode zusammenarbeiten (`target remote`, Port 3333)
- Die wichtigsten GDB-Befehle sicher einsetzen und **genau erklären können, was sie tun**
- Die Speichersegmente `.text`, `.rodata`, `.data`, `.bss`, Heap und Stack unterscheiden und im Debugger nachweisen
- Die Aufgaben des **Startup-Codes** (Vektortabelle, `.data` kopieren, `.bss` nullen, `main` aufrufen) nachvollziehen
- Das Konzept des **Stacks** verstehen: Stack Pointer, Stack-Frame, Wachstumsrichtung, Rücksprungadresse, Stack Overflow
- Die Funktionsweise von `_sbrk` und dem Heap beobachten

---

## Teil 1: Bauen und die ELF-Datei untersuchen

### 1.1 Bauen

```sh
cmake --preset debug
cmake --build --preset debug
ls -l build/debug
```

Sie sehen u. a. diese Dateien (die genauen Namen können leicht abweichen):

| Datei | Bedeutung |
|---|---|
| `stm32f103rb-app.elf` | **ELF** = Programm + Symbolnamen + Debug-Informationen. **Diese Datei braucht der Debugger** und Renode. |
| `stm32f103rb-app.hex` / `.bin` | Nur die reinen Bytes für den Flash-Speicher – ohne Namen und Debug-Infos. Für das Flashen auf echte Hardware. |

Zur Vereinfachung legen wir eine Shell-Variable an (Dateiname bitte an `ls` anpassen):

```sh
ELF=build/debug/stm32f103rb-app.elf
file $ELF
```

**Frage 1.1:** Was meldet `file` über die Architektur (ARM? 32 Bit?) Warum kann man die Datei nicht auf dem PC ausführen?

### 1.2 Segmentgrößen und Symbole ansehen

```sh
# Größe der Abschnitte (Sections)
arm-none-eabi-size --format=sysv $ELF

# Alle Symbole, sortiert nach Adresse (-n), mit Größe (-S)
arm-none-eabi-nm -S -n $ELF

# Adressen der Abschnitte: VMA = Adresse zur Laufzeit, LMA = Ladeadresse
arm-none-eabi-objdump -h $ELF
```

Was bedeuten die Spalten bei `objdump -h`?

| Spalte | Bedeutung |
|---|---|
| `VMA` | *Virtual Memory Address*: Wo liegt der Abschnitt, **wenn das Programm läuft**? |
| `LMA` | *Load Memory Address*: Wo liegt der Abschnitt **im Flash-Abbild**? |

**Frage 1.2:** Bei welchem Abschnitt sind VMA und LMA **verschieden**? Warum ist das genau bei diesem Abschnitt so? (Tipp: Was muss der Startup-Code mit ihm tun?)

### 1.3 Die Vektortabelle

Ganz am Anfang des Flash (`0x0800_0000`) liegt die **Vektortabelle** (Abschnitt `.isr_vector`). Die CPU liest sie beim Reset:

| Wort | Adresse | Inhalt |
|---|---|---|
| 0 | `0x0800_0000` | **Initialer Stack Pointer** (Wert für SP) |
| 1 | `0x0800_0004` | **Adresse des Reset-Handlers** (hier startet das Programm) |
| 2 | `0x0800_0008` | NMI-Handler |
| 3 | `0x0800_000C` | HardFault-Handler |
| … | … | weitere Exceptions und Interrupts |

```sh
arm-none-eabi-objdump -s -j .isr_vector $ELF | head
```

Die Ausgabe zeigt Bytes in Hex. Der Cortex-M ist **Little Endian**: Das niederwertigste Byte steht zuerst. Beispiel: Die Bytes `00 50 00 20` ergeben den Wert `0x20005000`.

**Aufgabe:** Lesen Sie Wort 0 und Wort 1 aus der Ausgabe ab und schreiben Sie die Werte (in der korrekten Byte-Reihenfolge) auf.

**Frage 1.3:**
- Wo im RAM liegt der Stack-Start (Wort 0)? Passt das zum Ende des RAMs?
- Wort 1 hat ein **ungerades** letztes Bit (z. B. `...A5`). Das ist kein Fehler: Das Bit 0 bedeutet „Thumb-Modus" (der Cortex-M kennt nur diesen). Die echte Adresse ist der Wert **minus 1**.

---

## Teil 2: Renode starten

### 2.1 Das Start-Script ansehen

Wechseln Sie in Terminal 1 in das **Projekt-Hauptverzeichnis** und sehen Sie sich das Script an:

```sh
cat renode/nucleo.resc
```

Es enthält sinngemäß Folgendes (Ihre Datei kann leicht abweichen):

```
using sysbus
mach create "nucleo"
machine LoadPlatformDescription @platforms/cpus/stm32f103.repl
sysbus LoadELF @build/debug/stm32f103rb-app.elf
machine StartGdbServer 3333
```

| Zeile | Was passiert |
|---|---|
| `using sysbus` | Abkürzung: `sysbus.xyz` kann als `xyz` geschrieben werden. |
| `mach create "nucleo"` | Legt eine neue, leere virtuelle Maschine an. |
| `machine LoadPlatformDescription @...repl` | Baut die Hardware nach: CPU, Flash, RAM, UARTs, … |
| `sysbus LoadELF @...elf` | Schreibt das Programm in den emulierten Flash und setzt Reset-Werte. |
| `machine StartGdbServer 3333` | Öffnet den GDB-Server auf TCP-Port **3333**. |

> **Beobachtung:** Das Script ruft **kein** `start` auf. Die emulierte CPU steht also still, bis GDB sie startet. Das ist genau das, was wir zum Debuggen wollen.

### 2.2 Renode starten

```sh
renode --disable-xwt --console renode/nucleo.resc
```

| Option | Bedeutung |
|---|---|
| `--disable-xwt` | Keine grafische Oberfläche. Im Devcontainer gibt es kein Display. |
| `--console` | Die Renode-Befehlszeile (der **Monitor**) läuft direkt in Ihrem Terminal. |
| `renode/nucleo.resc` | Dieses Script wird sofort ausgeführt. |

Sie sehen Meldungen über das Laden der Plattform und am Ende einen Prompt, der den Namen der Maschine enthält, z. B. `(nucleo)`.

### 2.3 Erste Renode-Befehle (Monitor)

Probieren Sie im Renode-Prompt aus:

```
help
peripherals
sysbus.cpu PC
sysbus.cpu SP
```

| Befehl | Bedeutung |
|---|---|
| `help` | Listet alle verfügbaren Befehle. |
| `peripherals` | Zeigt alle Bausteine der emulierten Maschine als Baum (CPU, `usart1`, `usart2`, `gpioPortA`, …). |
| `sysbus.cpu PC` | Zeigt den aktuellen Wert des Program Counters. |
| `sysbus.cpu SP` | Zeigt den aktuellen Wert des Stack Pointers. |
| `start` / `pause` | CPU starten / anhalten. **Benutzen wir nicht – GDB übernimmt das.** |
| `quit` | Beendet Renode. |

**Frage 2.1:** Welchen Wert haben PC und SP direkt nach dem Start? Vergleichen Sie mit Wort 0 und Wort 1 der Vektortabelle aus Teil 1.3. Was fällt auf? (Tipp: Thumb-Bit!)

### 2.4 Die UART-Ausgabe sichtbar machen (optional, aber empfohlen)

Alle Messungen in dieser Aufgabe machen Sie mit GDB; die UART-Ausgabe brauchen Sie dafür **nicht**. Wenn Sie trotzdem neugierig sind, was die Firmware sendet: Sie sendet Meldungen über **USART2**. Ohne grafische Oberfläche müssen Sie die Ausgabe in eine Datei umleiten – und zwar **bevor** das Programm läuft. Im Renode-Prompt:

```
sysbus.usart2 CreateFileBackend @/tmp/usart2.log true
```

In Terminal 3 können Sie die Ausgabe dann mitlesen:

```sh
tail -f /tmp/usart2.log
```

> Falls Ihre `nucleo.resc` bereits etwas Ähnliches enthält, überspringen Sie diesen Schritt. Falls `usart2` nicht gefunden wird, prüfen Sie die Namen mit `peripherals`.

---

## Teil 3: GDB verbinden und die wichtigsten Befehle kennenlernen

### 3.1 GDB starten und verbinden

Terminal 2 (Projekt-Hauptverzeichnis):

```sh
gdb-multiarch build/debug/stm32f103rb-app.elf
```

Das Argument sorgt dafür, dass GDB die **Symbole** der ELF-Datei einliest (Namen, Typen, Quelltextzeilen). Die Architektur (ARM) erkennt `gdb-multiarch` dabei selbst aus der ELF-Datei. Mit dem Target hat GDB noch keine Verbindung. Diese stellen Sie jetzt her:

```
(gdb) target remote localhost:3333
```

| Befehl | Was genau passiert |
|---|---|
| `target remote localhost:3333` | GDB öffnet eine TCP-Verbindung zu Port 3333 (Renode) und spricht ab jetzt das Remote Serial Protocol. GDB fragt die Registerwerte ab und zeigt, wo die CPU steht (Reset-Handler). |

> **Fehlermeldung `Connection refused`?** Läuft Renode (Terminal 1)? Hat das Script den GDB-Server gestartet? Ist die Portnummer richtig?

Mit diesen Einstellungen wird die Arbeit angenehmer:

```
(gdb) set pagination off
(gdb) set print pretty on
(gdb) set confirm off
```

| Befehl | Wirkung |
|---|---|
| `set pagination off` | Lange Ausgaben werden nicht seitenweise angehalten („--Mehr--"). |
| `set print pretty on` | Structs werden mehrzeilig, lesbar ausgegeben. |
| `set confirm off` | GDB fragt nicht mehr bei jeder Kleinigkeit nach. |

> **Tipp:** Schreiben Sie diese Zeilen in eine Datei `.gdbinit` im Projektverzeichnis (ggf. müssen Sie GDB mit `-x .gdbinit` starten oder `add-auto-load-safe-path` setzen). So sparen Sie sich das Tippen.

### 3.2 Der Zustand direkt nach dem Reset

```
(gdb) info registers
(gdb) info registers sp pc
(gdb) info symbol $pc
(gdb) x/2wx 0x08000000
```

| Befehl | Was genau passiert |
|---|---|
| `info registers` | Gibt alle CPU-Register aus: `r0`–`r12` (Allzweck), `sp` (`r13`), `lr` (`r14`, Link Register = Rücksprungadresse), `pc` (`r15`, Program Counter), `xpsr` (Statusregister). Kurzform: `i r`. |
| `info registers sp pc` | Wie oben, aber nur die genannten Register. |
| `info symbol $pc` | Fragt: „Zu welchem Symbol gehört die Adresse im PC?" Antwort z. B. `Reset_Handler in section .text`. `$pc` ist die GDB-Schreibweise für den Wert des Registers. |
| `x/2wx 0x08000000` | **E**x­amine memory: Zeigt 2 Wörter (`w` = 4 Byte) in Hex (`x`) ab Adresse `0x08000000`. Das sind die ersten beiden Einträge der Vektortabelle. |

Der Befehl `x` ist das Schweizer Taschenmesser zum Speicherlesen. Seine Form ist `x/<Anzahl><Format><Einheit> <Adresse>`:

| Teil | Mögliche Werte |
|---|---|
| Anzahl | beliebige Zahl, z. B. `8` |
| Format | `x` hex, `d` dezimal mit Vorzeichen, `u` dezimal ohne Vorzeichen, `t` binär, `c` Zeichen, `s` String, `i` **Instruktion** (Disassembly), `a` Adresse mit Symbolname |
| Einheit | `b` Byte, `h` Halbwort (2 Byte), `w` Wort (4 Byte), `g` 8 Byte |

Beispiele: `x/16xb &g_bss_buffer` (16 Bytes hex), `x/4i $pc` (nächste 4 Instruktionen), `x/s kRodataBanner` (Text).

**Frage 3.1:** Stimmen `sp` und `pc` mit den Werten aus der Vektortabelle überein? Warum steht in `pc` die Adresse *ohne* gesetztes Thumb-Bit?

### 3.3 Breakpoints und Ablaufsteuerung

Ein **Breakpoint** ist ein Haltepunkt: Die CPU läuft, bis sie an dieser Stelle ankommt, und hält dann an.

```
(gdb) break main
(gdb) info breakpoints
(gdb) continue
```

| Befehl | Kurzform | Was genau passiert |
|---|---|---|
| `break main` | `b main` | GDB schlägt die Adresse von `main` in den Symbolen nach und bittet Renode, dort einen Breakpoint zu setzen. Weitere Formen: `break datei.cpp:42` (Zeile), `break *0x08000100` (genaue Adresse). |
| `tbreak main` | `tb main` | „Temporary Breakpoint": wie `break`, löscht sich aber nach dem ersten Treffer selbst. |
| `info breakpoints` | `i b` | Listet alle Breakpoints mit Nummer, Adresse und Trefferzahl. |
| `delete 1` | `d 1` | Löscht Breakpoint Nr. 1 (ohne Nummer: alle). |
| `continue` | `c` | **Startet die CPU** und lässt sie laufen, bis ein Breakpoint trifft. Mit `Ctrl+C` können Sie die laufende CPU jederzeit anhalten. |

Wenn die CPU bei `main` hält, sehen Sie die Quelltextzeile. Nun können Sie schrittweise vorgehen:

```
(gdb) list
(gdb) next
(gdb) step
(gdb) finish
(gdb) stepi
(gdb) x/i $pc
```

| Befehl | Kurzform | Was genau passiert |
|---|---|---|
| `list` | `l` | Zeigt Quelltext um die aktuelle Stelle. |
| `next` | `n` | Führt **eine Quelltextzeile** aus. Funktionsaufrufe werden **übersprungen** (als Ganzes ausgeführt). |
| `step` | `s` | Wie `next`, springt aber **in** aufgerufene Funktionen hinein (wenn Quelltext vorhanden ist). |
| `finish` | `fin` | Läuft, bis die **aktuelle Funktion zurückkehrt**, und zeigt den Rückgabewert. |
| `stepi` | `si` | Führt **genau eine Maschinenanweisung** aus (nicht eine Quelltextzeile!). |
| `nexti` | `ni` | Wie `stepi`, überspringt aber Funktionsaufrufe (`bl`). |
| `x/i $pc` | – | Zeigt die Instruktion, die als Nächstes ausgeführt wird. |
| `until` / `advance <ziel>` | `u` | Läuft bis zu einer Stelle; nützlich, um eine Schleife zu verlassen. |

### 3.4 Variablen und Speicher untersuchen

```
(gdb) print g_data_counter
(gdb) print/x g_data_counter
(gdb) print &g_data_counter
(gdb) info locals
(gdb) display g_data_counter
(gdb) set var g_data_counter = 5
(gdb) ptype g_bss_buffer
```

| Befehl | Kurzform | Was genau passiert |
|---|---|---|
| `print <ausdruck>` | `p` | Wertet einen C-Ausdruck aus und gibt das Ergebnis aus. Dabei liest GDB den Speicher des Targets. |
| `print/x` | `p/x` | Ausgabe in Hex. Weitere Formate: `/d` dezimal, `/t` binär, `/c` Zeichen. |
| `print &var` | – | Gibt die **Adresse** der Variablen aus. |
| `print *ptr` / `print arr[2]` / `print *arr@4` | – | Dereferenzieren, Indexzugriff, „4 Elemente ab arr". |
| `info locals` / `info args` | – | Zeigt alle lokalen Variablen bzw. Funktionsargumente der aktuellen Funktion. |
| `display <ausdruck>` | – | Gibt den Ausdruck **nach jedem Halt automatisch** aus (z. B. `display/x $sp` oder `display/i $pc`). `undisplay` entfernt ihn wieder. |
| `set var <variable> = <wert>` | – | **Schreibt** in den Speicher des Targets. Praktisch zum Experimentieren. |
| `ptype <name>` | – | Zeigt den vollständigen Typ (Struct-Aufbau, Array-Länge). |
| `watch <variable>` | – | **Watchpoint**: Die CPU hält an, sobald sich der Wert der Variablen ändert. |

> **Hinweis:** Die Variablennamen `g_data_counter`, `g_bss_counter`, `g_bss_buffer` und `kRodataBanner` stammen aus der README des Projekts. Falls sie in Ihrer Version anders heißen, finden Sie die richtigen Namen mit `arm-none-eabi-nm -n $ELF` oder in `src/main.cpp`.

### 3.5 Der Stack in GDB (Vorgeschmack)

```
(gdb) backtrace
(gdb) info frame
(gdb) up
(gdb) down
```

| Befehl | Kurzform | Was genau passiert |
|---|---|---|
| `backtrace` | `bt` | Zeigt die Kette der **aktiven Funktionsaufrufe** (wer hat wen aufgerufen?), jeweils eine Zeile pro Stack-Frame. |
| `info frame` | `i f` | Zeigt Details zum aktuellen Frame: Adresse des Frames, gespeicherter PC (Rücksprungadresse), gesicherte Register. |
| `up` / `down` | – | Wechselt zum aufrufenden bzw. aufgerufenen Frame, damit Sie dort lokale Variablen ansehen können. |
| `frame <n>` | `f <n>` | Wechselt direkt zu Frame Nr. n. |

### 3.6 Renode-Befehle aus GDB heraus

Mit `monitor` schickt GDB einen Befehl **an den Monitor von Renode** weiter, ohne dass Sie das Terminal wechseln müssen:

```
(gdb) monitor sysbus.cpu PC
(gdb) monitor machine Reset
(gdb) flushregs
```

| Befehl | Was genau passiert |
|---|---|
| `monitor <befehl>` | Der Rest der Zeile wird an Renode geschickt und dort als Monitor-Befehl ausgeführt. Die Antwort erscheint in GDB. (Bei OpenOCD gilt dasselbe Prinzip, aber mit anderen Befehlen wie `monitor reset halt`.) |
| `monitor machine Reset` | Setzt die emulierte Maschine zurück: Die CPU beginnt wieder beim Reset-Handler, die RAM-Inhalte werden dabei **nicht** zwingend gelöscht. |
| `flushregs` | Verwirft den Register-Zwischenspeicher von GDB, sodass die Werte nach einem Reset frisch gelesen werden. |

### 3.7 GDB beenden

```
(gdb) quit
```

Renode läuft weiter. Sie können sich mit `target remote localhost:3333` erneut verbinden. Zum vollständigen Beenden wechseln Sie in Terminal 1 und tippen `quit`.

**Aufgabe 3:** Setzen Sie einen Breakpoint auf `main`, lassen Sie das Programm laufen (`continue`) und führen Sie mit `next` einige Zeilen aus. Lesen Sie mit `print` mindestens zwei Variablen aus. Optional (nur wenn Sie Teil 2.4 gemacht haben): Welche Meldungen erscheinen in `/tmp/usart2.log`?

---

## Teil 4: Wo liegt was? Die Speichersegmente im Debugger nachweisen

Jetzt prüfen Sie, dass die Aussagen aus der README tatsächlich stimmen.

### 4.1 Der Befehl `info symbol`

```
(gdb) info symbol &g_data_counter
(gdb) info symbol &g_bss_counter
(gdb) info symbol &g_bss_buffer
(gdb) info symbol kRodataBanner
(gdb) info address main
(gdb) info files
```

| Befehl | Was genau passiert |
|---|---|
| `info symbol <adresse>` | Sagt, in welchem **Abschnitt** und bei welchem Symbol die Adresse liegt, z. B. `g_data_counter in section .data`. |
| `info address <symbol>` | Zeigt, wo ein Symbol gespeichert ist. |
| `info files` | Listet alle Abschnitte der ELF-Datei mit ihren Adressbereichen. |

> **Achtung beim Zeiger auf Literale:** `kRodataBanner` ist vermutlich ein Zeiger oder Array auf einen Text. Wenn es ein Zeiger ist, liegt der **Zeiger** in `.data`/`.rodata`, der **Text** aber in `.rodata`. Prüfen Sie beides mit `info symbol kRodataBanner` und `info symbol &kRodataBanner`.

### 4.2 Tabelle ausfüllen

Erstellen Sie diese Tabelle (in Ihrer Abgabe) und füllen Sie sie mit den echten Werten:

| Variable | Adresse | Flash oder RAM? | Segment |
|---|---|---|---|
| `g_data_counter` | | | |
| `g_bss_counter` | | | |
| `g_bss_buffer` | | | |
| String aus `kRodataBanner` | | | |
| Funktion `main` | | | |

**Frage 4.1:** An welcher Adresse beginnt `.data`, und wo endet `.bss`? (Tipp: Die Linker-Symbole stehen in `nm`; siehe Teil 5.2.)

**Frage 4.2:** Warum haben `.data` und `.bss` Adressen im RAM, `.rodata` und `.text` aber Adressen im Flash?

---

## Teil 5: Der Startup-Code – was passiert vor `main`?

Auf dem PC startet `main` „einfach so". Das stimmt nicht: Auch dort läuft vorher Code (der Lader und die C-Laufzeit). Auf dem Mikrocontroller müssen wir ihn **selbst bereitstellen**. In diesem Projekt steht er in `src/startup_stm32f103rb.c`.

### 5.1 Der Ablauf nach dem Einschalten

```
1. Hardware (nicht Software!):
     SP <- Wort 0 der Vektortabelle        (Stack ist sofort benutzbar)
     PC <- Wort 1 der Vektortabelle        (= Reset-Handler)

2. Reset-Handler (Software, Startup-Code):
     a) .data kopieren:  Flash (LMA) --> RAM (VMA)
     b) .bss nullen:     RAM-Bereich mit 0 füllen
     c) C/C++-Initialisierung (z. B. Konstruktoren globaler Objekte)
     d) main() aufrufen
     e) falls main zurückkehrt: Endlosschleife

3. main()
```

**Warum muss `.data` kopiert werden?** Eine Variable wie `int g_data_counter = 42;` braucht zur Laufzeit RAM (sie ist veränderbar). Aber nach dem Einschalten ist das RAM **leer/zufällig**. Der Startwert `42` muss also irgendwo dauerhaft gespeichert sein – im Flash. Der Startup-Code kopiert ihn beim Reset ins RAM.

**Warum muss `.bss` genullt werden?** C/C++ garantiert, dass globale Variablen ohne Initialwert `0` sind. Damit der Flash nicht mit Nullen gefüllt werden muss (Platzverschwendung), merkt sich der Linker nur *Anfang und Ende* des Bereichs; der Startup-Code füllt ihn mit Nullen.

### 5.2 Die Linker-Symbole finden

Im Linker-Script stehen Symbole, die Anfang und Ende der Bereiche markieren. Der Startup-Code benutzt sie. Suchen Sie sie:

```sh
arm-none-eabi-nm -n $ELF | grep -i -E "sidata|sdata|edata|sbss|ebss|stack|heap"
```

Üblich sind Namen wie `_sidata` (Startwerte im Flash), `_sdata`/`_edata` (Anfang/Ende `.data` im RAM), `_sbss`/`_ebss` (Anfang/Ende `.bss`), `_stack_top`, `_heap_start`, `_heap_end`. Die genauen Namen stehen in `ld/stm32f103rb.ld`.

> **Wichtige Besonderheit bei Linker-Symbolen:** Ein Linker-Symbol ist **keine Variable mit Inhalt**, sondern nur ein **Name für eine Adresse**. In GDB bedeutet deshalb `print &_sdata` „die Adresse", während `print _sdata` unsinnigen Inhalt liefern würde. Schreiben Sie immer das `&`!

```
(gdb) print/x &_sdata
(gdb) print/x &_edata
(gdb) print/x &_sbss
(gdb) print/x &_ebss
```

**Aufgabe 5.2:** Berechnen Sie mit diesen Werten die Größe von `.data` und `.bss` in Bytes. Vergleichen Sie mit der Ausgabe von `arm-none-eabi-size --format=sysv $ELF`.

### 5.3 Den Reset-Handler im Quelltext und in Maschinencode lesen

Öffnen Sie `src/startup_stm32f103rb.c` und suchen Sie die Funktion für den Reset. Beantworten Sie:

- Wie heißt sie?
- Welche Schleifen enthält sie, und welche Linker-Symbole benutzen sie?
- Was wird zuletzt aufgerufen?

Dann in GDB:

```
(gdb) disassemble Reset_Handler
```

| Befehl | Was genau passiert |
|---|---|
| `disassemble <funktion>` | Zeigt den Maschinencode (Assembler) der Funktion. Der Pfeil `=>` markiert die aktuelle Instruktion. Mit `disassemble /s` wird der Quelltext zwischengeschoben. Kurzform: `disas`. |

(Falls die Funktion anders heißt, benutzen Sie den Namen, den `info symbol $pc` direkt nach dem Verbinden meldet.)

### 5.4 Experiment: Beobachten, wie `.data` und `.bss` initialisiert werden

Wir halten die CPU **ganz am Anfang** an – bevor der Startup-Code seine Arbeit getan hat – und beobachten ihn dabei.

1. Setzen Sie die Maschine auf den Anfang zurück:

   ```
   (gdb) monitor machine Reset
   (gdb) flushregs
   (gdb) info registers sp pc
   ```

2. **Vorher-Zustand** festhalten und den RAM absichtlich „vergiften":

   ```
   (gdb) x/wx &g_data_counter
   (gdb) x/wx &g_bss_counter
   (gdb) set var g_bss_counter = 0xDEADBEEF
   (gdb) x/wx &g_bss_counter
   ```

   Die Variable `g_bss_counter` enthält jetzt künstlichen Müll – so wie ein frisch eingeschaltetes RAM.

3. **Wer schreibt in `g_data_counter`?** Setzen Sie einen Watchpoint und lassen Sie die CPU laufen:

   ```
   (gdb) watch g_data_counter
   (gdb) continue
   ```

   Die CPU hält an, sobald jemand den Wert ändert. Schauen Sie nach, wo:

   ```
   (gdb) info symbol $pc
   (gdb) x/i $pc
   (gdb) bt
   (gdb) print g_data_counter
   ```

   > **Falls Ihre Renode-Version keine Watchpoints unterstützt:** Setzen Sie stattdessen `tbreak main`, lassen Sie das Programm mit `continue` bis dorthin laufen und vergleichen Sie die Werte vorher/nachher.

4. Entfernen Sie den Watchpoint (`delete`) und laufen Sie bis `main`:

   ```
   (gdb) delete
   (gdb) tbreak main
   (gdb) continue
   (gdb) print g_data_counter
   (gdb) print g_bss_counter
   ```

**Fragen:**

- **5.1** Welchen Wert hatte `g_data_counter` direkt nach dem Reset und welchen bei `main`? Woher stammt der Startwert?
- **5.2** Welchen Wert hat `g_bss_counter` jetzt, obwohl Sie `0xDEADBEEF` hineingeschrieben haben? Wer hat ihn genullt?
- **5.3** In welcher Funktion hielt der Watchpoint an? Passt das zu Ihrer Erwartung?
- **5.4** Wie viele Instruktionen hat der Startup-Code etwa gebraucht, bis `main` erreicht war? (Tipp: Nicht zählen, sondern anhand des Quelltextes abschätzen.)

---

## Teil 6: Der Stack

### 6.1 Ihr Werkzeug: eine Funktion mit sichtbaren Spuren

Damit Sie den Stack **nachmessen** können, bauen Sie in `src/main.cpp` eine kleine Hilfsfunktion ein. Sie ruft sich selbst auf (Rekursion) und merkt sich in jeder Ebene die Adresse einer lokalen Variablen.

Oben in `main.cpp` (bei den anderen Includes und globalen Variablen):

```cpp
#include <cstdint>

// Hier merkt sich jede Rekursionsebene die Adresse ihrer lokalen Variable.
// Globale Variable ohne Initialwert -> landet in .bss
volatile std::uintptr_t g_stack_probe[6];

// Ergebnis, damit der Compiler den Aufruf nicht wegoptimiert
volatile int g_stack_result;

// noinline: Der Compiler soll die Funktion wirklich aufrufen (kein Einsetzen)
__attribute__((noinline)) int stackDemo(int depth, int maxDepth)
{
    // volatile: Die Variable muss wirklich im Stack-Frame liegen
    volatile int local = depth * 10;

    if (depth < 6) {
        g_stack_probe[depth] = reinterpret_cast<std::uintptr_t>(&local);
    }

    if (depth < maxDepth) {
        return stackDemo(depth + 1, maxDepth) + local;
    }
    return local;
}
```

In `main()` vor der Endlosschleife (Blink-Schleife) einfügen:

```cpp
g_stack_result = stackDemo(0, 5);
```

Neu bauen:

```sh
cmake --build --preset debug
```

> **Wichtig:** Renode muss die **neue** ELF-Datei laden. Beenden Sie Renode (`quit`) und starten Sie es neu (Teil 2.2). Danach in GDB neu verbinden. Wenn Sie GDB offen lassen, mit `file build/debug/stm32f103rb-app.elf` die Symbole neu einlesen.

### 6.2 Den Stack beim Aufruf beobachten

```
(gdb) target remote localhost:3333
(gdb) break stackDemo
(gdb) continue
(gdb) bt
(gdb) info frame
(gdb) print $sp
(gdb) print &local
(gdb) print depth
```

Lassen Sie mit weiteren `continue`-Befehlen die Rekursion tiefer gehen. Bei jedem Halt:

```
(gdb) print depth
(gdb) print/x $sp
(gdb) bt
```

> Mit `display/x $sp` und `display depth` sehen Sie die Werte nach jedem Halt automatisch.

**Aufgabe 6.2:** Lassen Sie die Rekursion bis zum Ende laufen (alle sechs Ebenen) und lesen Sie danach das Feld aus:

```
(gdb) delete
(gdb) finish        # oder: continue bis zum nächsten sinnvollen Halt
(gdb) print/x g_stack_probe
(gdb) x/6wx g_stack_probe
```

Tragen Sie die Werte in diese Tabelle ein:

| Ebene (`depth`) | Adresse von `local` | Differenz zur vorherigen Ebene |
|---|---|---|
| 0 | | – |
| 1 | | |
| 2 | | |
| 3 | | |
| 4 | | |
| 5 | | |

**Fragen:**

- **6.1** Werden die Adressen mit jeder Ebene **größer oder kleiner**? Was sagt das über die Wachstumsrichtung des Stacks?
- **6.2** Wie groß ist ein Stack-Frame von `stackDemo` (die Differenz)? Ist der Wert durch 8 teilbar? (Hintergrund: Die ARM-Aufrufkonvention verlangt an Funktionsgrenzen einen auf **8 Byte ausgerichteten** Stack.)
- **6.3** Wie viele Bytes Stack verbrauchen sechs Rekursionsebenen? Wie viele Ebenen würden in die gesamten 20 KiB RAM passen (theoretisch, ohne `.data`/`.bss`)?

### 6.3 Einen Stack-Frame zerlegen

Halten Sie in einer tieferen Rekursionsebene an (z. B. `depth = 2`) und sehen Sie sich den Maschinencode des **Funktionsanfangs (Prolog)** an:

```
(gdb) disassemble stackDemo
```

Typischerweise (je nach Compiler-Version etwas unterschiedlich) finden Sie am Anfang:

```
push   {r7, lr}        ; sichert r7 und die Rücksprungadresse (lr) auf dem Stack
sub    sp, #16         ; reserviert Platz für lokale Variablen
add    r7, sp, #0      ; r7 wird zum "Frame Pointer"
...
add    sp, #16         ; Platz wieder freigeben (Epilog)
pop    {r7, pc}        ; r7 zurück, und PC <- gesicherte Rücksprungadresse = Rücksprung!
```

| Instruktion | Was genau passiert |
|---|---|
| `push {r7, lr}` | SP wird um 8 verringert, danach werden `r7` (bei `[SP]`) und `lr` (bei `[SP+4]`) gespeichert. |
| `sub sp, #N` | SP wird um N Bytes verringert: Der Platz für lokale Variablen ist reserviert („der Stack ist gewachsen"). |
| `bl <funktion>` | **B**ranch with **L**ink: Springt zur Funktion und speichert vorher die Rücksprungadresse in `lr`. |
| `pop {r7, pc}` | Holt `r7` zurück und lädt den **gespeicherten LR-Wert direkt in den PC**: Das ist der Rücksprung. |

So sieht der Stack aus, während `stackDemo` läuft:

```
hohe Adressen
   |  ...  Frame des Aufrufers (z. B. main)              |
   +-----------------------------------------------------+
   |  gesicherter lr  (Rücksprungadresse)                |  <- SP + N + 4
   |  gesicherter r7  (alter Frame Pointer)              |  <- SP + N
   |  lokale Variablen (z. B. "local") und Padding       |
   |  ...                                                |  <- SP  (zeigt auf das zuletzt belegte Wort)
   +-----------------------------------------------------+
niedrige Adressen         (hier wächst der Stack hin)
```

**Aufgaben:**

1. Bestimmen Sie in einer Rekursionsebene `sp` und die Adresse von `local`. Wo im Frame liegt `local`?
2. Lesen Sie den Frame direkt aus dem RAM:

   ```
   (gdb) x/8wx $sp
   (gdb) info frame
   ```

   Suchen Sie in den ausgegebenen Wörtern den **gespeicherten `lr`** (ein Wert, der wie eine Flash-Adresse `0x0800....` aussieht, mit gesetztem Thumb-Bit). Prüfen Sie mit `info symbol <adresse ohne Thumb-Bit>`, in welche Funktion er zurückführt.
3. Vergleichen Sie diese Adresse mit der Ausgabe von `info frame` („Saved registers" und „caller of") und von `bt`.
4. Überlegen Sie: Was passiert, wenn jemand die gespeicherte Rücksprungadresse überschreibt (z. B. durch einen Pufferüberlauf in einer lokalen Variable)?

### 6.4 Stack-Verbrauch bis `main`

Der Stack beginnt bei `_stack_top`. Wie viel ist schon belegt, wenn Sie in `main` ankommen?

```
(gdb) tbreak main
(gdb) continue
(gdb) print/x $sp
(gdb) print/x &_stack_top
```

**Frage 6.4:** Wie viele Bytes liegen zwischen `_stack_top` und dem SP in `main`? Wer hat dort Platz belegt (Tipp: `bt` in `main`, Startup-Code)?

### 6.5 Experiment: Stack Overflow

> Dieses Experiment zeigt, **warum Mikrocontroller-Programmierer Angst vor Rekursion haben**.

1. Ändern Sie den Aufruf in `main()` auf eine sehr große Tiefe:

   ```cpp
   g_stack_result = stackDemo(0, 2000);
   ```

2. Neu bauen, Renode neu starten, GDB neu verbinden.
3. Setzen Sie vorher einen Watchpoint auf eine **eigentlich unbeteiligte** Variable und beobachten Sie, wer sie überschreibt:

   ```
   (gdb) watch g_data_counter
   (gdb) continue
   ```

4. Wenn der Watchpoint anhält: Wo steht die CPU (`bt`, `info symbol $pc`, `print $sp`)? Wer hat `g_data_counter` verändert, obwohl der Quelltext das nirgends tut?

**Fragen:**

- **6.5** Was ist passiert? In welche Bereiche ist der Stack hineingewachsen (Heap? `.bss`? `.data`)?
- **6.6** Warum bekommt man auf dem Mikrocontroller keine Fehlermeldung wie „Segmentation fault"?
- **6.7** Nennen Sie drei Möglichkeiten, Stack Overflows zu vermeiden oder früh zu erkennen (z. B. Rekursion vermeiden, Stack-Größe im Linker-Script mit `_Min_Stack_Size` festlegen, Stack-Painting, Memory Protection Unit).

Stellen Sie danach den Wert in `main()` wieder auf `5` zurück.

---

## Teil 7: Der Heap und `_sbrk`

### 7.1 Eine Speicheranforderung auslösen

Fügen Sie in `main.cpp` einen kleinen `malloc`-Aufruf ein (bei den Includes `#include <cstdlib>`):

```cpp
volatile void* g_heap_ptr;   // landet in .bss

// in main(), vor der Endlosschleife:
g_heap_ptr = std::malloc(32);
```

(Falls im Projekt bereits die Demo-Allokation per `_sbrk(64)` existiert, können Sie diese ebenfalls beobachten.)

### 7.2 `_sbrk` im Debugger beobachten

```
(gdb) break _sbrk
(gdb) continue
(gdb) info registers r0
(gdb) bt
(gdb) finish
(gdb) print/x &_heap_start
(gdb) print/x &_heap_end
(gdb) print/x g_heap_ptr
```

| Beobachtung | Erklärung |
|---|---|
| `r0` beim Eintritt in `_sbrk` | Nach der ARM-Aufrufkonvention steht das **erste Argument in `r0`**: also `increment`, die Zahl der angeforderten Bytes. |
| `bt` | Zeigt, dass `_sbrk` von `malloc` (aus der newlib) aufgerufen wurde – nicht von Ihrem Code. |
| `finish` | Läuft bis zum Rücksprung und zeigt den **Rückgabewert** (Rückgabewerte stehen in `r0`): die **alte** Heap-Grenze. |
| `&_heap_start`, `&_heap_end` | Beginn und Obergrenze des Heaps. |

**Fragen:**

- **7.1** Wie viele Bytes fordert `_sbrk` an – genau 32, oder mehr? Warum fragt `malloc` die Hardware vermutlich in größeren Blöcken, als Sie wollten?
- **7.2** Liegt `g_heap_ptr` in der Nähe von `_heap_start`? Liegt der Zeiger **im Heap**, die Variable `g_heap_ptr` **selbst** aber in `.bss`? Belegen Sie das mit `info symbol &g_heap_ptr`.
- **7.3** Wie viel Heap steht insgesamt zur Verfügung (`_heap_end - _heap_start`)? Das ist zugleich der Abstand zwischen dem Ende der festen Segmente und dem reservierten Stack.
- **7.4** Der Heap wächst nach **oben**, der Stack nach **unten**. Was passiert, wenn sie sich treffen? Wie versucht `_sbrk` das zu verhindern (README: Grenze `_heap_end`)?

### 7.3 Unterschied Stack und Heap – Tabelle ausfüllen

| Eigenschaft | Stack | Heap |
|---|---|---|
| Wer verwaltet den Speicher? | | |
| Wachstumsrichtung auf diesem Chip | | |
| Wann wird Speicher belegt? | | |
| Wann wird er freigegeben? | | |
| Typisches Risiko | | |

---

## Teil 8: Das Gesamtbild – Speicherkarte selbst zeichnen

Zeichnen Sie das RAM des STM32F103RB (Adressen `0x2000_0000` – `0x2000_4FFF`) mit den **echten Adressen aus Ihrer Messung**. Übertragen Sie dazu die Werte aus den Teilen 4–7:

```
0x2000_5000  +----------------------------+  <- _stack_top (Start des Stacks)
             |  Stack (wächst nach ____)  |
             |             |              |
             |             v              |
             |  ~~~~ freier Platz ~~~~~~  |
             |             ^              |
             |             |              |
             |  Heap (wächst nach ____)   |
_heap_start  +----------------------------+  = ____________
             |  .noinit                   |
_ebss        +----------------------------+  = ____________
             |  .bss                      |
_sbss        +----------------------------+  = ____________
             |  .data                     |
0x2000_0000  +----------------------------+
```

Zeichnen Sie außerdem den **Flash** (Vektortabelle, `.text`, `.rodata`, Startwerte von `.data`) mit den Adressen aus `objdump -h`.

---

## Abgabe / kurzes Fazit

Dokumentieren Sie in wenigen Sätzen:

1. Wie arbeiten GDB und Renode zusammen? Was macht `target remote localhost:3333`?
2. Was tut der Startup-Code, bevor `main` läuft? Warum ist das nötig?
3. Welche Ihrer Variablen liegen in `.data`, `.bss`, `.rodata`, auf dem Stack oder im Heap? (Tabelle aus Teil 4 und Teil 7.)
4. Wie ist ein Stack-Frame aufgebaut, und warum wächst der Stack bei diesem Prozessor nach unten?
5. Was ist ein Stack Overflow, und warum bleibt er auf dem Mikrocontroller oft unbemerkt?
6. Erklären Sie jeweils in einem Satz die Befehle `break`, `continue`, `next`, `step`, `finish`, `x`, `print`, `info registers`, `bt`, `watch`.

Committen Sie Ihre Änderungen an `main.cpp` sowie Ihr Fazit (z. B. als `FAZIT.md`).

---

## Extended (Bonus): Lernen durch gezielte Experimente

### Bonus 1: Globale Objekte und Konstruktoren – Startup-Code Teil 2

Globale Objekte in C++ mit Konstruktor werden **vor `main`** konstruiert. Das erledigt der Startup-Code (Schritt 2c in Teil 5.1).

1. Fügen Sie in `main.cpp` ein:

   ```cpp
   volatile int g_ctor_ran;     // .bss, startet mit 0

   struct Probe {
       Probe() { g_ctor_ran = 1; }
   };

   Probe g_probe;               // globales Objekt
   ```

2. Setzen Sie `tbreak main` und prüfen Sie `print g_ctor_ran` bei `main`.
3. Setzen Sie stattdessen einen Breakpoint auf `Probe::Probe` und sehen Sie sich mit `bt` an, **wer** den Konstruktor aufruft (Tipp: `__libc_init_array` oder ähnlich).

**Fragen:** Wo steht die Liste der aufzurufenden Konstruktoren (Abschnitte `.init_array` / `.preinit_array`)? Was würde passieren, wenn der Startup-Code diesen Schritt vergäße?

### Bonus 2: Ein HardFault und der Exception-Stack-Frame

Wenn ein Interrupt oder eine Exception auftritt, legt die **Hardware selbst** acht Wörter auf den Stack:

```
SP+0x00  r0
SP+0x04  r1
SP+0x08  r2
SP+0x0C  r3
SP+0x10  r12
SP+0x14  lr   (Rücksprungadresse vor der Exception)
SP+0x18  pc   (die Instruktion, bei der es passiert ist)
SP+0x1C  xpsr
```

1. Fügen Sie in `main()` einmalig `__builtin_trap();` ein (erzeugt eine ungültige Instruktion).
2. Finden Sie in `startup_stm32f103rb.c` oder mit `nm` den Namen des HardFault-Handlers und setzen Sie dort einen Breakpoint.
3. Lassen Sie das Programm laufen. Halt der Breakpoint? Dann:

   ```
   (gdb) bt
   (gdb) x/8wx $sp
   (gdb) info registers lr
   (gdb) info symbol <gespeicherter pc>
   ```

4. Der Wert in `lr` ist hier **keine** Funktionsadresse, sondern ein Sonderwert `EXC_RETURN` (z. B. `0xFFFFFFF9`). Recherchieren Sie, was er bedeutet.

> Falls bei Ihrer Renode-Konfiguration kein HardFault auftritt (Renode behandelt manche Fehler nur mit einer Log-Meldung), prüfen Sie die Ausgabe im Renode-Terminal.

### Bonus 3: Debug- gegen Release-Build

```sh
cmake --preset release
cmake --build --preset release
```

Laden Sie die Release-ELF in Renode (Pfad in `nucleo.resc` anpassen oder kopieren) und wiederholen Sie Teil 6.2.

- Wie verändert sich die Frame-Größe von `stackDemo`?
- Warum ist `finish`/`next` im optimierten Code manchmal verwirrend („optimized out", springende Zeilennummern)?
- Wird die Rekursion eventuell in eine Schleife umgewandelt (Tail-Call-Optimierung)? Wie sieht das im `disassemble` aus?

### Bonus 4: GDB-Skript statt Handarbeit

Legen Sie eine Datei `gdb/stack.gdb` an, die Ihre Messung automatisiert:

```gdb
set pagination off
target remote localhost:3333
break stackDemo
commands
  silent
  printf "depth=%d sp=0x%08x\n", depth, (unsigned)$sp
  continue
end
continue
```

Starten Sie mit:

```sh
gdb-multiarch -batch -x gdb/stack.gdb build/debug/stm32f103rb-app.elf
```

Was bewirkt `commands ... end`? (Hinweis: Eine Liste von GDB-Befehlen, die automatisch bei jedem Treffer des Breakpoints ausgeführt wird; `silent` unterdrückt die übliche Halt-Meldung.)

### Bonus 5: Stack-Painting – Stack-Verbrauch messen

Ein klassisches Verfahren: Der Startup-Code (oder Sie selbst in `main`) füllt den ungenutzten Stack-Bereich mit einem Muster (z. B. `0xAAAAAAAA`). Später sucht man die Stelle, bis zu der das Muster **noch unverändert** ist – das ist der „Stack-Wasserstand" (High Water Mark).

1. Füllen Sie mit GDB per Hand den Bereich unterhalb von `$sp` bis `_heap_end`:

   ```
   (gdb) set var $p = (unsigned*)$sp - 1
   (gdb) while $p >= (unsigned*)&_heap_end
     > set var *$p = 0xAAAAAAAA
     > set var $p = $p - 1
     > end
   ```

2. Lassen Sie `stackDemo(0, 5)` laufen und suchen Sie mit `x/64wx` die Grenze zwischen Muster und Veränderungen.
3. Wie viele Bytes hat der Stack **maximal** benutzt?

---

## Anhang A: GDB-Spickzettel

| Aufgabe | Befehl |
|---|---|
| Verbinden | `target remote localhost:3333` |
| Breakpoint | `break main`, `break datei.cpp:42`, `tbreak`, `delete` |
| Starten / weiter | `continue` (`c`), `Ctrl+C` zum Anhalten |
| Zeilenweise | `next` (`n`), `step` (`s`), `finish` |
| Instruktionsweise | `stepi` (`si`), `nexti` (`ni`), `x/i $pc` |
| Variablen | `print`, `print/x`, `display`, `set var`, `info locals` |
| Speicher | `x/8wx <adresse>`, `info symbol <adresse>` |
| Register | `info registers`, `print $sp`, `print $pc`, `print $lr` |
| Aufrufkette | `backtrace`, `info frame`, `up`, `down`, `frame <n>` |
| Watchpoint | `watch var`, `rwatch var` (lesen), `awatch var` (lesen/schreiben) |
| Code ansehen | `list`, `disassemble`, `disassemble /s` |
| Renode ansprechen | `monitor <renode-befehl>` |
| Beenden | `quit` |

## Anhang B: Renode-Spickzettel

| Aufgabe | Befehl |
|---|---|
| Starten | `renode --disable-xwt --console renode/nucleo.resc` |
| Hilfe | `help` |
| Bausteine anzeigen | `peripherals` |
| PC / SP lesen | `sysbus.cpu PC`, `sysbus.cpu SP` |
| UART in Datei (optional) | `sysbus.usart2 CreateFileBackend @/tmp/usart2.log true` |
| Zurücksetzen | `machine Reset` |
| Beenden | `quit` |

## Anhang C: Häufige Probleme

| Problem | Ursache / Lösung |
|---|---|
| `Connection refused` in GDB | Renode läuft nicht oder hat den GDB-Server nicht gestartet (`machine StartGdbServer 3333` im Script?). |
| `Address already in use` | Port 3333 ist belegt (altes Renode, OpenOCD). Alte Prozesse beenden. |
| `No symbol "xyz" in current context` | Falscher Name oder GDB wurde ohne ELF-Datei gestartet. Namen mit `nm` prüfen; `file <elf>` einlesen. |
| `<optimized out>` | Release-Build oder Variable im Register. Für diese Aufgabe den Debug-Build verwenden. |
| Alte Werte nach Neubau | Renode läuft noch mit der alten ELF-Datei: Renode neu starten. |
| `print _sdata` zeigt Unsinn | Linker-Symbol! Immer `print &_sdata` benutzen. |
| Programm läuft „einfach durch" | Nach dem Neustart von GDB ist die CPU evtl. noch am Laufen: mit `Ctrl+C` anhalten oder `monitor machine Reset`. |
| Watchpoint löst nie aus | Variable wird nicht verändert, oder die Renode-Version unterstützt keine Watchpoints: mit Breakpoints und `x`-Vergleichen arbeiten. |

## Anhang D: Mini-Glossar

| Begriff | Bedeutung |
|---|---|
| ELF | Dateiformat des Programms inkl. Symbolen und Debug-Infos |
| Linker-Script | Legt fest, welche Abschnitte an welchen Adressen liegen |
| Vektortabelle | Tabelle am Flash-Anfang mit Start-SP und den Adressen aller Handler |
| Reset-Handler | Erste Funktion, die nach dem Reset läuft |
| SP | Stack Pointer: Adresse des Stack-Endes |
| LR | Link Register: Rücksprungadresse des letzten Funktionsaufrufs |
| PC | Program Counter: Adresse der nächsten Instruktion |
| Stack-Frame | Stack-Abschnitt einer Funktion (lokale Variablen, gesicherte Register) |
| Thumb-Bit | Bit 0 einer Sprungadresse; beim Cortex-M immer 1 |
| `_sbrk` | Funktion, mit der `malloc` mehr Heap-Speicher anfordert |
| Watchpoint | Haltepunkt auf einen Speicherzugriff statt auf eine Codezeile |