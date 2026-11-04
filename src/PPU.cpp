#include "PPU.hpp"
#include "Mapper.hpp"

PPU::PPU() {
    reset();
}

void PPU::reset() {
    ctrl = 0;
    mask = 0;
    status = 0;
    oamAddr = 0;
    v = t = 0;
    fineX = 0;
    w = false;
    readBuffer = 0;
    openBus = 0;
    vram.fill(0);
    oam.fill(0);
    palette.fill(0);
    fb.fill(0xFF000000);
    sl = 0;
    cyc = 0;
    frameDone = false;
    nmiEvent = false;
}

void PPU::setMapper(Mapper* m, bool vertical) {
    mapper = m;
    verticalMirroring = vertical;
}

// Nametable region $2000–$3EFF → 2KB CIRAM (indices 0–0x7FF)
uint16_t PPU::mirrorNametable(uint16_t addr) const {
    addr = (addr - 0x2000) & 0x0FFF; // 4KB nametable window
    if (verticalMirroring) {
        return addr & 0x7FF;
    }
    return ((addr & 0x800) >> 1) | (addr & 0x3FF);
}

// $3F00/$3F04/$3F08/$3F0C mirror through $3F10/$3F14/$3F18/$3F1C
uint8_t PPU::paletteIndex(uint16_t addr) {
    addr &= 0x1F;
    if ((addr & 0x13) == 0x10) {
        addr &= ~0x10;
    }
    return static_cast<uint8_t>(addr);
}

// Approximate 2C02 RGB for each of the 64 palette entries
uint32_t PPU::nesColor(uint8_t index) {
    static const uint32_t table[64] = {
        0xFF666666, 0xFF002A88, 0xFF1412A7, 0xFF3B00A4, 0xFF5C007E, 0xFF6E0040, 0xFF6C0600, 0xFF561D00,
        0xFF333500, 0xFF0B4800, 0xFF005200, 0xFF004F08, 0xFF00404D, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFFADADAD, 0xFF155FD9, 0xFF4240FF, 0xFF7527FE, 0xFFA01ACC, 0xFFB71E7B, 0xFFB53120, 0xFF994E00,
        0xFF6B6D00, 0xFF388700, 0xFF0C9300, 0xFF008F32, 0xFF007C8D, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFFFFFEFF, 0xFF64B0FF, 0xFF9290FF, 0xFFC676FF, 0xFFF36AFF, 0xFFFE6ECC, 0xFFFE8170, 0xFFEA9E22,
        0xFFBCBE00, 0xFF88D800, 0xFF5CE430, 0xFF45E082, 0xFF48CDDE, 0xFF4F4F4F, 0xFF000000, 0xFF000000,
        0xFFFFFEFF, 0xFFC0DFFF, 0xFFD3D2FF, 0xFFE8C8FF, 0xFFFBC2FF, 0xFFFEC4EA, 0xFFFECCC5, 0xFFF7D8A5,
        0xFFE4E594, 0xFFCFEF96, 0xFFBDF4AB, 0xFFB3F3CC, 0xFFB5EBF2, 0xFFB8B8B8, 0xFF000000, 0xFF000000,
    };
    return table[index & 0x3F];
}

uint8_t PPU::ppuRead(uint16_t addr) {
    addr &= 0x3FFF;
    if (addr < 0x2000) {
        return mapper ? mapper->ppuRead(addr) : 0x00;
    }
    if (addr < 0x3F00) {
        return vram[mirrorNametable(addr)];
    }
    return palette[paletteIndex(addr)];
}

void PPU::ppuWrite(uint16_t addr, uint8_t data) {
    addr &= 0x3FFF;
    if (addr < 0x2000) {
        if (mapper) mapper->ppuWrite(addr, data);
        return;
    }
    if (addr < 0x3F00) {
        vram[mirrorNametable(addr)] = data;
        return;
    }
    palette[paletteIndex(addr)] = data;
}

uint8_t PPU::regRead(uint8_t reg) {
    switch (reg & 7) {
    case 2: {
        uint8_t value = static_cast<uint8_t>((status & 0xE0) | (openBus & 0x1F));
        status &= ~0x80;
        w = false;
        openBus = value;
        return value;
    }
    case 4: {
        uint8_t value = oam[oamAddr];
        openBus = value;
        return value;
    }
    case 7: {
        uint8_t value;
        if ((v & 0x3FFF) >= 0x3F00) {
            value = palette[paletteIndex(v)];
            readBuffer = ppuRead(v & 0x2FFF);
        } else {
            value = readBuffer;
            readBuffer = ppuRead(v);
        }
        v = static_cast<uint16_t>(v + ((ctrl & 0x04) ? 32 : 1));
        openBus = value;
        return value;
    }
    default:
        return openBus;
    }
}

