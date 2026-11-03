#ifndef RAM_HPP
#define RAM_HPP

#include <cstdint>
#include <cstddef>
#include <array>

class RAM {
public:
    static constexpr size_t RAM_SIZE = 2048; // 2KB internal RAM

    RAM();
    ~RAM() = default;

    uint8_t read(uint16_t addr) const;
    void write(uint16_t addr, uint8_t data);
    void reset();

private:
    std::array<uint8_t, RAM_SIZE> memory{};
};

#endif // RAM_HPP
