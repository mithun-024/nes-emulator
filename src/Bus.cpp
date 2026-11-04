#include "Bus.hpp"
#include "Mapper0.hpp"
#include <iostream>

Bus::Bus() {
}

bool Bus::insertCartridge(std::unique_ptr<Cartridge> cart, std::string* error) {
    if (!cart) {
        if (error) *error = "null cartridge";
        return false;
    }

    std::unique_ptr<Mapper> newMapper;
    switch (cart->mapperId()) {
    case 0:
        newMapper = std::make_unique<Mapper0>(*cart);
        break;
    default:
        if (error) {
            *error = "unsupported mapper: " + std::to_string(cart->mapperId());
        }
        return false;
    }

    const bool vertical = cart->verticalMirroring();
    cartridge = std::move(cart);
    mapper = std::move(newMapper);
    ppu.setMapper(mapper.get(), vertical);
    return true;
}

uint8_t Bus::cpuRead(uint16_t addr) const {
    // 0x0000 - 0x1FFF: 2KB Internal RAM (mirrored 4 times)
    if (addr <= 0x1FFF) {
        return ram.read(addr);
    }
    // 0x2000 - 0x3FFF: PPU Registers (8 registers mirrored every 8 bytes)
    else if (addr >= 0x2000 && addr <= 0x3FFF) {
        return const_cast<PPU&>(ppu).regRead(static_cast<uint8_t>(addr & 7));
    }
    // 0x4000 - 0x4017: APU & I/O Registers
    else if (addr >= 0x4000 && addr <= 0x4017) {
        // Controller ports shift out one button bit per read
        if (addr == 0x4016) {
            return const_cast<Controller&>(controllers[0]).read();
        }
        if (addr == 0x4017) {
            return const_cast<Controller&>(controllers[1]).read();
        }
        // TODO: APU read ($4015) implementation
        return 0x00;
    }
    // 0x4020 - 0xFFFF: Cartridge Space
    else if (addr >= 0x4020) {
        if (mapper) {
            return mapper->cpuRead(addr);
        }
        // No cartridge: flat buffer so tests can plant vectors
        return cartSpace[addr];
    }

    return 0x00;
}

void Bus::cpuWrite(uint16_t addr, uint8_t data) {
    // 0x0000 - 0x1FFF: 2KB Internal RAM
    if (addr <= 0x1FFF) {
        ram.write(addr, data);
    }
    // 0x2000 - 0x3FFF: PPU Registers
    else if (addr >= 0x2000 && addr <= 0x3FFF) {
        ppu.regWrite(static_cast<uint8_t>(addr & 7), data);
    }
    // 0x4000 - 0x4017: APU & I/O Registers
    else if (addr >= 0x4000 && addr <= 0x4017) {
        if (addr == 0x4014) {
            // OAM DMA: copy 256 bytes from page data<<8 into OAM
            const uint16_t base = static_cast<uint16_t>(data) << 8;
            for (int i = 0; i < 256; i++) {
                ppu.oamWrite(static_cast<uint8_t>(i), cpuRead(static_cast<uint16_t>(base + i)));
            }
            // DMA stalls the CPU for 513 cycles (+1 on odd CPU cycle — we use 513)
            dmaStall += 513;
        } else if (addr == 0x4016) {
            // Strobe line is shared: one write clocks both controller ports
            controllers[0].write(data);
            controllers[1].write(data);
        }
        // TODO: APU write implementation
    }
    // 0x4020 - 0xFFFF: Cartridge Space
    else if (addr >= 0x4020) {
        if (mapper) {
            mapper->cpuWrite(addr, data);
        } else {
            cartSpace[addr] = data;
        }
    }
}
