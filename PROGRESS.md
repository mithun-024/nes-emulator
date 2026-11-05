# NES Emulator Development Log & Roadmap

This log records every step of development so anyone can resume and understand the codebase state.

## Completed Milestones

### Milestone 10: GxROM/MMC2 Mappers + blargg Hardware Test Suite ✅
- **Concept**: **GxROM (mapper 66)** is the cheapest possible mapper — one register at `$8000–$FFFF` whose bits 4–5 pick a 32KB PRG bank and bits 0–1 an 8KB CHR bank. **MMC2 (mapper 9, PxROM)** is the opposite extreme: 8KB PRG banking (`$E000` selects `$8000–$BFFF`, `$C000–$FFFF` fixed to the last bank) plus four 4KB CHR banks chosen by two PPU *latches* — the PPU fetching tile `$FD`/`$FE` from a pattern table (`$x0FD8–$x0FDF` = plane 0, `$x0FE8–$x0FEF` likewise) loads latch 0/1 with that value, and each half of pattern memory then reads through the register programmed for that latch state (`$A000`/`$B000` for `$0000`, `$C000`/`$D000` for `$1000`); `$F000` sets mirroring. The **blargg test harness** runs 44 hardware-test ROMs through the normal `System` loop and reads the standard PRG-RAM status protocol (`$6000` = `$80` running → `0` pass, text report at `$6004`).
- **Code**:
  - `include/MapperGxrom.hpp` / `src/MapperGxrom.cpp` — mapper 66 PRG/CHR register; `include/MapperMmc2.hpp` / `src/MapperMmc2.cpp` — mapper 9 with `mutable` latches updated from `ppuRead()`.
  - `src/Bus.cpp` — `insertCartridge` now builds mappers 1, 2, 3, 4, 7, 9, 66; unsupported-mapper fixture moved to mapper 5 (MMC5).
  - `src/CPU.cpp` — **7 missing unofficial opcodes** found by `instr_test-v5`: AAC/ANC (`$0B/$2B`), ASR/ALR (`$4B`), ARR (`$6B`, C = bit 6, V = bit 6 xor bit 5), ATX/LXA (`$AB`), AXS/SBX (`$CB`, flags as CMP), SHY/SYA (`$9C`) and SHX/SXA (`$9E`) — the store-high pair stores `reg & (operand high + 1)` and, on a page crossing, collapses the store address's high byte to that same value (the classic bus-conflict corruption).
  - `src/System.cpp` + `include/CPU.hpp` — **6502 I-flag interrupt delay**: `irqPollDelay` is set by CLI/SEI/PLP so the interrupt poll after them still sees the *pre-instruction* I flag (IRQ exactly one instruction after `CLI`, one IRQ just after `CLI SEI`, none after `CLI RTI`); RTI/BRK use the freshly loaded I.
  - `tests/test_blargg.cpp` + `tests/fetch_blargg.sh` + `tests/blargg_baseline.txt` — harness with a known-failure baseline: the build fails only on a **regression** (failing ROM not in the baseline) or a **stale entry** (listed ROM now passes). ROMs are downloaded on demand and gitignored.
  - `tests/test_mappers.cpp` — +11 tests for GxROM/MMC2 (27 → 28 mapper tests total incl. latch flips, hi-plane non-flip, wrap rules).
- **Bugs caught by blargg's ROMs** (fixed this milestone):
  1. CNROM mapped **no PRG RAM** at `$6000` — blargg's CNROM ROMs (`cpu_dummy_reads`, `ppu_read_buffer`) write their status there, so they could never report; PRG RAM added (dev carts rely on it even though real CNROM boards omit it).
  2. MMC1 **CHR RAM writes ignored banking** — writes landed at the raw address while reads went through the bank registers; `chrOffset()` now shared by read and write.
  3. IRQ serviced **immediately after `CLI`** instead of after the following instruction; then the naive fix broke `CLI SEI` — resolved with the `irqPollDelay` model above.
  4. Unimplemented unofficial opcodes silently became 2-cycle NOPs that didn't consume their operand (would desync PC on any ROM using them).
