#include "../include/Bus.hpp"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

static int failures = 0;

static void expect(bool cond, const char* name) {
    if (cond) {
        std::printf("[PASS] %s\n", name);
    } else {
        std::printf("[FAIL] %s\n", name);
        ++failures;
    }
}

// 64KB PRG (4 × 16KB) filled with its 16KB bank index,
// 32KB CHR (8 × 4KB) filled with its 4KB bank index, mapper 1.
static std::unique_ptr<Cartridge> makeMmc1Cart() {
    std::vector<uint8_t> img(16 + 65536 + 32768, 0);
    img[0] = 'N'; img[1] = 'E'; img[2] = 'S'; img[3] = 0x1A;
    img[4] = 4;   // 64KB PRG
    img[5] = 4;   // 32KB CHR
    img[6] = (1 << 4) & 0xF0;
    img[7] = 1 & 0xF0;
    for (size_t i = 0; i < 65536; i++) {
        img[16 + i] = static_cast<uint8_t>(i / 16384);
    }
    for (size_t i = 0; i < 32768; i++) {
        img[16 + 65536 + i] = static_cast<uint8_t>(i / 4096);
    }

    const char* path = "/tmp/nes_mmc1.nes";
    FILE* f = std::fopen(path, "wb");
    if (!f) return nullptr;
    std::fwrite(img.data(), 1, img.size(), f);
    std::fclose(f);

    std::string err;
    auto cart = Cartridge::load(path, &err);
    if (!cart) {
        std::printf("[FAIL] MMC1 fixture load: %s\n", err.c_str());
        ++failures;
    }
    return cart;
}

static void newMmc1Bus(Bus& bus) {
    std::string err;
    auto cart = makeMmc1Cart();
    if (!cart || !bus.insertCartridge(std::move(cart), &err)) {
        std::printf("[FAIL] MMC1 insertCartridge: %s\n", err.c_str());
        ++failures;
    }
}

// Five serial writes, LSB first; the fifth write's address selects the register.
static void mmc1Write(Bus& bus, uint16_t addr, uint8_t value) {
    for (int i = 0; i < 5; i++) {
        bus.cpuWrite(addr, static_cast<uint8_t>((value >> i) & 1));
    }
}

static void testResetStateIsMode3() {
    Bus bus; newMmc1Bus(bus);
    // control resets to 0x0C → PRG mode 3: switch $8000, fix last at $C000
    mmc1Write(bus, 0xE000, 1); // PRG bank 1
    expect(bus.cpuRead(0x8000) == 1, "reset: $8000 follows PRG bank (mode 3)");
    expect(bus.cpuRead(0xC000) == 3, "reset: $C000 fixed to last 16KB bank");
}

static void testSerialWriteRequiresFive() {
    Bus bus; newMmc1Bus(bus);
    mmc1Write(bus, 0xE000, 1);
    // Four writes of 0 must not commit the register
    for (int i = 0; i < 4; i++) bus.cpuWrite(0xE000, 0);
    expect(bus.cpuRead(0x8000) == 1, "4 serial writes do not commit");
    bus.cpuWrite(0xE000, 0); // fifth write commits 0
    expect(bus.cpuRead(0x8000) == 0, "5th serial write commits register");
}

static void testBit7ResetsShiftAndForcesMode3() {
    Bus bus; newMmc1Bus(bus);
    // Mode 0 (32KB), PRG bank 1 → banks 0,1
    mmc1Write(bus, 0x8000, 0x00);
    mmc1Write(bus, 0xE000, 0x01);
    expect(bus.cpuRead(0x8000) == 0 && bus.cpuRead(0xC000) == 1,
           "mode 0 maps 32KB banks 0,1");

    bus.cpuWrite(0x8000, 0x80); // reset: control |= 0x0C → mode 3
    expect(bus.cpuRead(0x8000) == 1, "bit7 write forces PRG mode 3");
    expect(bus.cpuRead(0xC000) == 3, "mode 3 fixes last bank at $C000");

    // Shift reset: a single write must not commit
    bus.cpuWrite(0xE000, 0);
    expect(bus.cpuRead(0x8000) == 1, "shift reset clears partial writes");
}

static void testPrgMode0And1Are32k() {
    {
        Bus bus; newMmc1Bus(bus);
        mmc1Write(bus, 0x8000, 0x00); // mode 0
        mmc1Write(bus, 0xE000, 0x02);
        expect(bus.cpuRead(0x8000) == 2 && bus.cpuRead(0xC000) == 3,
               "mode 0: 32KB switch at $8000 (banks 2,3)");
    }
    {
        Bus bus; newMmc1Bus(bus);
        mmc1Write(bus, 0x8000, 0x04); // mode 1
        mmc1Write(bus, 0xE000, 0x00);
        expect(bus.cpuRead(0x8000) == 0 && bus.cpuRead(0xC000) == 1,
               "mode 1: 32KB switch at $8000 (banks 0,1)");
    }
}

static void testPrgMode2FixesFirstBank() {
    Bus bus; newMmc1Bus(bus);
    mmc1Write(bus, 0x8000, 0x08); // mode 2
    mmc1Write(bus, 0xE000, 0x02);
    expect(bus.cpuRead(0x8000) == 0, "mode 2: $8000 fixed to first bank");
    expect(bus.cpuRead(0xC000) == 2, "mode 2: $C000 switchable");
}

