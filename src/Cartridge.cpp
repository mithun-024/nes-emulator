#include "Cartridge.hpp"
#include <cstring>
#include <fstream>

std::unique_ptr<Cartridge> Cartridge::load(const std::string& path,
                                           std::string* error) {
    auto fail = [error](const std::string& msg) -> std::unique_ptr<Cartridge> {
        if (error) *error = msg;
        return nullptr;
    };

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return fail("cannot open file: " + path);
    }

    uint8_t header[16];
    file.read(reinterpret_cast<char*>(header), 16);
    if (file.gcount() != 16) {
        return fail("file too short for iNES header");
    }
    if (std::memcmp(header, "NES\x1A", 4) != 0) {
        return fail("missing iNES magic (NES\\x1A)");
    }

    auto cart = std::unique_ptr<Cartridge>(new Cartridge());

    const uint8_t prgUnits = header[4]; // 16KB units
    const uint8_t chrUnits = header[5]; // 8KB units
    if (prgUnits == 0) {
        return fail("PRG ROM size is zero");
    }

    const uint8_t flags6 = header[6];
    const uint8_t flags7 = header[7];

    cart->mapper = (flags6 >> 4) | (flags7 & 0xF0);
    cart->mirrorVertical = (flags6 & 0x01) != 0;
    cart->battery = (flags6 & 0x02) != 0;
    cart->trainer = (flags6 & 0x04) != 0;

    // Byte 8: PRG RAM in 8KB units; 0 → assume 8KB (iNES 1.0 convention)
    cart->prgRamBytes = header[8] != 0 ? static_cast<size_t>(header[8]) * 8192 : 8192;

    if (cart->trainer) {
        file.seekg(512, std::ios::cur);
        if (!file) return fail("truncated before trainer");
    }

    cart->prg.resize(static_cast<size_t>(prgUnits) * 16384);
    file.read(reinterpret_cast<char*>(cart->prg.data()),
              static_cast<std::streamsize>(cart->prg.size()));
    if (static_cast<size_t>(file.gcount()) != cart->prg.size()) {
        return fail("truncated PRG ROM");
    }

    if (chrUnits > 0) {
        cart->chr.resize(static_cast<size_t>(chrUnits) * 8192);
        file.read(reinterpret_cast<char*>(cart->chr.data()),
                  static_cast<std::streamsize>(cart->chr.size()));
        if (static_cast<size_t>(file.gcount()) != cart->chr.size()) {
            return fail("truncated CHR ROM");
        }
    }
    // chrUnits == 0 → CHR RAM (allocated by the mapper later)

    return cart;
}