- **Baseline (25 known failures, all tracked in `tests/blargg_baseline.txt`)**: PPU vblank/NMI exact timing (8), dummy writes/reads (3), I/O-space execution + instruction timing (3), NMI-during-BRK (1), APU `$4017`/frame-IRQ/power-on state (7), PPU `$2007` buffer/open-bus/OAM stress (3).
- **Excluded suites**: `cpu_reset/*` (needs a physical reset press mid-test), `blargg_nes_cpu_test5` + `cpu_dummy_reads` (older framework: screen-only output, no `$6000` protocol).
- **Verified**: **220 unit assertions** pass (incl. nestest **8991/8991**); blargg run = 44 ROMs, 19 passing / 25 baseline-matching failures (~8 s); headless `./nes_emulator tests/fixtures/nestest.nes` → `Reset -> PC = 0xC004`.

### Milestone 9: UxROM / CNROM / AxROM Mappers ✅
- **Concept**: Three low-cost mapper families cover a large slice of the library. **UxROM (mapper 2)** splits PRG into 16KB banks: a write to `$8000–$FFFF` picks the bank shown at `$8000–$BFFF` while `$C000–$FFFF` is hardwired to the last bank (the fixed bank holds the reset/IRQ vectors) — CHR is almost always 8KB CHR **RAM**, and there is no PRG RAM. **CNROM (mapper 3)** leaves PRG exactly as NROM (16KB mirrored or 32KB linear) and only banks CHR: each write to `$8000–$FFFF` selects an 8KB CHR ROM bank covering both pattern tables. **AxROM (mapper 7)** swaps 32KB PRG blocks as a unit (so `$8000` and `$C000` always come from the same bank) and, uniquely, reprograms mirroring from the same write: bit 4 = 0 → single-screen low (nametable at `$2000`), bit 4 = 1 → single-screen high (nametable at `$2400`).
- **Code**:
  - `include/Mapper2.hpp` / `src/Mapper2.cpp` — UxROM: 16KB bank select with fixed last bank, 8KB CHR RAM, no PRG RAM.
  - `include/MapperCnrom.hpp` / `src/MapperCnrom.cpp` — CNROM: NROM PRG mapping + 8KB CHR bank register (named `MapperCnrom` because `Mapper3` already means MMC3/mapper 4).
  - `include/MapperAxrom.hpp` / `src/MapperAxrom.cpp` — AxROM: 32KB PRG switch + `bus->setMirror(SingleLow/SingleHigh)` from bit 4.
  - `src/Bus.cpp` — `insertCartridge` now builds mappers 1, 2, 3, 4, 7.
  - `tests/test_mappers.cpp` — 27 tests: bank switching/wrapping, fixed banks, CHR RAM, no-PRG-RAM, CNROM 16KB mirror, AxROM mirroring modes; PRG image filled with a configurable bank-index granularity.
  - `tests/test_cartridge.cpp` — the unsupported-mapper fixture moved from mapper 2 (now implemented) to mapper 9 (MMC2, still unimplemented).
- **Bugs caught by tests**: 5 failures in the AxROM group, all test-side — the fixture filled PRG with **16KB** bank indices, so `$C000` read `index+1` while AxROM exposes one **32KB** bank across both halves (and the 32KB-wide mirroring test used a single-bank cart where no bank switch could show). Fixed by a `prgFillBytes` parameter on the fixture builder, defaulting to 16384.
- **Verified**: **198 assertions** pass (incl. nestest **8991/8991**); headless `./nes_emulator tests/fixtures/nestest.nes` → `Mapper 0, PRG 16KB, CHR 8KB` / `Reset -> PC = 0xC004`.

