# NES Emulator (C++17)

A modular NES (Nintendo Entertainment System) Emulator written in C++ for learning computer system architecture, CPU design, bus communication, and graphics rendering.

## Project Structure
- `src/`: Source files (`main.cpp`, `Bus.cpp`, `CPU.cpp`, etc.)
- `include/`: Header files (`Bus.hpp`, `CPU.hpp`, `RAM.hpp`, etc.)
- `tests/`: Unit tests and verification scripts
- `PROGRESS.md`: Step-by-step progress tracking log

## Build Instructions
```bash
mkdir build && cd build
cmake ..
make
./nes_emulator
```
