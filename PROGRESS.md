# NES Emulator Development Log & Roadmap

This log records every step of development so anyone can resume and understand the codebase state.

## Current Status: Milestone 1 - Project Setup & Architecture Skeleton

### Completed Steps:
- [x] Initialized Git repository (branch `main`).
- [x] Created project directory layout (`src/`, `include/`, `tests/`).
- [x] Set up `CMakeLists.txt` build configuration.

### Active Step:
- [ ] Implement `RAM` (2KB internal RAM + mirroring) and `Bus` (64KB CPU Address Space routing).

### Next Steps:
- [ ] Implement 6502 CPU register structure & reset sequence.
- [ ] Implement CPU addressing modes & cycle execution.
