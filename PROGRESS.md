# NES Emulator Development Log & Roadmap

This log records every step of development so anyone can resume and understand the codebase state.

## Completed Milestones

### Milestone 6: PPU Rendering + SDL2 Window ✅
- **Concept**: The PPU produces one frame every 262 scanlines × 341 dots = 89342 dots ≈ 29781 CPU cycles (NTSC: 3 PPU dots per CPU cycle). CPU and PPU run in lockstep: after every CPU instruction the PPU advances 3 dots per cycle consumed (including OAM DMA stalls), and a pending NMI is serviced between instructions (+7 cycles). The finished 256×240 ARGB8888 framebuffer is presented through an SDL2 streaming texture at ~60 fps.
- **Code**:
  - `include/System.hpp` / `src/System.cpp` — `step()` (instruction + PPU sync + NMI + DMA stall), `runFrame()`, `runCycles()`.
  - `src/main.cpp` — SDL2 window (256×240 at 3× integer scale), event loop (ESC/quit), ~60 fps cap, FPS title.
  - `CMakeLists.txt` — `find_package(SDL2)`; if missing, the `nes_emulator` target is disabled with a warning so tests still build headless.
  - `tests/test_render.cpp` — background pixel + left-clip, sprite placement, steady-state frame length (29781 ±10 cycles), NMI integration (hand-assembled 6502 program counts NMIs into RAM), OAM DMA stall consumed by `System::step`.
- **Bugs caught by tests** (both test-side, not renderer-side):
  1. Expected palette `$21` = `0xFF4240FF` — that is actually `$12`; correct `$21` is `0xFF64B0FF`.
  2. Sprite test left the other 63 OAM entries at y=0 (reset fill), so 7 ghost sprites (tile 0, x=0) drew at the left edge; fixtures now hide them with y=$FF.
- **Verified**: all tests pass; nestest **8991/8991**; `./nes_emulator nestest.nes` opens a window and renders nestest's menu (headless check: 5375 non-black pixels after 120 frames).

### Milestone 5: PPU Registers & Memory Mapping ✅
- **Concept**: The CPU sees PPU registers at `$2000–$2007`, mirrored every 8 bytes through `$3FFF`. Internally the PPU owns 2KB CIRAM (nametables), 256B OAM, and 32B palette RAM. `$2005`/`$2006` share a two-write address latch (loopy `v`/`t`/`x`/`w`); reading `$2002` clears vblank **and** resets that latch. `$2007` is buffered for nametable/palette reads (palette is unbuffered) and increments `v` by 1 or 32 based on PPUCTRL bit 2. Nametable mirroring (H: `|A|A|B|B|`, V: `|A|B|A|B|`) comes from the cartridge header.
- **Code**:
  - `include/PPU.hpp` / `src/PPU.cpp` — full register file, loopy scroll/address internals, CIRAM/OAM/palette, H/V nametable mirror, palette `$3F10` aliasing, `$2007` read buffer, open-bus on write-only regs.
  - `include/Bus.hpp` / `src/Bus.cpp` — `$2000–$3FFF` routes to `PPU::regRead/regWrite` (`addr & 7`); `insertCartridge` pushes mapper + mirroring into the PPU.
  - `tests/test_ppu.cpp` — register mirroring, `$2002` side effects, two-write `$2006`, `$2007` increment 1/32 + buffer, palette alias, H/V nametable banks, OAM auto-increment, CHR via mapper.
- **Bugs caught by tests**:
  1. `setMapper(Mapper*, bool verticalMirroring)` — parameter shadowed the member, so mirroring mode was never updated (always default vertical). Renamed param to `vertical`.
  2. `mirrorNametable` returned `uint8_t`, truncating bank-B index `0x400 → 0x00` (both nametables aliased). Return type fixed to `uint16_t`.
  3. Horizontal test initially wrote different values to `$2000` and `$2400` (same bank under H-mirror) — fixture corrected to write only NT A then NT B.
- **Verified**: all tests pass; nestest still **8991/8991**.

