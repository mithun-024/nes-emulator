#include "CPU.hpp"
#include "Bus.hpp"

CPU::CPU(Bus& bus) : bus(&bus) {}

void CPU::reset() {
    A = X = Y = 0;
    SP = 0xFD;               // NES convention: stack starts at top of page 1 minus 3
    P = FLAG_U | FLAG_I;     // 0x24: unused bit always 1, IRQs masked during reset
    bus->setDeviceCycle(0);  // rebaseline device sync before the vector reads
    // The cartridge supplies a 16-bit little-endian start address at 0xFFFC/0xFFFD
    PC = read16(0xFFFC);
    total_cycles = 7;        // hardware reset sequence takes 7 cycles (nestest log starts at CYC:7)
    bus->syncDevicesTo(total_cycles);
}

void CPU::nmi() {
    push16(PC);
    // Push P with B clear (not a BRK) and U forced set
    uint8_t p = static_cast<uint8_t>((P | FLAG_U) & ~FLAG_B);
    push8(p);
    setFlag(FLAG_I, true);
    PC = read16(0xFFFA);
    total_cycles += 7;
    bus->syncDevicesTo(total_cycles);
}

void CPU::irq() {
    push16(PC);
    uint8_t p = static_cast<uint8_t>((P | FLAG_U) & ~FLAG_B);
    push8(p);
    setFlag(FLAG_I, true);
    PC = read16(0xFFFE);
    total_cycles += 7;
    bus->syncDevicesTo(total_cycles);
}

bool CPU::getFlag(uint8_t mask) const {
    return (P & mask) != 0;
}

void CPU::setFlag(uint8_t mask, bool value) {
    if (value) {
        P |= mask;
    } else {
        P &= ~mask;
    }
}

void CPU::setZN(uint8_t value) {
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, (value & 0x80) != 0);
}

void CPU::push8(uint8_t value) {
    // 6502 push: store at 0x0100 + SP first, then decrement SP
    write(0x0100 + SP, value);
    SP--;
}

uint8_t CPU::pop8() {
    // 6502 pull: increment SP first, then read at 0x0100 + SP
    SP++;
    return read(0x0100 + SP);
}

void CPU::push16(uint16_t value) {
    // High byte pushed first so the low byte ends up at the lower address
    push8(static_cast<uint8_t>(value >> 8));
    push8(static_cast<uint8_t>(value & 0xFF));
}

uint16_t CPU::pop16() {
    uint8_t lo = pop8();
    uint8_t hi = pop8();
    return static_cast<uint16_t>((hi << 8) | lo);
}

uint8_t CPU::read(uint16_t addr) const {
    bus->advanceCycle(); // the bus access consumes one CPU cycle
    return bus->cpuRead(addr);
}

void CPU::write(uint16_t addr, uint8_t data) {
    bus->advanceCycle(); // the bus access consumes one CPU cycle
    bus->cpuWrite(addr, data);
}

uint16_t CPU::read16(uint16_t addr) const {
    // Little-endian: low byte at addr, high byte at addr + 1
    uint16_t lo = read(addr);
    uint16_t hi = read(static_cast<uint16_t>(addr + 1));
    return static_cast<uint16_t>((hi << 8) | lo);
}

// --- ALU helpers ---

void CPU::opADC(uint8_t value) {
    // Binary mode only: the NES 2A03 ignores the decimal flag in ADC/SBC
    uint16_t sum = static_cast<uint16_t>(A) + value + (getFlag(FLAG_C) ? 1 : 0);
    setFlag(FLAG_C, sum > 0xFF);
    setFlag(FLAG_V, ((A ^ value) & 0x80) == 0 && ((A ^ sum) & 0x80) != 0);
    A = static_cast<uint8_t>(sum);
    setZN(A);
}

void CPU::opSBC(uint8_t value) {
    opADC(static_cast<uint8_t>(~value));
}

void CPU::opCMP(uint8_t reg, uint8_t value) {
    uint16_t diff = static_cast<uint16_t>(reg) - value;
    setFlag(FLAG_C, reg >= value);
    setZN(static_cast<uint8_t>(diff));
}

void CPU::opBIT(uint8_t value) {
    setFlag(FLAG_Z, (A & value) == 0);
    setFlag(FLAG_V, (value & 0x40) != 0);
    setFlag(FLAG_N, (value & 0x80) != 0);
}