static void testChr4kBanking() {
    Bus bus; newMmc1Bus(bus);
    PPU& ppu = bus.getPPU();
    // bit4 set → 4KB mode, PRG mode 3
    mmc1Write(bus, 0x8000, 0x1C);
    mmc1Write(bus, 0xA000, 0x03); // CHR bank 0 → $0000
    mmc1Write(bus, 0xC000, 0x05); // CHR bank 1 → $1000

    expect(ppu.ppuRead(0x0000) == 3, "4KB mode: $0000 maps CHR bank 3");
    expect(ppu.ppuRead(0x0FFF) == 3, "4KB mode: $0FFF still bank 3");
    expect(ppu.ppuRead(0x1000) == 5, "4KB mode: $1000 maps CHR bank 5");
    expect(ppu.ppuRead(0x1FFF) == 5, "4KB mode: $1FFF still bank 5");
}

static void testChr8kUsesOnlyBank0() {
    Bus bus; newMmc1Bus(bus);
    PPU& ppu = bus.getPPU();
    mmc1Write(bus, 0x8000, 0x0C); // bit4 clear → 8KB mode
    mmc1Write(bus, 0xA000, 0x03); // R0 = 3 → 8KB base (3 & 0x1E) = 2
    mmc1Write(bus, 0xC000, 0x07); // R1 ignored in 8KB mode

    expect(ppu.ppuRead(0x0000) == 2, "8KB mode: $0000 = R0 base");
    expect(ppu.ppuRead(0x0FFF) == 2, "8KB mode: $0FFF = R0 base");
    expect(ppu.ppuRead(0x1000) == 3, "8KB mode: $1000 = R0 base + 4KB");
    expect(ppu.ppuRead(0x1FFF) == 3, "8KB mode: $1FFF = R0 base + 4KB");
}

static void testMirroringModes() {
    {
        Bus bus; newMmc1Bus(bus);
        PPU& ppu = bus.getPPU();
        mmc1Write(bus, 0x8000, 0x00); // mirroring 0 → single-screen low
        ppu.ppuWrite(0x2000, 0x42);
        expect(ppu.ppuRead(0x2400) == 0x42 && ppu.ppuRead(0x2800) == 0x42 &&
                   ppu.ppuRead(0x2C00) == 0x42,
               "control 0: single-screen low");
        expect(ppu.mirrorMode() == PPU::Mirror::SingleLow,
               "control 0 sets Mirror::SingleLow");
    }
    {
        Bus bus; newMmc1Bus(bus);
        PPU& ppu = bus.getPPU();
        mmc1Write(bus, 0x8000, 0x01); // single-screen high
        ppu.ppuWrite(0x2400, 0x11);
        expect(ppu.ppuRead(0x2000) == 0x11 && ppu.ppuRead(0x2800) == 0x11,
               "control 1: single-screen high");
        expect(ppu.mirrorMode() == PPU::Mirror::SingleHigh,
               "control 1 sets Mirror::SingleHigh");
    }
    {
        Bus bus; newMmc1Bus(bus);
        PPU& ppu = bus.getPPU();
        mmc1Write(bus, 0x8000, 0x02); // vertical arrangement
        ppu.ppuWrite(0x2000, 0x33);
        ppu.ppuWrite(0x2400, 0x44);
        expect(ppu.ppuRead(0x2800) == 0x33, "control 2: $2800 mirrors $2000");
        expect(ppu.ppuRead(0x2C00) == 0x44, "control 2: $2C00 mirrors $2400");
        expect(ppu.ppuRead(0x2400) == 0x44 && ppu.ppuRead(0x2000) == 0x33,
               "control 2: vertical keeps $2000/$2400 distinct");
        expect(ppu.mirrorMode() == PPU::Mirror::Vertical,
               "control 2 sets Mirror::Vertical");
    }
    {
        Bus bus; newMmc1Bus(bus);
        PPU& ppu = bus.getPPU();
        mmc1Write(bus, 0x8000, 0x03); // horizontal arrangement
        ppu.ppuWrite(0x2000, 0x55);
        ppu.ppuWrite(0x2800, 0x66);
        expect(ppu.ppuRead(0x2400) == 0x55, "control 3: $2400 mirrors $2000");
        expect(ppu.ppuRead(0x2C00) == 0x66, "control 3: $2C00 mirrors $2800");
        expect(ppu.ppuRead(0x2800) == 0x66 && ppu.ppuRead(0x2000) == 0x55,
               "control 3: horizontal keeps rows distinct");
        expect(ppu.mirrorMode() == PPU::Mirror::Horizontal,
               "control 3 sets Mirror::Horizontal");
    }
}

static void testPrgRam() {
    Bus bus; newMmc1Bus(bus);
    bus.cpuWrite(0x6000, 0xAB);
    bus.cpuWrite(0x7FFF, 0xCD);
    expect(bus.cpuRead(0x6000) == 0xAB, "PRG RAM $6000 writable");
    expect(bus.cpuRead(0x7FFF) == 0xCD, "PRG RAM $7FFF writable");
}

void run_mmc1_tests() {
    std::printf("\nRunning MMC1 (mapper 1) tests...\n");
    testResetStateIsMode3();
    testSerialWriteRequiresFive();
    testBit7ResetsShiftAndForcesMode3();
    testPrgMode0And1Are32k();
    testPrgMode2FixesFirstBank();
    testChr4kBanking();
    testChr8kUsesOnlyBank0();
    testMirroringModes();
    testPrgRam();
    if (failures == 0) {
        std::printf("MMC1 tests passed!\n");
    } else {
        std::printf("MMC1 tests: %d FAILURES\n", failures);
        std::exit(1);
    }
}
