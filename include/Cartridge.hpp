#ifndef CARTRIDGE_HPP
#define CARTRIDGE_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Parsed iNES cartridge: header metadata + PRG/CHR ROM images.
class Cartridge {
public:
    // Returns nullptr and sets *error on failure.
    static std::unique_ptr<Cartridge> load(const std::string& path,
                                           std::string* error = nullptr);

    int mapperId() const { return mapper; }
    bool verticalMirroring() const { return mirrorVertical; }
    bool batteryBacked() const { return battery; }
    bool hasTrainer() const { return trainer; }

    const std::vector<uint8_t>& prgRom() const { return prg; }
    const std::vector<uint8_t>& chrRom() const { return chr; }
    // 0 in the header means 8KB for compatibility with old dumpers
    size_t prgRamSize() const { return prgRamBytes; }

private:
    Cartridge() = default;

    int mapper = 0;
    bool mirrorVertical = false;
    bool battery = false;
    bool trainer = false;
    size_t prgRamBytes = 0;
    std::vector<uint8_t> prg;
    std::vector<uint8_t> chr;
};

#endif // CARTRIDGE_HPP
