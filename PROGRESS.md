# NES Emulator Development Log & Roadmap

This log records every step of development so anyone can resume and understand the codebase state.

## Completed Milestones

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
| 0x2000–0x3FFF | PPU registers | stub → Milestone 5 |
| 0x4000–0x4017 | APU + joypads | stub → Milestone 7/8 |
| 0x4020–0xFFFF | Cartridge (PRG ROM / mapper) | stub → Milestone 4 |
Stubs return 0x00 so the build stays green — pattern used all project: interface now, implement later.

**Tests — `tests/test_bus.cpp`**
`assert()` aborts if false. We write 0xAB at 0x0000 and assert it reads back from all 4 mirrors, plus a reverse test from 0x0FFF. Re-run after every change as a safety net.

**Build — `CMakeLists.txt`**
CMake builds two targets: `nes_emulator` (src/main.cpp) and `nes_tests` (tests + shared sources, main.cpp excluded). Run `./nes_tests` to verify.

**Bug hit during build**: `size_t` required `#include <cstddef>` in `include/RAM.hpp` — C++ headers do not leak into each other.

## Active Step
(None — between milestones. Next: Milestone 3, addressing modes + full instruction set.)

## Next Steps
- [ ] Milestone 3: Addressing modes + instruction set, verified against nestest.nes log.
- [ ] Milestone 4: iNES cartridge loader + Mapper 0.
- [ ] Milestone 5: PPU registers & memory mapping.
- [ ] Milestone 6: PPU rendering + SDL2 window.
- [ ] Milestone 7: Controller input.
- [ ] Milestone 8: APU + advanced mappers (MMC1/MMC3).

## How to resume after a break
1. Read this file (PROGRESS.md) — it is the source of truth for state.
2. `git log --oneline` — see what landed.
3. Build & test: `cd build && cmake .. && make && ./nes_tests`
