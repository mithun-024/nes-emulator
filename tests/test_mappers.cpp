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

// Builds an iNES image: PRG filled with its bank index at the given
// granularity, CHR likewise. chrUnits == 0 → CHR RAM.
static std::unique_ptr<Cartridge> makeCart(int mapper, int prgUnits,
                                           int chrUnits,
                                           size_t prgFillBytes = 16384,
                                           size_t chrFillBytes = 8192) {
    std::vector<uint8_t> img(16 + prgUnits * 16384 + chrUnits * 8192, 0);
    img[0] = 'N'; img[1] = 'E'; img[2] = 'S'; img[3] = 0x1A;
    img[4] = static_cast<uint8_t>(prgUnits);
    img[5] = static_cast<uint8_t>(chrUnits);
    img[6] = static_cast<uint8_t>((mapper & 0x0F) << 4);
    img[7] = static_cast<uint8_t>(mapper & 0xF0);

    for (size_t i = 0; i < static_cast<size_t>(prgUnits) * 16384; i++) {
        img[16 + i] = static_cast<uint8_t>(i / prgFillBytes);
    }
    for (size_t i = 0; i < static_cast<size_t>(chrUnits) * 8192; i++) {
        img[16 + prgUnits * 16384 + i] = static_cast<uint8_t>(i / chrFillBytes);
    }

    static int counter = 0;
    const std::string path = "/tmp/nes_mapper_fixture_" +
                             std::to_string(counter++) + ".nes";
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return nullptr;
    std::fwrite(img.data(), 1, img.size(), f);
    std::fclose(f);

    std::string err;
    auto cart = Cartridge::load(path, &err);
    if (!cart) {
        std::printf("[FAIL] fixture load: %s\n", err.c_str());
        ++failures;
    }
    return cart;
}

static bool insert(Bus& bus, std::unique_ptr<Cartridge> cart) {
    std::string err;
    if (!cart || !bus.insertCartridge(std::move(cart), &err)) {
        std::printf("[FAIL] insertCartridge: %s\n", err.c_str());
        ++failures;
        return false;
    }
    return true;
}

// ---------------------------------------------------------------- UxROM

static void testUxromBankSwitch() {
    Bus bus;
    if (!insert(bus, makeCart(2, 4, 0))) return; // 64KB PRG, CHR RAM

    expect(bus.cpuRead(0x8000) == 0, "UxROM: starts in bank 0");
    expect(bus.cpuRead(0xC000) == 3, "UxROM: last bank fixed at $C000");

    bus.cpuWrite(0x8000, 2);
    expect(bus.cpuRead(0x8000) == 2, "UxROM: write selects $8000 bank");
    expect(bus.cpuRead(0xBFFF) == 2, "UxROM: bank covers $BFFF");
    expect(bus.cpuRead(0xC000) == 3, "UxROM: fixed bank unaffected");

    bus.cpuWrite(0xFFFF, 1);
    expect(bus.cpuRead(0x8000) == 1, "UxROM: write at $FFFF also switches");

    bus.cpuWrite(0x8000, 0xFF); // out of range → wraps to a valid bank
    expect(bus.cpuRead(0x8000) == 0xFF % 4, "UxROM: bank index wraps");
}

static void testUxromChrRam() {
    Bus bus;
    if (!insert(bus, makeCart(2, 2, 0))) return;
    PPU& ppu = bus.getPPU();

    ppu.ppuWrite(0x0000, 0x5A);
    ppu.ppuWrite(0x1FFF, 0xA5);
    expect(ppu.ppuRead(0x0000) == 0x5A, "UxROM: CHR RAM write/read");
    expect(ppu.ppuRead(0x1FFF) == 0xA5, "UxROM: CHR RAM top byte");
}

static void testUxromHasNoPrgRam() {
    Bus bus;
    if (!insert(bus, makeCart(2, 2, 0))) return;
    bus.cpuWrite(0x6000, 0xAB);
    expect(bus.cpuRead(0x6000) == 0x00, "UxROM: no PRG RAM at $6000");
}

// ---------------------------------------------------------------- CNROM

