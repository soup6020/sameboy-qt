#pragma once

#include <cstdint>
#include <cstddef>

class EmulatorSession;

// Port of Cocoa/GBMemoryByteArray: a banked view over one Game Boy address space.
class MemoryModel
{
public:
    enum Mode {
        EntireSpace,
        ROM,
        VRAM,
        ExternalRAM,
        RAM,
    };

    explicit MemoryModel(EmulatorSession *session) : m_session(session) {}

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode = mode; }
    uint16_t selectedBank() const { return m_selectedBank; }
    void setSelectedBank(uint16_t bank) { m_selectedBank = bank; }

    size_t length() const;
    uint16_t base() const;
    void read(size_t offset, size_t length, uint8_t *destination) const;
    void write(size_t offset, const uint8_t *data, size_t length);

private:
    void readChunk(size_t location, size_t length, uint8_t *destination) const;
    void writeChunk(size_t location, const uint8_t *data, size_t length);

    EmulatorSession *m_session;
    Mode m_mode = EntireSpace;
    uint16_t m_selectedBank = 0;
};
