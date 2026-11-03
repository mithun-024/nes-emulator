# NES Emulator Development Log & Roadmap

This log records every step of development so anyone can resume and understand the codebase state.

## Completed Milestones

### Milestone 1: Project Setup & Bus/RAM Skeleton ✅
- **Concept**: The NES CPU sees a flat 64KB address space (0x0000–0xFFFF), but only 2KB of internal RAM exists at 0x0000–0x07FF. Real hardware "mirrors" it 4 times so 0x0800–0x0FFF aliases back to 0x0000–0x07FF (implemented with `addr & 0x07FF`).
- **Code**:
  - `include/RAM.hpp` / `src/RAM.cpp` — 2KB RAM with mirroring mask.
  - `include/Bus.hpp` / `src/Bus.cpp` — CPU bus router with address ranges (RAM / PPU / APU / Cartridge), PPU+APU+Mapper stubs return 0 for now.
  - `tests/test_bus.cpp` — asserts mirroring at 0x0800, 0x1000, 0x1800, and cross-mirror writes.
- **Verified**: `nes_tests` all pass; `nes_emulator` runs demo read/write.
- **Commit**: `chore: initial repository structure and build config` + `feat: implement RAM and CPU bus with mirroring`

## Active Step
(None — between milestones)

## Next Steps
- [ ] Milestone 2: 6502 CPU registers (A, X, Y, PC, SP, P flags) + reset sequence + fetch/decode/execute loop skeleton.
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
