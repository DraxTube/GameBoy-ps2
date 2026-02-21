# PS2 Game Boy Emulator

Un emulatore Game Boy (DMG) per PlayStation 2, scritto in C, compilabile con il PS2DEV SDK tramite GitHub Actions.

## Caratteristiche

- CPU LR35902 (Z80-like) completa con tutti gli opcode
- PPU con background, window e sprites
- MMU con I/O registers completi
- Timer e interrupt handler
- Input mappato dal DualShock 2
- Output su schermo PS2 scalato (160×144 → ~640×432)
- ROM embeddabile nel binario ELF

## Controlli

| DualShock 2 | Game Boy |
|-------------|----------|
| Croce (×)   | A        |
| Cerchio (○) | B        |
| Select      | Select   |
| Start       | Start    |
| D-Pad       | D-Pad    |

## Come usare

### 1. Clona il repository

```bash
git clone https://github.com/TUO_USERNAME/ps2gb.git
cd ps2gb
```

### 2. Aggiungi la tua ROM

Converti il file `.gb` in C:

```bash
python3 tools/embed_rom.py tua_rom.gb > src/rom_data.c
```

### 3. Compila con GitHub Actions

Fai push su GitHub — la workflow `.github/workflows/build.yml` compila automaticamente usando il container Docker `ps2dev/ps2dev:latest`.

Il file `ps2gb.elf` sarà disponibile come Artifact nella sezione Actions, e come Release ad ogni push su `main`.

### 4. Copia sull'HD del PS2

Copia `ps2gb.elf` (o `ps2gb_packed.elf`) sull'hard disk della PS2 usando:
- uLaunchELF / wLaunchELF
- SMS (Simple Media System)
- FreeMCBoot + file manager

## Compilazione locale (se hai PS2DEV installato)

```bash
export PS2DEV=/usr/local/ps2dev
export PS2SDK=$PS2DEV/ps2sdk
export PATH=$PS2DEV/bin:$PS2DEV/ee/bin:$PATH

make
```

## Struttura del progetto

```
ps2gb/
├── src/
│   ├── main.c       # Entry point PS2, loop principale
│   ├── gb.h         # Strutture dati Game Boy
│   ├── gb.c         # Inizializzazione e input
│   ├── cpu.c/h      # CPU LR35902 completa
│   ├── mmu.c/h      # Memory Management Unit
│   ├── ppu.c/h      # Pixel Processing Unit
│   ├── display.c/h  # Rendering PS2 via gsKit
│   └── rom_data.c   # ROM embeddato (da sostituire)
├── tools/
│   └── embed_rom.py # Script per embeddare ROM
├── .github/
│   └── workflows/
│       └── build.yml
├── Makefile
└── README.md
```

## Limitazioni note

- Solo ROM **senza MBC** (32KB, no banking) — la maggior parte dei giochi originali GB usa MBC1/MBC3/MBC5
- Audio non implementato
- Link Cable non implementato
- Game Boy Color non supportato (solo DMG)

## Aggiungere supporto MBC

Per supportare giochi più grandi, estendi `mmu.c` implementando MBC1:
- Intercetta le write in `0x0000-0x7FFF` per il bank switching
- Alloca un buffer ROM più grande in `MMU_State`

## Licenza

MIT — usa, modifica e distribuisci liberamente.
