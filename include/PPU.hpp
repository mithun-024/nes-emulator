#ifndef PPU_HPP
#define PPU_HPP

#include <array>
#include <cstdint>

class Mapper;

// Picture Processing Unit: registers, memory map, scanline renderer, timing.
class PPU {
public:
    PPU();

    void reset();

    // CPU-side register access; addr is 0–7 ($2000–$2007 → addr & 7)
    uint8_t regRead(uint8_t reg);
    void regWrite(uint8_t reg, uint8_t data);

    // PPU-side memory (pattern tables, nametables, palette)
    uint8_t ppuRead(uint16_t addr);
    void ppuWrite(uint16_t addr, uint8_t data);

    // Advance one PPU dot (341 dots × 262 scanlines)
    void tick();

    // Called by Bus when a cartridge is inserted/removed
    void setMapper(Mapper* m, bool vertical);

    // Frame output: 256×240 ARGB8888, set at the start of vblank
    const uint32_t* framebuffer() const { return fb.data(); }
    bool takeFrameComplete();
    bool takeNmi();

    // --- Test / debug hooks (no read side effects) ---
    uint8_t statusRaw() const { return status; }
    uint8_t ctrlRaw() const { return ctrl; }
    uint8_t maskRaw() const { return mask; }
    uint16_t vramAddrRaw() const { return v; }
    int scanline() const { return sl; }
    int dot() const { return cyc; }
    void setVblank() { status |= 0x80; }

    // OAM access for tests / sprite evaluation
    uint8_t oamRead(uint8_t idx) const { return oam[idx]; }
    void oamWrite(uint8_t idx, uint8_t data) { oam[idx] = data; }

private:
    uint16_t mirrorNametable(uint16_t addr) const;
    static uint8_t paletteIndex(uint16_t addr);
    static uint32_t nesColor(uint8_t index);

    bool renderingEnabled() const { return (mask & 0x18) != 0; }
    void renderScanline(int y);
    void incrementY();
    void copyHorizontalBits();
    void copyVerticalBits();

    Mapper* mapper = nullptr;
    bool verticalMirroring = true;

    // 2KB CIRAM (nametables), 256B OAM, 32B palette
    std::array<uint8_t, 0x800> vram{};
    std::array<uint8_t, 256> oam{};
    std::array<uint8_t, 32> palette{};
    std::array<uint32_t, 256 * 240> fb{};

    // Registers
    uint8_t ctrl = 0;    // $2000 PPUCTRL
    uint8_t mask = 0;    // $2001 PPUMASK
    uint8_t status = 0;  // $2002 PPUSTATUS
    uint8_t oamAddr = 0; // $2003 OAMADDR

    // Loopy internals
    uint16_t v = 0;      // current VRAM address
    uint16_t t = 0;      // temporary address
    uint8_t fineX = 0;   // fine X scroll
    bool w = false;      // write latch: false = first/next write

    uint8_t readBuffer = 0; // $2007 read buffer
    uint8_t openBus = 0;    // last value on the PPU data bus

    // Timing
    int sl = 0;  // scanline 0–261 (0–239 visible, 240 post, 241–260 vblank, 261 pre)
    int cyc = 0; // dot 0–340
    bool frameDone = false;
    bool nmiEvent = false;
};

#endif // PPU_HPP