static void testCnromChrBankSwitch() {
    Bus bus;
    if (!insert(bus, makeCart(3, 2, 4))) return; // 32KB PRG, 32KB CHR
    PPU& ppu = bus.getPPU();

    expect(ppu.ppuRead(0x0000) == 0, "CNROM: CHR bank 0 after reset");
    bus.cpuWrite(0x8000, 1);
    expect(ppu.ppuRead(0x0000) == 1, "CNROM: write selects CHR bank");
    expect(ppu.ppuRead(0x1FFF) == 1, "CNROM: 8KB bank spans both tables");

    bus.cpuWrite(0x8000, 3);
    expect(ppu.ppuRead(0x0FFF) == 3, "CNROM: bank 3 selected");
    bus.cpuWrite(0x8000, 4); // only 4 banks (0–3)
    expect(ppu.ppuRead(0x0000) == 0, "CNROM: bank index wraps");

    // PRG side behaves like 32KB NROM and is unaffected by CHR writes
    expect(bus.cpuRead(0x8000) == 0 && bus.cpuRead(0xC000) == 1,
           "CNROM: 32KB PRG linear");
}

static void testCnrom16kPrgMirror() {
    Bus bus;
    if (!insert(bus, makeCart(3, 1, 2))) return; // 16KB PRG
    expect(bus.cpuRead(0x8000) == bus.cpuRead(0xC000),
           "CNROM: 16KB PRG mirrored at $C000");
}

// ---------------------------------------------------------------- AxROM

static void testAxromPrgSwitch() {
    Bus bus;
    // 128KB PRG = 4 × 32KB banks; fill with the 32KB bank index
    if (!insert(bus, makeCart(7, 8, 0, 32768))) return;

    expect(bus.cpuRead(0x8000) == 0, "AxROM: starts in bank 0");
    expect(bus.cpuRead(0xC000) == 0, "AxROM: $C000 shares the 32KB bank");

    bus.cpuWrite(0x8000, 2);
    expect(bus.cpuRead(0x8000) == 2 && bus.cpuRead(0xC000) == 2,
           "AxROM: one write switches the whole 32KB window");
    expect(bus.cpuRead(0xFFFF) == 2, "AxROM: bank covers $FFFF");

    bus.cpuWrite(0x8000, 0xFF); // bank nibble 0xF → wraps
    expect(bus.cpuRead(0x8000) == 0xFF % 4, "AxROM: bank index wraps");
}

static void testAxromMirroring() {
    Bus bus;
    if (!insert(bus, makeCart(7, 8, 0, 32768))) return;
    PPU& ppu = bus.getPPU();

    bus.cpuWrite(0x8000, 0x00); // bit4 clear → single-screen low
    expect(ppu.mirrorMode() == PPU::Mirror::SingleLow,
           "AxROM: bit4=0 selects SingleLow");
    ppu.ppuWrite(0x2000, 0x42);
    expect(ppu.ppuRead(0x2400) == 0x42 && ppu.ppuRead(0x2C00) == 0x42,
           "AxROM: all nametables alias $2000");

    bus.cpuWrite(0x8000, 0x13); // bank 3 + bit4 set
    expect(bus.cpuRead(0x8000) == 3, "AxROM: bank bits keep selecting PRG");
    expect(ppu.mirrorMode() == PPU::Mirror::SingleHigh,
           "AxROM: bit4=1 selects SingleHigh");
    ppu.ppuWrite(0x2400, 0x77);
    expect(ppu.ppuRead(0x2000) == 0x77 && ppu.ppuRead(0x2800) == 0x77,
           "AxROM: all nametables alias $2400");
}

// ---------------------------------------------------------------- GxROM

static void testGxromBankSwitch() {
    Bus bus;
    // 128KB PRG (4 × 32KB), 32KB CHR (4 × 8KB), mapper 66
    if (!insert(bus, makeCart(66, 8, 4, 32768, 8192))) return;
    PPU& ppu = bus.getPPU();

    bus.cpuWrite(0x8000, 0x00);
    expect(bus.cpuRead(0x8000) == 0 && bus.cpuRead(0xC000) == 0,
           "GxROM: bank 0 fills the 32KB window");
    expect(ppu.ppuRead(0x0000) == 0, "GxROM: CHR bank 0");

    bus.cpuWrite(0x8000, 0x23); // PRG bank 2, CHR bank 3
    expect(bus.cpuRead(0x8000) == 2 && bus.cpuRead(0xC000) == 2,
           "GxROM: bits 4–5 switch 32KB PRG");
    expect(ppu.ppuRead(0x0000) == 3 && ppu.ppuRead(0x1FFF) == 3,
           "GxROM: bits 0–1 switch 8KB CHR");

    bus.cpuWrite(0x8000, 0xF0); // only bits 4–5 are wired → bank (0xF & 3) = 3
    expect(bus.cpuRead(0x8000) == 3, "GxROM: PRG bank uses only bits 4–5");
}

