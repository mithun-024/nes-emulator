#include "Bus.hpp"
#include "Mapper0.hpp"
#include "Mapper1.hpp"
#include "Mapper2.hpp"
#include "Mapper3.hpp"
#include "MapperAxrom.hpp"
#include "MapperCnrom.hpp"
#include "MapperGxrom.hpp"
#include "MapperMmc2.hpp"
#include <iostream>

Bus::Bus() {
    apu.setBus(this);
}

bool Bus::irqPending() const {
    if (apu.irqPending()) return true;
    return mapper != nullptr && mapper->irqPending();
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
    case 1:
        newMapper = std::make_unique<Mapper1>(*cart);
        break;
    case 2:
        newMapper = std::make_unique<Mapper2>(*cart);
        break;
    case 3:
        newMapper = std::make_unique<MapperCnrom>(*cart);
        break;
    case 4:
        newMapper = std::make_unique<Mapper3>(*cart);
        break;
    case 7:
        newMapper = std::make_unique<MapperAxrom>(*cart);
        break;
    case 9:
        newMapper = std::make_unique<MapperMmc2>(*cart);
        break;
    case 66:
        newMapper = std::make_unique<MapperGxrom>(*cart);
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
    mapper->setBus(this);
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
        // APU status (also acknowledges the frame IRQ)
        if (addr == 0x4015) {
            return const_cast<APU&>(apu).readStatus();
        }
        // Controller ports shift out one button bit per read
        if (addr == 0x4016) {
            return const_cast<Controller&>(controllers[0]).read();
        }
        if (addr == 0x4017) {
            return const_cast<Controller&>(controllers[1]).read();
        }
        return 0x00;
    }
    // 0x4020 - 0xFFFF: Cartridge Space
    else if (addr >= 0x4020) {
        if (mapper) {
            // MMC3: any CPU read in $E000–$FFFF acknowledges its IRQ
            if (addr >= 0xE000) {
                const_cast<Mapper&>(*mapper).irqAcknowledge();
            }
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
        } else {
            // APU registers: $4000–$4013, $4015, $4017
            apu.write(addr, data);
        }
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
