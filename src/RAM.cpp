#include "RAM.hpp"

RAM::RAM() {
    reset();
}

uint8_t RAM::read(uint16_t addr) const {
    // 2KB RAM mirrored from 0x0000 to 0x1FFF (bitmask 0x07FF)
    return memory[addr & 0x07FF];
}

void RAM::write(uint16_t addr, uint8_t data) {
    memory[addr & 0x07FF] = data;
}

void RAM::reset() {
    memory.fill(0x00);
}