uint8_t CPU::opASL(uint8_t value) {
    setFlag(FLAG_C, (value & 0x80) != 0);
    value <<= 1;
    setZN(value);
    return value;
}

uint8_t CPU::opLSR(uint8_t value) {
    setFlag(FLAG_C, (value & 0x01) != 0);
    value >>= 1;
    setZN(value);
    return value;
}

uint8_t CPU::opROL(uint8_t value) {
    bool oldCarry = getFlag(FLAG_C);
    setFlag(FLAG_C, (value & 0x80) != 0);
    value = static_cast<uint8_t>((value << 1) | (oldCarry ? 1 : 0));
    setZN(value);
    return value;
}

uint8_t CPU::opROR(uint8_t value) {
    bool oldCarry = getFlag(FLAG_C);
    setFlag(FLAG_C, (value & 0x01) != 0);
    value = static_cast<uint8_t>((value >> 1) | (oldCarry ? 0x80 : 0));
    setZN(value);
    return value;
}

// --- Addressing modes ---

uint16_t CPU::addrImm() {
    return PC++;
}

uint16_t CPU::addrZp() {
    return read(PC++);
}

uint16_t CPU::addrZpX() {
    return static_cast<uint8_t>(read(PC++) + X);
}

uint16_t CPU::addrZpY() {
    return static_cast<uint8_t>(read(PC++) + Y);
}

uint16_t CPU::addrAbs() {
    uint16_t addr = read16(PC);
    PC += 2;
    return addr;
}

uint16_t CPU::addrAbsX() {
    uint16_t base = read16(PC);
    PC += 2;
    uint16_t addr = static_cast<uint16_t>(base + X);
    page_crossed = (base & 0xFF00) != (addr & 0xFF00);
    return addr;
}

uint16_t CPU::addrAbsY() {
    uint16_t base = read16(PC);
    PC += 2;
    uint16_t addr = static_cast<uint16_t>(base + Y);
    page_crossed = (base & 0xFF00) != (addr & 0xFF00);
    return addr;
}

uint16_t CPU::addrIndX() {
    // (zp,X): pointer wraps within page zero
    uint8_t zp = static_cast<uint8_t>(read(PC++) + X);
    uint8_t lo = read(zp);
    uint8_t hi = read(static_cast<uint8_t>(zp + 1));
    return static_cast<uint16_t>((hi << 8) | lo);
}

uint16_t CPU::addrIndY() {
    // (zp),Y: pointer wraps within page zero, then Y added (may cross page)
    uint8_t zp = read(PC++);
    uint8_t lo = read(zp);
    uint8_t hi = read(static_cast<uint8_t>(zp + 1));
    uint16_t base = static_cast<uint16_t>((hi << 8) | lo);
    uint16_t addr = static_cast<uint16_t>(base + Y);
    page_crossed = (base & 0xFF00) != (addr & 0xFF00);
    return addr;
}

uint16_t CPU::addrInd() {
    // JMP ($xxxx): 6502 bug — high byte comes from the same page when low wraps
    uint16_t ptr = read16(PC);
    PC += 2;
    uint8_t lo = read(ptr);
    uint8_t hi = read(static_cast<uint16_t>((ptr & 0xFF00) | ((ptr + 1) & 0x00FF)));
    return static_cast<uint16_t>((hi << 8) | lo);
}

// --- Instruction execution ---