### Milestone 8: APU + MMC1/MMC3 Mappers ✅
- **Concept**: The APU has 4 channel types — 2 pulse channels (duty-cycle waveform + hardware sweep), triangle (linear counter, no volume), noise (LFSR), and DMC (delta-modulated sample playback from CPU memory). A programmable frame counter clocks envelope/length/sweep units at 4-step (29830 CPU cycles, raises frame IRQ at 29829) or 5-step (37282, no IRQ) rates. `$4015` enables channels and reports status/IRQ flags; `$4017` selects frame mode. Output mixes to 44.1 kHz mono samples queued to SDL audio. **MMC1 (mapper 1)** writes a 5-bit shift register serially (LSB first, bit 7 resets) into control/CHR/PRG registers: PRG modes 0/1 switch 32KB at `$8000`, mode 2 fixes first bank + switches `$C000`, mode 3 switches `$8000` + fixes last; CHR is 4KB×2 or 8KB (R1 ignored); control bits 0–1 select one of 4 mirroring modes. **MMC3 (mapper 4)** uses `$8000`/`$8001` index/data pairs for 8KB PRG and 1KB/2KB CHR banking, `$A000` mirroring, and a scanline IRQ counter (`$C000` latch, `$C001` reload, `$E000`/`$E001` disable/enable) clocked once per rendered scanline — CPU reads of `$E000–$FFFF` acknowledge.
- **Code**:
  - `include/APU.hpp` / `src/APU.cpp` — pulse/triangle/noise/DMC channels, frame counter, length/envelope/sweep units, 44.1 kHz mixer + `drainSamples()`, frame & DMC IRQs, test hooks.
  - `include/Mapper1.hpp` / `src/Mapper3.cpp` → `Mapper1`/`Mapper3` — full MMC1 serial protocol + banking; MMC3 banking + IRQ counter (`scanlineTick()`).
  - `include/Bus.hpp` / `src/Bus.cpp` — `$4000–$4013/$4015/$4017` → APU, `irqPending()` = APU OR mapper, `setMirror()`, `addDmaStall()` (DMC), mapper IRQ ack on `$E000+` reads, mappers 1 & 4 in `insertCartridge`.
  - `include/CPU.hpp` / `src/CPU.cpp` — `irq()` (push PC+P, vector `$FFFE`, +7 cycles) for level-triggered interrupts.
  - `include/PPU.hpp` / `src/PPU.cpp` — `Mirror` extended with `SingleLow`/`SingleHigh`; clocks `mapper->scanlineTick()` at dot 260 of rendered scanlines.
  - `src/System.cpp` — `step()` ticks the APU alongside the PPU and services a pending IRQ when the I flag is clear (NMI has priority).
  - `src/main.cpp` — SDL audio device (44.1 kHz mono), per-frame `drainSamples()` → `SDL_QueueAudio`.
  - `tests/test_apu.cpp`, `tests/test_mmc1.cpp`, `tests/test_mmc3.cpp` — 11 + 9 + 10 tests; registered in `tests/main.cpp`.
- **Bugs caught before tests ran** (found in review of the new mapper code, fixed then verified by tests):
  1. MMC1 8KB CHR mode used `chrBank1` for `$1000–$1FFF` — in 8KB mode R0 covers the whole `$0000–$1FFF` (R1 ignored).
  2. MMC1 PRG modes lumped mode 1 with mode 3 — modes **0 and 1** are both 32KB switching; mode 2 fixes first, mode 3 fixes last.
  3. MMC3 2KB CHR slots scaled the base with `×2` and split pages with `>>11` — 1KB base is `reg & 0xFE` (no ×2), half selected by `>>10`.
  4. MMC3 didn't mask R6/R7 top bits (64×8KB limit) — added `& 0x3F`.
  5. Stale `test_cartridge` fixture still expected mapper 4 rejection — now supported; switched the unsupported-mapper test to mapper 2 (UxROM).
- **Verified**: **171 assertions** pass (incl. nestest **8991/8991**); headless `./nes_emulator nestest.nes` runs with audio queued (3 s smoke test, no crash).

### Milestone 7: Controller Input ✅
- **Concept**: The NES controller is an 8-bit parallel-load shift register. The CPU writes `$4016` bit 0 high then low (strobe); while high the register reloads continuously (reads return A), and the falling edge latches the buttons. Each subsequent read of `$4016`/`$4017` clocks out one bit in fixed order — **A, B, Select, Start, Up, Down, Left, Right** — and reads past the eighth return 1 (official pad behavior). The strobe line is shared: one `$4016` write latches both ports; `$4016` reads port 1, `$4017` reads port 2.
- **Code**:
  - `include/Controller.hpp` / `src/Controller.cpp` — button bitmap, strobe latch, serial `read()` (shifts right, fills with 1s after 8).
  - `include/Bus.hpp` / `src/Bus.cpp` — two controller ports; `$4016`/`$4017` reads clock the shifters, `$4016` writes strobe both (APU `$4015` still a stub).
  - `src/main.cpp` — SDL keyboard → port 1: `X`=A, `Z`=B, `RShift`=Select, `Enter`=Start, arrows=D-pad (polled each frame via `SDL_GetKeyboardState`).
  - `tests/test_controller.cpp` — serial bit order, all/none pressed, 9th+ reads return 1, strobe-held-high live reads, port independence, latch freeze across button release, re-strobe picks up new state.
- **Bug caught by tests**: expected constant `0x42` for B+Right — `BTN_RIGHT` is bit 7, so the value is `0x82` (test arithmetic error, not the shifter).
- **Verified**: all tests pass; nestest **8991/8991**.

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
| 0x4000–0x4017 | APU + joypads | done → Milestones 7 + 8 |
| 0x4020–0xFFFF | Cartridge (PRG ROM / mapper) | done → Milestone 4 (Mapper 0) |
Stubs return 0x00 so the build stays green — pattern used all project: interface now, implement later.