// ---------------------------------------------------------------- MMC2

static void testMmc2PrgBanking() {
    Bus bus;
    // 512KB PRG (64 × 8KB), 128KB CHR (32 × 4KB), mapper 9
    if (!insert(bus, makeCart(9, 32, 16, 8192, 4096))) return;

    expect(bus.cpuRead(0x8000) == 0, "MMC2: $8000 starts in bank 0");
    expect(bus.cpuRead(0xC000) == 63, "MMC2: last 8KB bank fixed at $C000");

    bus.cpuWrite(0xE000, 5);
    expect(bus.cpuRead(0x8000) == 5, "MMC2: $E000 selects $8000 bank");
    expect(bus.cpuRead(0x9FFF) == 5, "MMC2: bank covers $9FFF");
    expect(bus.cpuRead(0xC000) == 63, "MMC2: fixed bank unaffected");

    bus.cpuWrite(0xE000, 0xFF); // out of range → wraps
    expect(bus.cpuRead(0x8000) == 0xFF % 64, "MMC2: PRG bank index wraps");
}

static void testMmc2ChrLatches() {
    Bus bus;
    if (!insert(bus, makeCart(9, 4, 16, 8192, 4096))) return;
    PPU& ppu = bus.getPPU();

    // Latches power up in the $FD state: $0000 ← $A000, $1000 ← $C000
    bus.cpuWrite(0xA000, 2);
    bus.cpuWrite(0xC000, 4);
    expect(ppu.ppuRead(0x0000) == 2, "MMC2: latch0=$FD → $A000 bank");
    expect(ppu.ppuRead(0x1000) == 4, "MMC2: latch1=$FD → $C000 bank");

    // Reading tile $FE ($0FE8–$0FEF) flips latch 0
    bus.cpuWrite(0xB000, 6);
    ppu.ppuRead(0x0FE8);
    expect(ppu.ppuRead(0x0000) == 6, "MMC2: latch0=$FE → $B000 bank");
    expect(ppu.ppuRead(0x1000) == 4, "MMC2: latch0 flip leaves $1000 alone");

    // Reading tile $FD ($0FD8–$0FDF) flips latch 0 back
    ppu.ppuRead(0x0FD8);
    expect(ppu.ppuRead(0x0000) == 2, "MMC2: latch0 back to $FD bank");

    // Latch 1: $1000 half
    bus.cpuWrite(0xD000, 9);
    ppu.ppuRead(0x1FE8);
    expect(ppu.ppuRead(0x1000) == 9, "MMC2: latch1=$FE → $D000 bank");
    expect(ppu.ppuRead(0x0000) == 2, "MMC2: latch1 flip leaves $0000 alone");
    ppu.ppuRead(0x1FD8);
    expect(ppu.ppuRead(0x1000) == 4, "MMC2: latch1 back to $FD bank");

    // High-plane fetches ($0FE0–$0FE7) must not flip the latch
    ppu.ppuRead(0x0FE0);
    expect(ppu.ppuRead(0x0000) == 2, "MMC2: $0FE0 hi-plane read keeps latch");
}

static void testMmc2Mirroring() {
    Bus bus;
    if (!insert(bus, makeCart(9, 4, 4, 8192, 4096))) return;
    PPU& ppu = bus.getPPU();

    bus.cpuWrite(0xF000, 0);
    expect(ppu.mirrorMode() == PPU::Mirror::Vertical,
           "MMC2: $F000 bit0=0 → vertical");
    bus.cpuWrite(0xF000, 1);
    expect(ppu.mirrorMode() == PPU::Mirror::Horizontal,
           "MMC2: $F000 bit0=1 → horizontal");
}

void run_mapper_bank_tests() {
    std::printf("\nRunning UxROM/CNROM/AxROM (mappers 2/3/7) tests...\n");
    testUxromBankSwitch();
    testUxromChrRam();
    testUxromHasNoPrgRam();
    testCnromChrBankSwitch();
    testCnrom16kPrgMirror();
    testAxromPrgSwitch();
    testAxromMirroring();
    std::printf("Running GxROM/MMC2 (mappers 66/9) tests...\n");
    testGxromBankSwitch();
    testMmc2PrgBanking();
    testMmc2ChrLatches();
    testMmc2Mirroring();
    if (failures == 0) {
        std::printf("Mapper bank tests passed!\n");
    } else {
        std::printf("Mapper bank tests: %d FAILURES\n", failures);
        std::exit(1);
    }
}
