# NES Emulator (C++17)

A modular NES (Nintendo Entertainment System) emulator written in C++ as a
learning project for computer system architecture: CPU design, bus
communication, video timing, and audio generation.

## What works

- **CPU**: full 6502/2A03 instruction set, verified against nestest's
  instruction log (8991/8991 lines) including common unofficial opcodes
- **PPU**: background + sprite rendering, loopy scrolling, nametable
  mirroring, presented through an SDL2 window at 60 fps
- **APU**: pulse/triangle/noise/DMC channels with frame counter and IRQs,
  mixed to 44.1 kHz mono audio
- **Input**: keyboard-driven joypad (strobe/shift-register protocol)
- **Mappers**: 0 (NROM), 1 (MMC1), 2 (UxROM), 3 (CNROM), 4 (MMC3),
  7 (AxROM), 9 (MMC2), 66 (GxROM)
- **Accuracy tests**: blargg's hardware test ROM suite runs as part of
  `nes_tests` with a known-failure baseline

Development history and roadmap live in [`PROGRESS.md`](PROGRESS.md).

## Requirements

- CMake 3.16+ and a C++17 compiler
- SDL2 (optional — without it the `nes_emulator` target is skipped and only
  the headless tests build)

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

## Run

```bash
./build/nes_emulator path/to/game.nes
```

With no display available (or for a quick smoke test):

```bash
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  ./build/nes_emulator tests/fixtures/nestest.nes
```

### Controls

| Key | Controller |
|---|---|
| `X` | A |
| `Z` | B |
| `Right Shift` | Select |
| `Enter` | Start |
| Arrow keys | D-pad |
| `Esc` | Quit |

## Tests

```bash
./build/nes_tests
```

This runs the unit tests (CPU, PPU, APU, controllers, mappers, and the
nestest log comparison) plus, when present, blargg's hardware test ROMs.

The blargg ROMs are not checked in. Fetch them once:

```bash
./tests/fetch_blargg.sh
./build/nes_tests
```

blargg results are compared against `tests/blargg_baseline.txt`:

- a ROM that fails but is **not** in the baseline breaks the build
  (regression)
- a ROM in the baseline that **starts passing** also breaks the build until
  its lines are removed — shrinking that file is the accuracy scoreboard

## Project structure

```
include/   headers (CPU, PPU, APU, Bus, mappers, ...)
src/       implementation, incl. main.cpp (SDL front-end)
tests/     test runner, test files, fixtures, blargg baseline + fetch script
build/     CMake output (gitignored)
PROGRESS.md  milestone-by-milestone development log and roadmap
```