### Milestone 4: iNES Cartridge Loader + Mapper 0 (NROM) ✅
- **Concept**: A `.nes` file starts with a 16-byte iNES header (`NES\x1A`, PRG size in 16KB units, CHR size in 8KB units, mapper number split across flags 6/7). Mapper 0 (NROM) does no bank switching: 16KB PRG is mirrored at `0x8000` **and** `0xC000` (so the reset vector at `0xFFFC` reads the same physical bytes as `0x3FFC` in the file); 32KB PRG fills `0x8000–0xFFFF` linearly. 8KB PRG RAM lives at `0x6000–0x7FFF` (writable); PRG ROM writes are ignored. CHR is 8KB at PPU `0x0000–0x1FFF` (ROM, or RAM when the header says 0 CHR units).
- **Code**:
  - `include/Cartridge.hpp` / `src/Cartridge.cpp` — `Cartridge::load(path)` parses the header, skips the optional 512-byte trainer, reads PRG/CHR; returns `nullptr` + error string on bad files.
  - `include/Mapper.hpp` — abstract `cpuRead/cpuWrite` + `ppuRead/ppuWrite` interface (PPU side lands in Milestone 5).
  - `include/Mapper0.hpp` / `src/Mapper0.cpp` — NROM: 16KB mirror mask `addr & 0x3FFF`, 32KB linear, PRG RAM, CHR ROM/RAM.
  - `include/Bus.hpp` / `src/Bus.cpp` — `insertCartridge()` builds the mapper from the header id (rejects unsupported mappers); `0x4020–0xFFFF` routes through the mapper when present, else falls back to the old flat buffer so no-cartridge unit tests still work.
  - `tests/test_cartridge.cpp` — header parse, bad-file rejection, NROM mirror + reset vector, PRG RAM r/w + ROM write-protect, unsupported-mapper error, `cpu.reset()` through Mapper 0.
  - `tests/test_nestest.cpp` — refactored to use `Cartridge::load` + `insertCartridge` (dropped the temporary inline loader).
  - `src/main.cpp` — `./nes_emulator game.nes` loads a ROM and prints mapper/PRG/CHR + reset PC; no-arg demo unchanged.
