#include <cassert>
#include <iostream>
#include <string>
#include "Bus.hpp"
#include "Cartridge.hpp"
#include "CPU.hpp"

#ifndef NES_SOURCE_DIR
#define NES_SOURCE_DIR "."
#endif

static std::string fixture(const char* name) {
    return std::string(NES_SOURCE_DIR) + "/tests/fixtures/" + name;
}

// Header fields: nestest is 1×16KB PRG, 1×8KB CHR, mapper 0, horizontal mirror
void testHeaderParse() {
    std::string err;
    auto cart = Cartridge::load(fixture("nestest.nes"), &err);
    assert(cart && "nestest.nes must load");

    assert(cart->mapperId() == 0);
    assert(cart->prgRom().size() == 16384);
    assert(cart->chrRom().size() == 8192);
    assert(!cart->verticalMirroring());
    assert(!cart->batteryBacked());
    assert(!cart->hasTrainer());
    assert(cart->prgRamSize() == 8192); // header byte 8 == 0 → assume 8KB
    std::cout << "[PASS] iNES header parse" << std::endl;
}

void testRejectsBadFiles() {
    std::string err;
    auto missing = Cartridge::load(fixture("does_not_exist.nes"), &err);
    assert(!missing);
    assert(err.find("cannot open") != std::string::npos);

    // Non-iNES bytes
    const char* junkPath = "/tmp/nes_bad_header.nes";
    FILE* f = fopen(junkPath, "wb");
    assert(f);
    fwrite("NOT_AN_ROM_FILE_XXXX", 1, 20, f);
    fclose(f);

    err.clear();
    auto junk = Cartridge::load(junkPath, &err);
    assert(!junk);
    assert(err.find("magic") != std::string::npos);
    std::cout << "[PASS] reject invalid files" << std::endl;
}

// 16KB NROM: PRG appears at both 0x8000 and 0xC000 (mirror)
void testNrom16kMirror() {
    std::string err;
    auto cart = Cartridge::load(fixture("nestest.nes"), &err);
    assert(cart);

    Bus bus;
    assert(bus.insertCartridge(std::move(cart), &err));

    // First PRG byte should be visible through both windows
    uint8_t lo = bus.cpuRead(0x8000);
    uint8_t hi = bus.cpuRead(0xC000);
    assert(lo == hi);

    // Offset 0x0004 in nestest's PRG (0x8004 / 0xC004) — same physical byte
    assert(bus.cpuRead(0x8004) == bus.cpuRead(0xC004));

    // Reset vector lives in PRG at file offset (0xFFFC & 0x3FFF) = 0x3FFC
    const auto& prg = bus.getCartridge()->prgRom();
    uint16_t reset =
        static_cast<uint16_t>(prg[0x3FFC] | (prg[0x3FFD] << 8));
    uint16_t viaBus =
        static_cast<uint16_t>(bus.cpuRead(0xFFFC) | (bus.cpuRead(0xFFFD) << 8));
    assert(reset == viaBus);
    std::cout << "[PASS] NROM-128 PRG mirror + reset vector" << std::endl;
}

// PRG ROM ignores writes; PRG RAM at 0x6000 is read/write
void testPrgRamAndRomWrites() {
    std::string err;
    auto cart = Cartridge::load(fixture("nestest.nes"), &err);
    assert(cart);

    Bus bus;
    assert(bus.insertCartridge(std::move(cart), &err));

    uint8_t before = bus.cpuRead(0x8000);
    bus.cpuWrite(0x8000, static_cast<uint8_t>(before ^ 0xFF));
    assert(bus.cpuRead(0x8000) == before); // ROM write ignored

    bus.cpuWrite(0x6000, 0xAB);
    bus.cpuWrite(0x7FFF, 0xCD);
    assert(bus.cpuRead(0x6000) == 0xAB);
    assert(bus.cpuRead(0x7FFF) == 0xCD);
    assert(bus.cpuRead(0x6001) == 0x00); // fresh PRG RAM
    std::cout << "[PASS] PRG RAM r/w, PRG ROM write-protect" << std::endl;
}

// Unsupported mapper must be rejected, not crash
void testUnsupportedMapper() {
    // Build a minimal iNES with mapper 4 (MMC3) in the header
    const char* path = "/tmp/nes_mmc3.nes";
    FILE* f = fopen(path, "wb");
    assert(f);
    uint8_t header[16] = {};
    header[0] = 'N'; header[1] = 'E'; header[2] = 'S'; header[3] = 0x1A;
    header[4] = 2; // 32KB PRG
    header[5] = 1; // 8KB CHR
    header[6] = 0x40; // mapper number bits 0–3 = 4
    header[7] = 0x00; // mapper number bits 4–7 = 0 → mapper 4
    fwrite(header, 1, 16, f);
    uint8_t zeros[16384 * 2 + 8192] = {};
    fwrite(zeros, 1, sizeof(zeros), f);
    fclose(f);

    std::string err;
    auto cart = Cartridge::load(path, &err);
    assert(cart);
    assert(cart->mapperId() == 4);

    Bus bus;
    assert(!bus.insertCartridge(std::move(cart), &err));
    assert(err.find("unsupported mapper") != std::string::npos);
    assert(!bus.hasCartridge());
    std::cout << "[PASS] reject unsupported mapper" << std::endl;
}

// CPU reset vector still works through the real cartridge
void testResetVectorThroughMapper() {
    std::string err;
    auto cart = Cartridge::load(fixture("nestest.nes"), &err);
    assert(cart);

    Bus bus;
    assert(bus.insertCartridge(std::move(cart), &err));
    CPU cpu(bus);
    cpu.reset();

    const auto& prg = bus.getCartridge()->prgRom();
    uint16_t expected =
        static_cast<uint16_t>(prg[0x3FFC] | (prg[0x3FFD] << 8));
    assert(cpu.PC == expected);
    assert(cpu.SP == 0xFD);
    assert(cpu.P == (CPU::FLAG_U | CPU::FLAG_I));
    std::cout << "[PASS] reset vector via Mapper 0 (PC=0x" << std::hex
              << cpu.PC << std::dec << ")" << std::endl;
}

void run_cartridge_tests() {
    std::cout << "Running Cartridge/Mapper tests..." << std::endl;
    testHeaderParse();
    testRejectsBadFiles();
    testNrom16kMirror();
    testPrgRamAndRomWrites();
    testUnsupportedMapper();
    testResetVectorThroughMapper();
    std::cout << "Cartridge/Mapper tests passed!" << std::endl;
}