**Tests — `tests/test_bus.cpp`**
`assert()` aborts if false. We write 0xAB at 0x0000 and assert it reads back from all 4 mirrors, plus a reverse test from 0x0FFF. Re-run after every change as a safety net.

**Build — `CMakeLists.txt`**
CMake builds two targets: `nes_emulator` (src/main.cpp) and `nes_tests` (tests + shared sources, main.cpp excluded). Run `./nes_tests` to verify.

**Bug hit during build**: `size_t` required `#include <cstddef>` in `include/RAM.hpp` — C++ headers do not leak into each other.

## Active Step
**Milestone 11 in progress: shrinking blargg baseline (25 → 18 failing ROMs)**
- **Completed so far**:
  - Cycle-level device sync (`bus->advanceCycle()` per CPU bus read/write, `bus->syncDevicesTo()` for internal cycle catch-up).
  - PPU NMI edge-latching model with 2-dot maturation / glitch-pulse rejection and deferred `$2000` write effects.
  - VBL flag & NMI suppression on `$2002` reads around scanline 241 dot 1.
  - Pre-render scanline (261) odd/even 1-dot skip at dot 339 when rendering enabled.
  - Baseline reduced by 7 ROMs (now passing): `02-vbl_set_time`, `04-nmi_control`, `05-nmi_timing`, `06-suppression`, `07-nmi_on_timing`, `08-nmi_off_timing`, `09-even_odd_frames`.
  - 220 unit tests + nestest 8991/8991 + 44 blargg ROMs pass cleanly against updated baseline.

- **Immediate Todos when resuming (Tomorrow)**:
  - [ ] **PPU**: `ppu_vbl_nmi/10-even_odd_timing.nes` (fine-tune dot 339 skip vs `$2001` BG enable delay).
  - [ ] **CPU dummy writes & reads** (`cpu_dummy_writes_oam`, `cpu_dummy_writes_ppumem`, `instr_misc`):
    - Implement RMW double-write (write original value back, then write modified value) for ASL, LSR, ROL, ROR, INC, DEC, DCP, ISC, SLO, RLA, SRE, RRA.
    - Implement indexed dummy reads for ABS,X / ABS,Y / (ZP),Y (page-crossing for reads; always for writes and RMW).
    - Implement RTS dummy read of the byte following opcode (`read(PC);`).
  - [ ] **CPU unofficial opcodes & timing** (`instr_timing`):
    - Implement `$8B` (XAA / ANE imm, 2 cyc), `$93` (SHA (zp),Y, 6 cyc), `$9B` (TAS abs,Y, 5 cyc), `$9F` (SHA abs,Y, 5 cyc), `$BB` (LAS abs,Y, 4+ cyc).
  - [ ] **CPU / Bus open bus** (`cpu_exec_space_apu`, `cpu_exec_space_ppuio`):
    - Model open bus on `$4000–$401F` / `$4018–$40FF` so execution from write-only ports returns the last driven data bus byte (e.g. `$40` = RTI).
  - [ ] **CPU interrupts & APU** (`cpu_interrupts_v2`, `apu_test`, `apu_reset/*`):
    - NMI interrupting BRK / B flag preservation.
    - APU `$4017` write jitter and frame IRQ timing; length counter reset behavior.
  - [ ] **PPU stress & buffer** (`ppu_read_buffer`, `ppu_open_bus`, `oam_stress`):
    - STA abs,Y dummy read to `$2007`.
    - PPU open bus ~600ms decay to zero.

## Next Steps
- [x] Milestone 6: PPU rendering + SDL2 window.
- [x] Milestone 7: Controller input.
- [x] Milestone 8: APU + advanced mappers (MMC1/MMC3).
- [x] Milestone 9: more mappers (UxROM/CNROM/AxROM).
- [x] Milestone 10: GxROM/MMC2 mappers + blargg hardware test suite.
- [ ] Milestone 11: shrink the blargg baseline (in-flight: 7/25 fixed; 18 remaining).
- [ ] Milestone 12: save states.
- [ ] Milestone 13: gamepad config / polish.

## How to resume after a break
1. Read this file (PROGRESS.md) — it is the source of truth for state.
2. `git log --oneline` — see what landed.
3. Build & test: `cmake --build build && ./build/nes_tests`
4. Pick up from the Milestone 11 checklist above.