- **Bug caught by tests**: synthetic MMC3 test header initially set flags 7 wrong so the parsed mapper id wasn't 4 — fixed the fixture bytes (header is the test's problem, not the loader's). Also a C++ name clash: member `prgRamSize` vs accessor `prgRamSize()` → member renamed `prgRamBytes`.
- **Verified**: all tests pass; nestest still **8991/8991** through the real cartridge path; `./nes_emulator nestest.nes` prints `PC = 0xc004` (nestest's reset vector).

### Milestone 3: Addressing Modes + Full Instruction Set (nestest-verified) ✅
- **Concept**: Every 6502 instruction is `opcode + operands`; the addressing mode decides *where* the operand lives (immediate, zero page, absolute, indexed, indirect, relative for branches). Cycle counts are part of the ISA: page-crossed indexed reads and taken branches cost +1 cycle each. The NES CPU (2A03) ignores the decimal flag in ADC/SBC — binary only. nestest.nes exercises all official opcodes plus the common unofficial ones (LAX, SAX, SLO, RLA, SRE, RRA, DCP, ISC, multi-byte NOPs).
- **Code**:
  - `include/CPU.hpp` / `src/CPU.cpp` — `step()` fetch-decode-execute; addressing helpers (`addrImm`…`addrIndY`); ALU helpers (`opADC`/`opSBC`/`opCMP`/`opBIT`/shifts); `total_cycles` counter; full official set + unofficial opcodes nestest needs; `JMP ($xxxx)` implements the 6502 page-wrap bug (high byte from same page when low wraps).
  - `tests/test_nestest.cpp` — loads `tests/fixtures/nestest.nes` (minimal iNES PRG load, mapper 0 mirror — full loader is Milestone 4), runs automated mode from `0xC000`, compares PC/A/X/Y/P/SP/CYC against every line of `nestest.log` (8991 instructions).
  - `tests/fixtures/` — nestest.nes + nestest.log (kevtris test ROM).
- **Bug caught by tests**: none this milestone — full log matched on first run (design note: push/pop order and page-cross penalties were written from the log's cycle column as ground truth).
- **Verified**: `./nes_tests` → `[PASS] nestest: 8991 instructions matched`.

### Milestone 2: 6502 CPU Registers, Reset & Stack ✅
- **Concept**: The 6502 has 7 visible registers: `A` (accumulator), `X`/`Y` (index registers), `PC` (16-bit program counter), `SP` (8-bit stack pointer), `P` (status flags `N V - B D I Z C`), plus an internal instruction register. On power-on/reset the CPU reads a **reset vector** — a 16-bit little-endian address stored at fixed location `0xFFFC/0xFFFD` — and jumps there; games place their startup code address in that vector. The stack lives in page `0x0100–0x01FF` and grows **downward** (push decrements SP, pull increments it).
- **Code**:
  - `include/CPU.hpp` / `src/CPU.cpp` — registers, flag helpers (`getFlag`/`setFlag` with bitmask constants `FLAG_C`…`FLAG_N`), `reset()` (loads PC from `0xFFFC`, sets `SP=0xFD`, `P=0x24`), stack ops (`push8`/`pop8`/`push16`/`pop16`), bus forwarding (`read`/`write`/`read16` little-endian).
  - `include/Bus.hpp` / `src/Bus.cpp` — added temporary `cartSpace[0x10000]` buffer for `0x4020–0xFFFF` so tests can plant a reset vector before the real iNES loader arrives (Milestone 4).
  - `tests/test_cpu.cpp` — reset vector, flag bit isolation, 8/16-bit stack push/pop round-trips, little-endian `read16`.
  - `tests/main.cpp` — single test runner calling `run_bus_tests()` + `run_cpu_tests()` (avoids duplicate `main`).
- **Bug caught by tests**: first `push8` implementation decremented SP *before* writing, but real 6502 stores at `0x0100+SP` **first**, then decrements (pull: increments first, then reads). Test failed → fixed in `src/CPU.cpp`.
- **Verified**: all tests pass; demo prints `PC = 0x8000`, `SP = 0xfd`, `P = 0x24` after reset.

### Milestone 1: Project Setup & Bus/RAM Skeleton ✅
- **Concept**: The NES CPU sees a flat 64KB address space (0x0000–0xFFFF), but only 2KB of internal RAM exists at 0x0000–0x07FF. Real hardware "mirrors" it 4 times so 0x0800–0x0FFF aliases back to 0x0000–0x07FF (implemented with `addr & 0x07FF`).
- **Code**:
  - `include/RAM.hpp` / `src/RAM.cpp` — 2KB RAM with mirroring mask.
  - `include/Bus.hpp` / `src/Bus.cpp` — CPU bus router with address ranges (RAM / PPU / APU / Cartridge), PPU+APU+Mapper stubs return 0 for now.
  - `tests/test_bus.cpp` — asserts mirroring at 0x0800, 0x1000, 0x1800, and cross-mirror writes.
- **Verified**: `nes_tests` all pass; `nes_emulator` runs demo read/write.
- **Commit**: `chore: initial repository structure and build config` + `feat: implement RAM and CPU bus with mirroring`

### Milestone 1 Code Walkthrough (for review/presentation)

**RAM mirroring — `src/RAM.cpp`**
```cpp
return memory[addr & 0x07FF];
```
The NES has only 2KB of RAM (2048 bytes), but the CPU address space maps RAM across 8KB (0x0000–0x1FFF). Real hardware does this by not wiring the upper address lines to the RAM chip — the chip only sees the low 11 bits of any address. `0x07FF` = binary `0000 0111 1111 1111` = exactly the low 11 bits.
- `0x0005 & 0x07FF` → `0x0005`
- `0x0805 & 0x07FF` → `0x0005` (same cell)
- `0x1805 & 0x07FF` → `0x0005` (same cell)
Writing `0xAB` to `0x0000` means reading `0xAB` from `0x0800`/`0x1000`/`0x1800` — all four "windows" see the same physical byte. Games depend on this behavior.

**The Bus — `src/Bus.cpp`**
The CPU is dumb: it puts a 16-bit address on the wires and asks for read/write; it does not know what device lives there. The Bus decodes the address and routes it:
| Range | Device | Status |
|---|---|---|
| 0x0000–0x1FFF | Internal RAM | done |
| 0x2000–0x3FFF | PPU registers | done → Milestone 5 |
| 0x4000–0x4017 | APU + joypads | stub → Milestone 7/8 |
| 0x4020–0xFFFF | Cartridge (PRG ROM / mapper) | done → Milestone 4 (Mapper 0) |
Stubs return 0x00 so the build stays green — pattern used all project: interface now, implement later.

**Tests — `tests/test_bus.cpp`**
`assert()` aborts if false. We write 0xAB at 0x0000 and assert it reads back from all 4 mirrors, plus a reverse test from 0x0FFF. Re-run after every change as a safety net.

**Build — `CMakeLists.txt`**
CMake builds two targets: `nes_emulator` (src/main.cpp) and `nes_tests` (tests + shared sources, main.cpp excluded). Run `./nes_tests` to verify.

**Bug hit during build**: `size_t` required `#include <cstddef>` in `include/RAM.hpp` — C++ headers do not leak into each other.

## Active Step
(None — between milestones. Next: Milestone 7, controller input.)

## Next Steps
- [x] Milestone 6: PPU rendering + SDL2 window.
- [ ] Milestone 7: Controller input.
- [ ] Milestone 8: APU + advanced mappers (MMC1/MMC3).

## How to resume after a break
1. Read this file (PROGRESS.md) — it is the source of truth for state.
2. `git log --oneline` — see what landed.
3. Build & test: `cd build && cmake .. && make && ./nes_tests`