void PPU::regWrite(uint8_t reg, uint8_t data) {
    openBus = data;
    switch (reg & 7) {
    case 0: {
        const uint8_t old = ctrl;
        ctrl = data;
        t = static_cast<uint16_t>((t & ~0x0C00) | ((data & 0x03) << 10));
        // Enabling NMI while vblank flag is already set fires NMI immediately
        if ((data & 0x80) && !(old & 0x80) && (status & 0x80)) {
            nmiEvent = true;
        }
        break;
    }
    case 1:
        mask = data;
        break;
    case 3:
        oamAddr = data;
        break;
    case 4:
        oam[oamAddr++] = data;
        break;
    case 5:
        if (!w) {
            t = static_cast<uint16_t>((t & 0x7FE0) | (data >> 3));
            fineX = data & 0x07;
            w = true;
        } else {
            t = static_cast<uint16_t>((t & 0x0C1F) |
                                      ((data & 0xF8) << 2) |
                                      ((data & 0x07) << 12));
            w = false;
        }
        break;
    case 6:
        if (!w) {
            t = static_cast<uint16_t>((t & 0x00FF) | ((data & 0x3F) << 8));
            w = true;
        } else {
            t = static_cast<uint16_t>((t & 0xFF00) | data);
            v = t;
            w = false;
        }
        break;
    case 7:
        ppuWrite(v, data);
        v = static_cast<uint16_t>(v + ((ctrl & 0x04) ? 32 : 1));
        break;
    default:
        break;
    }
}

void PPU::incrementY() {
    if ((v & 0x7000) != 0x7000) {
        v += 0x1000;
    } else {
        v &= ~0x7000;
        int coarseY = static_cast<int>((v >> 5) & 0x1F);
        if (coarseY == 29) {
            coarseY = 0;
            v ^= 0x0800;
        } else if (coarseY == 31) {
            coarseY = 0;
        } else {
            coarseY++;
        }
        v = static_cast<uint16_t>((v & ~0x03E0) | (coarseY << 5));
    }
}

void PPU::copyHorizontalBits() {
    v = static_cast<uint16_t>((v & ~0x041F) | (t & 0x041F));
}

void PPU::copyVerticalBits() {
    v = static_cast<uint16_t>((v & ~0x7BE0) | (t & 0x7BE0));
}