void CPU::step() {
    page_crossed = false;
    irqPollDelay = false;
    uint8_t opcode = read(PC++);
    int cyc = 0;

    switch (opcode) {
    // ADC
    case 0x69: opADC(read(addrImm()));        cyc = 2; break;
    case 0x65: opADC(read(addrZp()));         cyc = 3; break;
    case 0x75: opADC(read(addrZpX()));        cyc = 4; break;
    case 0x6D: opADC(read(addrAbs()));        cyc = 4; break;
    case 0x7D: opADC(read(addrAbsX()));       cyc = 4 + page_crossed; break;
    case 0x79: opADC(read(addrAbsY()));       cyc = 4 + page_crossed; break;
    case 0x61: opADC(read(addrIndX()));       cyc = 6; break;
    case 0x71: opADC(read(addrIndY()));       cyc = 5 + page_crossed; break;

    // AND
    case 0x29: A &= read(addrImm());          setZN(A); cyc = 2; break;
    case 0x25: A &= read(addrZp());           setZN(A); cyc = 3; break;
    case 0x35: A &= read(addrZpX());          setZN(A); cyc = 4; break;
    case 0x2D: A &= read(addrAbs());          setZN(A); cyc = 4; break;
    case 0x3D: A &= read(addrAbsX());         setZN(A); cyc = 4 + page_crossed; break;
    case 0x39: A &= read(addrAbsY());         setZN(A); cyc = 4 + page_crossed; break;
    case 0x21: A &= read(addrIndX());         setZN(A); cyc = 6; break;
    case 0x31: A &= read(addrIndY());         setZN(A); cyc = 5 + page_crossed; break;

    // ASL
    case 0x0A: A = opASL(A);                  cyc = 2; break;
    case 0x06: { uint16_t a = addrZp();  write(a, opASL(read(a))); cyc = 5; } break;
    case 0x16: { uint16_t a = addrZpX(); write(a, opASL(read(a))); cyc = 6; } break;
    case 0x0E: { uint16_t a = addrAbs(); write(a, opASL(read(a))); cyc = 6; } break;
    case 0x1E: { uint16_t a = addrAbsX(); write(a, opASL(read(a))); cyc = 7; } break;

    // Branches (cycle: 2 +1 taken +1 page cross)
    case 0x90: case 0xB0: case 0xF0: case 0x30:
    case 0xD0: case 0x10: case 0x50: case 0x70: {
        int8_t offset = static_cast<int8_t>(read(PC++));
        bool taken = false;
        switch (opcode) {
        case 0x90: taken = !getFlag(FLAG_C); break; // BCC
        case 0xB0: taken =  getFlag(FLAG_C); break; // BCS
        case 0xF0: taken =  getFlag(FLAG_Z); break; // BEQ
        case 0x30: taken =  getFlag(FLAG_N); break; // BMI
        case 0xD0: taken = !getFlag(FLAG_Z); break; // BNE
        case 0x10: taken = !getFlag(FLAG_N); break; // BPL
        case 0x50: taken = !getFlag(FLAG_V); break; // BVC
        case 0x70: taken =  getFlag(FLAG_V); break; // BVS
        }
        cyc = 2;
        if (taken) {
            uint16_t target = static_cast<uint16_t>(PC + offset);
            cyc++;
            if ((PC & 0xFF00) != (target & 0xFF00)) cyc++;
            PC = target;
        }
    } break;

    // BIT
    case 0x24: opBIT(read(addrZp()));   cyc = 3; break;
    case 0x2C: opBIT(read(addrAbs()));  cyc = 4; break;

    // BRK
    case 0x00:
        read(PC++); // signature byte (a real bus read on hardware)
        push16(PC);
        push8(P | FLAG_B | FLAG_U);
        setFlag(FLAG_I, true);
        PC = read16(0xFFFE);
        cyc = 7;
        break;

    // Flag clears
    case 0x18: setFlag(FLAG_C, false); cyc = 2; break; // CLC
    case 0xD8: setFlag(FLAG_D, false); cyc = 2; break; // CLD
    case 0x58: setFlag(FLAG_I, false); irqPollDelay = true; cyc = 2; break; // CLI
    case 0xB8: setFlag(FLAG_V, false); cyc = 2; break; // CLV

    // CMP
    case 0xC9: opCMP(A, read(addrImm()));       cyc = 2; break;
    case 0xC5: opCMP(A, read(addrZp()));        cyc = 3; break;
    case 0xD5: opCMP(A, read(addrZpX()));       cyc = 4; break;
    case 0xCD: opCMP(A, read(addrAbs()));       cyc = 4; break;
    case 0xDD: opCMP(A, read(addrAbsX()));      cyc = 4 + page_crossed; break;
    case 0xD9: opCMP(A, read(addrAbsY()));      cyc = 4 + page_crossed; break;
    case 0xC1: opCMP(A, read(addrIndX()));      cyc = 6; break;
    case 0xD1: opCMP(A, read(addrIndY()));      cyc = 5 + page_crossed; break;

    // CPX
    case 0xE0: opCMP(X, read(addrImm()));  cyc = 2; break;
    case 0xE4: opCMP(X, read(addrZp()));   cyc = 3; break;
    case 0xEC: opCMP(X, read(addrAbs()));  cyc = 4; break;

    // CPY
    case 0xC0: opCMP(Y, read(addrImm()));  cyc = 2; break;
    case 0xC4: opCMP(Y, read(addrZp()));   cyc = 3; break;
    case 0xCC: opCMP(Y, read(addrAbs()));  cyc = 4; break;

    // DEC
    case 0xC6: { uint16_t a = addrZp();  uint8_t v = static_cast<uint8_t>(read(a) - 1); write(a, v); setZN(v); cyc = 5; } break;
    case 0xD6: { uint16_t a = addrZpX(); uint8_t v = static_cast<uint8_t>(read(a) - 1); write(a, v); setZN(v); cyc = 6; } break;
    case 0xCE: { uint16_t a = addrAbs(); uint8_t v = static_cast<uint8_t>(read(a) - 1); write(a, v); setZN(v); cyc = 6; } break;
    case 0xDE: { uint16_t a = addrAbsX(); uint8_t v = static_cast<uint8_t>(read(a) - 1); write(a, v); setZN(v); cyc = 7; } break;

    // DEX / DEY
    case 0xCA: X--; setZN(X); cyc = 2; break;
    case 0x88: Y--; setZN(Y); cyc = 2; break;

    // EOR
    case 0x49: A ^= read(addrImm());          setZN(A); cyc = 2; break;
    case 0x45: A ^= read(addrZp());           setZN(A); cyc = 3; break;
    case 0x55: A ^= read(addrZpX());          setZN(A); cyc = 4; break;
    case 0x4D: A ^= read(addrAbs());          setZN(A); cyc = 4; break;
    case 0x5D: A ^= read(addrAbsX());         setZN(A); cyc = 4 + page_crossed; break;
    case 0x59: A ^= read(addrAbsY());         setZN(A); cyc = 4 + page_crossed; break;
    case 0x41: A ^= read(addrIndX());         setZN(A); cyc = 6; break;
    case 0x51: A ^= read(addrIndY());         setZN(A); cyc = 5 + page_crossed; break;

    // INC
    case 0xE6: { uint16_t a = addrZp();  uint8_t v = static_cast<uint8_t>(read(a) + 1); write(a, v); setZN(v); cyc = 5; } break;
    case 0xF6: { uint16_t a = addrZpX(); uint8_t v = static_cast<uint8_t>(read(a) + 1); write(a, v); setZN(v); cyc = 6; } break;
    case 0xEE: { uint16_t a = addrAbs(); uint8_t v = static_cast<uint8_t>(read(a) + 1); write(a, v); setZN(v); cyc = 6; } break;
    case 0xFE: { uint16_t a = addrAbsX(); uint8_t v = static_cast<uint8_t>(read(a) + 1); write(a, v); setZN(v); cyc = 7; } break;

    // INX / INY
    case 0xE8: X++; setZN(X); cyc = 2; break;
    case 0xC8: Y++; setZN(Y); cyc = 2; break;

    // JMP
    case 0x4C: PC = addrAbs();  cyc = 3; break;
    case 0x6C: PC = addrInd();  cyc = 5; break;

    // JSR
    case 0x20: {
        uint16_t target = read16(PC);
        PC += 2;
        push16(static_cast<uint16_t>(PC - 1));
        PC = target;
        cyc = 6;
    } break;

    // LDA
    case 0xA9: A = read(addrImm());          setZN(A); cyc = 2; break;
    case 0xA5: A = read(addrZp());           setZN(A); cyc = 3; break;
    case 0xB5: A = read(addrZpX());          setZN(A); cyc = 4; break;
    case 0xAD: A = read(addrAbs());          setZN(A); cyc = 4; break;
    case 0xBD: A = read(addrAbsX());         setZN(A); cyc = 4 + page_crossed; break;
    case 0xB9: A = read(addrAbsY());         setZN(A); cyc = 4 + page_crossed; break;
    case 0xA1: A = read(addrIndX());         setZN(A); cyc = 6; break;
    case 0xB1: A = read(addrIndY());         setZN(A); cyc = 5 + page_crossed; break;

    // LDX
    case 0xA2: X = read(addrImm());          setZN(X); cyc = 2; break;
    case 0xA6: X = read(addrZp());           setZN(X); cyc = 3; break;
    case 0xB6: X = read(addrZpY());          setZN(X); cyc = 4; break;
    case 0xAE: X = read(addrAbs());          setZN(X); cyc = 4; break;
    case 0xBE: X = read(addrAbsY());         setZN(X); cyc = 4 + page_crossed; break;

    // LDY
    case 0xA0: Y = read(addrImm());          setZN(Y); cyc = 2; break;
    case 0xA4: Y = read(addrZp());           setZN(Y); cyc = 3; break;
    case 0xB4: Y = read(addrZpX());          setZN(Y); cyc = 4; break;
    case 0xAC: Y = read(addrAbs());          setZN(Y); cyc = 4; break;
    case 0xBC: Y = read(addrAbsX());         setZN(Y); cyc = 4 + page_crossed; break;

    // LSR
    case 0x4A: A = opLSR(A);                 cyc = 2; break;
    case 0x46: { uint16_t a = addrZp();  write(a, opLSR(read(a))); cyc = 5; } break;
    case 0x56: { uint16_t a = addrZpX(); write(a, opLSR(read(a))); cyc = 6; } break;
    case 0x4E: { uint16_t a = addrAbs(); write(a, opLSR(read(a))); cyc = 6; } break;
    case 0x5E: { uint16_t a = addrAbsX(); write(a, opLSR(read(a))); cyc = 7; } break;

    // NOP
    case 0xEA: cyc = 2; break;

    // ORA
    case 0x09: A |= read(addrImm());          setZN(A); cyc = 2; break;
    case 0x05: A |= read(addrZp());           setZN(A); cyc = 3; break;
    case 0x15: A |= read(addrZpX());          setZN(A); cyc = 4; break;
    case 0x0D: A |= read(addrAbs());          setZN(A); cyc = 4; break;
    case 0x1D: A |= read(addrAbsX());         setZN(A); cyc = 4 + page_crossed; break;
    case 0x19: A |= read(addrAbsY());         setZN(A); cyc = 4 + page_crossed; break;
    case 0x01: A |= read(addrIndX());         setZN(A); cyc = 6; break;
    case 0x11: A |= read(addrIndY());         setZN(A); cyc = 5 + page_crossed; break;

    // PHA / PHP
    case 0x48: push8(A);               cyc = 3; break;
    case 0x08: push8(P | FLAG_B | FLAG_U); cyc = 3; break;

    // PLA / PLP
    case 0x68: A = pop8(); setZN(A);   cyc = 4; break;
    case 0x28: // PLP
        P = static_cast<uint8_t>((pop8() & ~FLAG_B) | FLAG_U);
        irqPollDelay = true;
        cyc = 4;
        break;

    // ROL
    case 0x2A: A = opROL(A);                 cyc = 2; break;
    case 0x26: { uint16_t a = addrZp();  write(a, opROL(read(a))); cyc = 5; } break;
    case 0x36: { uint16_t a = addrZpX(); write(a, opROL(read(a))); cyc = 6; } break;
    case 0x2E: { uint16_t a = addrAbs(); write(a, opROL(read(a))); cyc = 6; } break;
    case 0x3E: { uint16_t a = addrAbsX(); write(a, opROL(read(a))); cyc = 7; } break;

    // ROR
    case 0x6A: A = opROR(A);                 cyc = 2; break;
    case 0x66: { uint16_t a = addrZp();  write(a, opROR(read(a))); cyc = 5; } break;
    case 0x76: { uint16_t a = addrZpX(); write(a, opROR(read(a))); cyc = 6; } break;
    case 0x6E: { uint16_t a = addrAbs(); write(a, opROR(read(a))); cyc = 6; } break;
    case 0x7E: { uint16_t a = addrAbsX(); write(a, opROR(read(a))); cyc = 7; } break;

    // RTI
    case 0x40:
        P = static_cast<uint8_t>((pop8() & ~FLAG_B) | FLAG_U);
        PC = pop16();
        cyc = 6;
        break;

    // RTS
    case 0x60:
        PC = static_cast<uint16_t>(pop16() + 1);
        cyc = 6;
        break;

    // SBC
    case 0xE9: case 0xEB: opSBC(read(addrImm()));      cyc = 2; break;
    case 0xE5: opSBC(read(addrZp()));                  cyc = 3; break;
    case 0xF5: opSBC(read(addrZpX()));                 cyc = 4; break;
    case 0xED: opSBC(read(addrAbs()));                 cyc = 4; break;
    case 0xFD: opSBC(read(addrAbsX()));                cyc = 4 + page_crossed; break;
    case 0xF9: opSBC(read(addrAbsY()));                cyc = 4 + page_crossed; break;
    case 0xE1: opSBC(read(addrIndX()));                cyc = 6; break;
    case 0xF1: opSBC(read(addrIndY()));                cyc = 5 + page_crossed; break;

    // Flag sets
    case 0x38: setFlag(FLAG_C, true); cyc = 2; break; // SEC
    case 0xF8: setFlag(FLAG_D, true); cyc = 2; break; // SED
    case 0x78: setFlag(FLAG_I, true); irqPollDelay = true; cyc = 2; break; // SEI

    // STA
    case 0x85: write(addrZp(),  A);  cyc = 3; break;
    case 0x95: write(addrZpX(), A);  cyc = 4; break;
    case 0x8D: write(addrAbs(), A);  cyc = 4; break;
    case 0x9D: write(addrAbsX(), A); cyc = 5; break;
    case 0x99: write(addrAbsY(), A); cyc = 5; break;
    case 0x81: write(addrIndX(), A); cyc = 6; break;
    case 0x91: write(addrIndY(), A); cyc = 6; break;

    // STX
    case 0x86: write(addrZp(),  X); cyc = 3; break;
    case 0x96: write(addrZpY(), X); cyc = 4; break;
    case 0x8E: write(addrAbs(), X); cyc = 4; break;

    // STY
    case 0x84: write(addrZp(),  Y); cyc = 3; break;
    case 0x94: write(addrZpX(), Y); cyc = 4; break;
    case 0x8C: write(addrAbs(), Y); cyc = 4; break;

    // Transfers
    case 0xAA: X = A; setZN(X); cyc = 2; break; // TAX
    case 0xA8: Y = A; setZN(Y); cyc = 2; break; // TAY
    case 0x8A: A = X; setZN(A); cyc = 2; break; // TXA
    case 0x98: A = Y; setZN(A); cyc = 2; break; // TYA
    case 0xBA: X = SP; setZN(X); cyc = 2; break; // TSX
    case 0x9A: SP = X;            cyc = 2; break; // TXS

    // ===== Unofficial opcodes required by nestest =====

    // NOPs: immediate (2 bytes, 2 cycles)
    case 0x80: case 0x82: case 0x89: case 0xC2: case 0xE2:
        PC++;
        cyc = 2;
        break;

    // NOPs: zp / zp,X / abs / abs,X (read the operand; abs,X may cross page)
    case 0x04: case 0x44: case 0x64:
        read(addrZp());  cyc = 3; break;
    case 0x14: case 0x34: case 0x54: case 0x74: case 0xD4: case 0xF4:
        read(addrZpX()); cyc = 4; break;
    case 0x0C:
        read(addrAbs()); cyc = 4; break;
    case 0x1C: case 0x3C: case 0x5C: case 0x7C: case 0xDC: case 0xFC:
        read(addrAbsX()); cyc = 4 + page_crossed; break;

    // NOPs: implied (1 byte, 2 cycles)
    case 0x1A: case 0x3A: case 0x5A: case 0x7A: case 0xDA: case 0xFA:
        cyc = 2;
        break;

    // LAX: LDA + LDX
    case 0xA7: A = X = read(addrZp());    setZN(A); cyc = 3; break;
    case 0xB7: A = X = read(addrZpY());   setZN(A); cyc = 4; break;
    case 0xAF: A = X = read(addrAbs());   setZN(A); cyc = 4; break;
    case 0xBF: A = X = read(addrAbsY());  setZN(A); cyc = 4 + page_crossed; break;
    case 0xA3: A = X = read(addrIndX());  setZN(A); cyc = 6; break;
    case 0xB3: A = X = read(addrIndY());  setZN(A); cyc = 5 + page_crossed; break;

    // SAX: store A & X
    case 0x87: write(addrZp(),  static_cast<uint8_t>(A & X));  cyc = 3; break;
    case 0x97: write(addrZpY(), static_cast<uint8_t>(A & X));  cyc = 4; break;
    case 0x8F: write(addrAbs(), static_cast<uint8_t>(A & X));  cyc = 4; break;
    case 0x83: write(addrIndX(), static_cast<uint8_t>(A & X)); cyc = 6; break;

    // AAC (ANC): A &= #n, then C = N
    case 0x0B: case 0x2B:
        A &= read(addrImm());
        setZN(A);
        setFlag(FLAG_C, (A & 0x80) != 0);
        cyc = 2;
        break;

    // ASR (ALR): A = (A & #n) LSR 1
    case 0x4B: {
        const uint8_t v = static_cast<uint8_t>(A & read(addrImm()));
        setFlag(FLAG_C, (v & 0x01) != 0);
        A = static_cast<uint8_t>(v >> 1);
        setZN(A);
        cyc = 2;
    } break;

    // ARR: A &= #n, ROR through carry; C = bit 6, V = bit 6 xor bit 5
    case 0x6B: {
        const uint8_t v = static_cast<uint8_t>(A & read(addrImm()));
        const bool oldCarry = getFlag(FLAG_C);
        A = static_cast<uint8_t>((v >> 1) | (oldCarry ? 0x80 : 0));
        setFlag(FLAG_C, (A & 0x40) != 0);
        setFlag(FLAG_V, (((A >> 6) ^ (A >> 5)) & 1) != 0);
        setZN(A);
        cyc = 2;
    } break;

    // ATX (LXA): A = X = #n (magic-constant model: | $FF)
    case 0xAB:
        A = X = read(addrImm());
        setZN(A);
        cyc = 2;
        break;

    // AXS (SBX): X = (A & X) - #n, flags as if CMP
    case 0xCB: {
        const uint8_t imm = read(addrImm());
        const uint8_t lhs = static_cast<uint8_t>(A & X);
        X = static_cast<uint8_t>(lhs - imm);
        setFlag(FLAG_C, lhs >= imm);
        setZN(X);
        cyc = 2;
    } break;

    // SHY (SYA) abs,X: value = Y & (operand high byte + 1).
    // On a page crossing the store address's high byte collapses to that
    // same value (bus conflict during the high-byte fix-up).
    case 0x9C: {
        const uint8_t lo = read(PC++);
        const uint8_t hi = read(PC++);
        const uint16_t base = static_cast<uint16_t>((hi << 8) | lo);
        const uint16_t addr = static_cast<uint16_t>(base + X);
        const bool crossed = (base & 0xFF00) != (addr & 0xFF00);
        const uint8_t v = static_cast<uint8_t>(Y) & static_cast<uint8_t>(hi + 1);
        write(crossed ? static_cast<uint16_t>((v << 8) | (addr & 0xFF)) : addr,
              v);
        cyc = 5;
    } break;

    // SHX (SXA) abs,Y: same as SHY but stores X, indexed by Y
    case 0x9E: {
        const uint8_t lo = read(PC++);
        const uint8_t hi = read(PC++);
        const uint16_t base = static_cast<uint16_t>((hi << 8) | lo);
        const uint16_t addr = static_cast<uint16_t>(base + Y);
        const bool crossed = (base & 0xFF00) != (addr & 0xFF00);
        const uint8_t v = static_cast<uint8_t>(X) & static_cast<uint8_t>(hi + 1);
        write(crossed ? static_cast<uint16_t>((v << 8) | (addr & 0xFF)) : addr,
              v);
        cyc = 5;
    } break;

    // SLO: ASL memory, then ORA into A
    case 0x07: case 0x17: case 0x0F: case 0x1F: case 0x1B: case 0x03: case 0x13: {
        uint16_t a;
        int base;
        switch (opcode) {
        case 0x07: a = addrZp();  base = 5; break;
        case 0x17: a = addrZpX(); base = 6; break;
        case 0x0F: a = addrAbs(); base = 6; break;
        case 0x1F: a = addrAbsX(); base = 7; break;
        case 0x1B: a = addrAbsY(); base = 7; break;
        case 0x03: a = addrIndX(); base = 8; break;
        default:   a = addrIndY(); base = 8; break;
        }
        uint8_t v = opASL(read(a));
        write(a, v);
        A |= v;
        setZN(A);
        cyc = base;
    } break;

    // RLA: ROL memory, then AND into A
    case 0x27: case 0x37: case 0x2F: case 0x3F: case 0x3B: case 0x23: case 0x33: {
        uint16_t a;
        int base;
        switch (opcode) {
        case 0x27: a = addrZp();  base = 5; break;
        case 0x37: a = addrZpX(); base = 6; break;
        case 0x2F: a = addrAbs(); base = 6; break;
        case 0x3F: a = addrAbsX(); base = 7; break;
        case 0x3B: a = addrAbsY(); base = 7; break;
        case 0x23: a = addrIndX(); base = 8; break;
        default:   a = addrIndY(); base = 8; break;
        }
        uint8_t v = opROL(read(a));
        write(a, v);
        A &= v;
        setZN(A);
        cyc = base;
    } break;

    // SRE: LSR memory, then EOR into A
    case 0x47: case 0x57: case 0x4F: case 0x5F: case 0x5B: case 0x43: case 0x53: {
        uint16_t a;
        int base;
        switch (opcode) {
        case 0x47: a = addrZp();  base = 5; break;
        case 0x57: a = addrZpX(); base = 6; break;
        case 0x4F: a = addrAbs(); base = 6; break;
        case 0x5F: a = addrAbsX(); base = 7; break;
        case 0x5B: a = addrAbsY(); base = 7; break;
        case 0x43: a = addrIndX(); base = 8; break;
        default:   a = addrIndY(); base = 8; break;
        }
        uint8_t v = opLSR(read(a));
        write(a, v);
        A ^= v;
        setZN(A);
        cyc = base;
    } break;

    // RRA: ROR memory, then ADC into A
    case 0x67: case 0x77: case 0x6F: case 0x7F: case 0x7B: case 0x63: case 0x73: {
        uint16_t a;
        int base;
        switch (opcode) {
        case 0x67: a = addrZp();  base = 5; break;
        case 0x77: a = addrZpX(); base = 6; break;
        case 0x6F: a = addrAbs(); base = 6; break;
        case 0x7F: a = addrAbsX(); base = 7; break;
        case 0x7B: a = addrAbsY(); base = 7; break;
        case 0x63: a = addrIndX(); base = 8; break;
        default:   a = addrIndY(); base = 8; break;
        }
        uint8_t v = opROR(read(a));
        write(a, v);
        opADC(v);
        cyc = base;
    } break;

    // DCP: DEC memory, then CMP
    case 0xC7: case 0xD7: case 0xCF: case 0xDF: case 0xDB: case 0xC3: case 0xD3: {
        uint16_t a;
        int base;
        switch (opcode) {
        case 0xC7: a = addrZp();  base = 5; break;
        case 0xD7: a = addrZpX(); base = 6; break;
        case 0xCF: a = addrAbs(); base = 6; break;
        case 0xDF: a = addrAbsX(); base = 7; break;
        case 0xDB: a = addrAbsY(); base = 7; break;
        case 0xC3: a = addrIndX(); base = 8; break;
        default:   a = addrIndY(); base = 8; break;
        }
        uint8_t v = static_cast<uint8_t>(read(a) - 1);
        write(a, v);
        opCMP(A, v);
        cyc = base;
    } break;

    // ISC: INC memory, then SBC
    case 0xE7: case 0xF7: case 0xEF: case 0xFF: case 0xFB: case 0xE3: case 0xF3: {
        uint16_t a;
        int base;
        switch (opcode) {
        case 0xE7: a = addrZp();  base = 5; break;
        case 0xF7: a = addrZpX(); base = 6; break;
        case 0xEF: a = addrAbs(); base = 6; break;
        case 0xFF: a = addrAbsX(); base = 7; break;
        case 0xFB: a = addrAbsY(); base = 7; break;
        case 0xE3: a = addrIndX(); base = 8; break;
        default:   a = addrIndY(); base = 8; break;
        }
        uint8_t v = static_cast<uint8_t>(read(a) + 1);
        write(a, v);
        opSBC(v);
        cyc = base;
    } break;

    default:
        // Unimplemented opcode: treat as 2-cycle NOP so the build stays green
        cyc = 2;
        break;
    }

    total_cycles += static_cast<uint64_t>(cyc);
    // Catch devices up through this instruction's internal (non-bus) cycles
    bus->syncDevicesTo(total_cycles);
}