void PPU::renderScanline(int y) {
    const bool bgOn = (mask & 0x08) != 0;
    const bool spOn = (mask & 0x10) != 0;
    const bool clipBgLeft = (mask & 0x02) == 0;
    const bool clipSpLeft = (mask & 0x04) == 0;

    const uint32_t backdrop = nesColor(palette[paletteIndex(0x3F00)]);

    // --- Sprite layer for this scanline ---
    // spritePixels: 0 = transparent, else palette color index 1–3
    uint8_t sprPal[256] = {};
    uint8_t sprColor[256] = {};
    uint8_t sprPrio[256] = {}; // 0 = in front of BG, 1 = behind
    bool sprIsZero[256] = {};

    if (spOn) {
        const int height = (ctrl & 0x20) ? 16 : 8;
        int inRange = 0;
        for (int i = 0; i < 64; i++) {
            const uint8_t sy = oam[i * 4 + 0];
            const uint8_t tile = oam[i * 4 + 1];
            const uint8_t attr = oam[i * 4 + 2];
            const uint8_t sx = oam[i * 4 + 3];
            int row = y - static_cast<int>(sy);
            if (row < 0 || row >= height) continue;
            if (inRange < 8) {
                inRange++;
            } else {
                status |= 0x20; // sprite overflow (simplified)
                break;
            }

            if (attr & 0x80) row = height - 1 - row;

            uint16_t patternAddr;
            if (height == 16) {
                const uint16_t table = (tile & 0x01) ? 0x1000 : 0x0000;
                uint8_t tilePair = tile & 0xFE;
                if (row >= 8) {
                    tilePair++;
                    row -= 8;
                }
                patternAddr = static_cast<uint16_t>(table + tilePair * 16 + row);
            } else {
                const uint16_t table = (ctrl & 0x08) ? 0x1000 : 0x0000;
                patternAddr = static_cast<uint16_t>(table + tile * 16 + row);
            }

            const uint8_t lo = ppuRead(patternAddr);
            const uint8_t hi = ppuRead(static_cast<uint16_t>(patternAddr + 8));
            const uint8_t palNum = static_cast<uint8_t>((attr >> 1) & 0x03);
            const bool behind = (attr & 0x20) != 0;
            const bool flipH = (attr & 0x40) != 0;

            for (int col = 0; col < 8; col++) {
                const int px = sx + col;
                if (px < 0 || px >= 256) continue;
                const int bit = flipH ? col : (7 - col);
                const uint8_t pixel = static_cast<uint8_t>(((lo >> bit) & 1) | (((hi >> bit) & 1) << 1));
                if (pixel == 0) continue;
                // Lower OAM index wins among sprites
                if (sprColor[px] != 0) continue;
                sprColor[px] = pixel;
                sprPal[px] = palNum;
                sprPrio[px] = behind ? 1 : 0;
                sprIsZero[px] = (i == 0);
            }
        }
    }

    // --- Background + composite ---
    const int coarseX0 = v & 0x1F;
    const int ntX0 = (v >> 10) & 1;
    const int coarseY = (v >> 5) & 0x1F;
    const int fineY = (v >> 12) & 7;
    const int ntY = (v >> 11) & 1;
    const int baseX = coarseX0 * 8 + fineX;

    for (int x = 0; x < 256; x++) {
        uint8_t bgPixel = 0;
        uint8_t bgPal = 0;

        const bool bgVisible = bgOn && (!clipBgLeft || x >= 8);
        if (bgVisible) {
            int absX = baseX + x;
            int ntSel = ntX0;
            if (absX >= 256) {
                absX -= 256;
                ntSel ^= 1;
            } else if (absX < 0) {
                absX += 256;
                ntSel ^= 1;
            }
            // absX can still exceed 256 if coarseX*8+fineX+x is large
            if (absX >= 256) {
                absX -= 256;
                ntSel ^= 1;
            }

            const int tileCol = absX >> 3;
            const int finePX = absX & 7;

            const uint16_t ntAddr = static_cast<uint16_t>(
                0x2000 | (ntY << 11) | (ntSel << 10) | (coarseY << 5) | tileCol);
            const uint8_t tile = ppuRead(ntAddr);

            const uint16_t patternBase = (ctrl & 0x10) ? 0x1000 : 0x0000;
            const uint16_t patternAddr = static_cast<uint16_t>(patternBase + tile * 16 + fineY);
            const uint8_t lo = ppuRead(patternAddr);
            const uint8_t hi = ppuRead(static_cast<uint16_t>(patternAddr + 8));
            const int bit = 7 - finePX;
            bgPixel = static_cast<uint8_t>(((lo >> bit) & 1) | (((hi >> bit) & 1) << 1));

            if (bgPixel != 0) {
                const uint16_t attrAddr = static_cast<uint16_t>(
                    0x23C0 | (ntY << 11) | (ntSel << 10) | ((coarseY >> 2) << 3) | (tileCol >> 2));
                const uint8_t attr = ppuRead(attrAddr);
                const int shift = ((coarseY & 0x02) << 1) | (tileCol & 0x02);
                bgPal = static_cast<uint8_t>((attr >> shift) & 0x03);
            }
        }

        uint8_t spPixel = 0;
        uint8_t spPal = 0;
        bool spBehind = false;
        bool spZero = false;
        if (spOn && (!clipSpLeft || x >= 8)) {
            spPixel = sprColor[x];
            spPal = sprPal[x];
            spBehind = sprPrio[x] != 0;
            spZero = sprIsZero[x];
        }

        // Sprite 0 hit: opaque BG + opaque sprite 0, not at x=255
        if (spZero && bgPixel != 0 && spPixel != 0 && x != 255) {
            status |= 0x40;
        }

        uint32_t rgb;
        if (spPixel != 0 && (bgPixel == 0 || !spBehind)) {
            rgb = nesColor(palette[paletteIndex(0x10 + spPal * 4 + spPixel)]);
        } else if (bgPixel != 0) {
            rgb = nesColor(palette[paletteIndex(bgPal * 4 + bgPixel)]);
        } else {
            rgb = backdrop;
        }
        fb[y * 256 + x] = rgb;
    }
}

void PPU::tick() {
    // VBlank flag + NMI at scanline 241, dot 1
    if (sl == 241 && cyc == 1) {
        status |= 0x80;
        if (ctrl & 0x80) {
            nmiEvent = true;
        }
        frameDone = true;
    }

    // Pre-render: clear flags at scanline 261, dot 1
    if (sl == 261 && cyc == 1) {
        status &= ~0xE0; // vblank, sprite0, overflow
    }

    // Visible scanlines: render line and update scroll at end-of-line dots
    if (sl < 240) {
        if (cyc == 256) {
            if (renderingEnabled()) {
                renderScanline(sl);
                incrementY();
            } else {
                // Forced blank / rendering off: backdrop for the line
                const uint32_t backdrop = nesColor(palette[paletteIndex(0x3F00)]);
                for (int x = 0; x < 256; x++) {
                    fb[sl * 256 + x] = backdrop;
                }
            }
        } else if (cyc == 257 && renderingEnabled()) {
            copyHorizontalBits();
        }
    }

    // Pre-render vertical reload (cycles 280–304)
    if (sl == 261 && cyc >= 280 && cyc <= 304 && renderingEnabled()) {
        copyVerticalBits();
    }

    // Advance dot / scanline
    cyc++;
    if (cyc > 340) {
        cyc = 0;
        sl++;
        if (sl > 261) {
            sl = 0;
        }
    }
}

bool PPU::takeFrameComplete() {
    const bool done = frameDone;
    frameDone = false;
    return done;
}

bool PPU::takeNmi() {
    const bool n = nmiEvent;
    nmiEvent = false;
    return n;
}
