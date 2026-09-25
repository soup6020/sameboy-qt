#include "MemoryModel.h"
#include "core/EmulatorSession.h"

#include <cstring>

size_t MemoryModel::length() const
{
    switch (m_mode) {
        case EntireSpace: return 0x10000;
        case ROM: return 0x8000;
        case VRAM: return 0x2000;
        case ExternalRAM: return 0x2000;
        case RAM: return 0x2000;
    }
    return 0;
}

uint16_t MemoryModel::base() const
{
    switch (m_mode) {
        case EntireSpace: return 0;
        case ROM: return 0;
        case VRAM: return 0x8000;
        case ExternalRAM: return 0xA000;
        case RAM: return 0xC000;
    }
    return 0;
}

void MemoryModel::read(size_t offset, size_t length, uint8_t *destination) const
{
    // Do everything in 0x1000 chunks, never cross a 0x1000 boundary
    while (length) {
        const size_t chunk = std::min(length, 0x1000 - (offset & 0xFFF));
        readChunk(offset + base(), chunk, destination);
        offset += chunk;
        destination += chunk;
        length -= chunk;
    }
}

void MemoryModel::write(size_t offset, const uint8_t *data, size_t length)
{
    while (length) {
        const size_t chunk = std::min(length, 0x1000 - (offset & 0xFFF));
        writeChunk(offset + base(), data, chunk);
        offset += chunk;
        data += chunk;
        length -= chunk;
    }
}

void MemoryModel::readChunk(size_t location, size_t length, uint8_t *dst) const
{
    GB_gameboy_t *gb = m_session->gb();
    auto slowPath = [&] {
        m_session->performAtomic([&] {
            for (size_t i = 0; i < length; i++) {
                dst[i] = GB_safe_read_memory(gb, uint16_t(location + i));
            }
        });
    };
    switch (location >> 12) {
        case 0x0: case 0x1: case 0x2: case 0x3: {
            uint16_t bank = 0;
            auto *data = static_cast<uint8_t *>(GB_get_direct_access(gb, GB_DIRECT_ACCESS_ROM0, nullptr, &bank));
            memcpy(dst, data + bank * 0x4000 + location, length);
            break;
        }
        case 0x4: case 0x5: case 0x6: case 0x7: {
            uint16_t bank = 0;
            size_t size = 0;
            auto *data = static_cast<uint8_t *>(GB_get_direct_access(gb, GB_DIRECT_ACCESS_ROM, &size, &bank));
            if (m_mode != EntireSpace) {
                bank = uint16_t(m_selectedBank & (size / 0x4000 - 1));
            }
            memcpy(dst, data + bank * 0x4000 + location - 0x4000, length);
            break;
        }
        case 0x8: case 0x9: {
            uint16_t bank = 0;
            size_t size = 0;
            auto *data = static_cast<uint8_t *>(GB_get_direct_access(gb, GB_DIRECT_ACCESS_VRAM, &size, &bank));
            if (m_mode != EntireSpace) {
                bank = uint16_t(m_selectedBank & (size / 0x2000 - 1));
            }
            memcpy(dst, data + bank * 0x2000 + location - 0x8000, length);
            break;
        }
        case 0xA: case 0xB: {
            // Some carts are special, use memory read directly in full mem mode
            if (m_mode == EntireSpace) {
                slowPath();
                break;
            }
            uint16_t bank = 0;
            size_t size = 0;
            auto *data = static_cast<uint8_t *>(GB_get_direct_access(gb, GB_DIRECT_ACCESS_CART_RAM, &size, &bank));
            if (size == 0) {
                memset(dst, 0xFF, length);
                break;
            }
            bank = uint16_t(m_selectedBank & (size / 0x2000 - 1));
            if (location + length - 0xA000 > size) {
                slowPath();
                break;
            }
            memcpy(dst, data + bank * 0x2000 + location - 0xA000, length);
            break;
        }
        case 0xC: case 0xE: {
            auto *data = static_cast<uint8_t *>(GB_get_direct_access(gb, GB_DIRECT_ACCESS_RAM, nullptr, nullptr));
            memcpy(dst, data + (location & 0xFFF), length);
            break;
        }
        case 0xD: {
            uint16_t bank = 0;
            size_t size = 0;
            auto *data = static_cast<uint8_t *>(GB_get_direct_access(gb, GB_DIRECT_ACCESS_RAM, &size, &bank));
            if (m_mode != EntireSpace) {
                bank = uint16_t(m_selectedBank & (size / 0x1000 - 1));
            }
            memcpy(dst, data + bank * 0x1000 + location - 0xD000, length);
            break;
        }
        case 0xF:
            slowPath();
            break;
    }
}

void MemoryModel::writeChunk(size_t location, const uint8_t *src, size_t length)
{
    GB_gameboy_t *gb = m_session->gb();
    auto slowPath = [&] {
        m_session->performAtomic([&] {
            for (size_t i = 0; i < length; i++) {
                GB_write_memory(gb, uint16_t(location + i), src[i]);
            }
        });
    };
    switch (location >> 12) {
        case 0x0: case 0x1: case 0x2: case 0x3:
        case 0x4: case 0x5: case 0x6: case 0x7: {
            uint16_t bank = 0;
            size_t size = 0;
            auto *data = static_cast<uint8_t *>(GB_get_direct_access(
                gb, location < 0x4000 ? GB_DIRECT_ACCESS_ROM0 : GB_DIRECT_ACCESS_ROM, &size, &bank));
            if (m_mode != EntireSpace && location >= 0x4000) {
                bank = uint16_t(m_selectedBank & (size / 0x4000 - 1));
            }
            memcpy(data + bank * 0x4000 + (location & 0x3FFF), src, length);
            m_session->setROMModified();
            break;
        }
        case 0x8: case 0x9: {
            uint16_t bank = 0;
            size_t size = 0;
            auto *data = static_cast<uint8_t *>(GB_get_direct_access(gb, GB_DIRECT_ACCESS_VRAM, &size, &bank));
            if (m_mode != EntireSpace) {
                bank = uint16_t(m_selectedBank & (size / 0x2000 - 1));
            }
            memcpy(data + bank * 0x2000 + location - 0x8000, src, length);
            break;
        }
        case 0xA: case 0xB: {
            if (m_mode == EntireSpace) {
                slowPath();
                break;
            }
            uint16_t bank = 0;
            size_t size = 0;
            auto *data = static_cast<uint8_t *>(GB_get_direct_access(gb, GB_DIRECT_ACCESS_CART_RAM, &size, &bank));
            if (size == 0) {
                break; // Nothing to write to
            }
            bank = uint16_t(m_selectedBank & (size / 0x2000 - 1));
            if (location + length - 0xA000 > size) {
                slowPath();
                break;
            }
            memcpy(data + bank * 0x2000 + location - 0xA000, src, length);
            break;
        }
        case 0xC: case 0xE: {
            auto *data = static_cast<uint8_t *>(GB_get_direct_access(gb, GB_DIRECT_ACCESS_RAM, nullptr, nullptr));
            memcpy(data + (location & 0xFFF), src, length);
            break;
        }
        case 0xD: {
            uint16_t bank = 0;
            size_t size = 0;
            auto *data = static_cast<uint8_t *>(GB_get_direct_access(gb, GB_DIRECT_ACCESS_RAM, &size, &bank));
            if (m_mode != EntireSpace) {
                bank = uint16_t(m_selectedBank & (size / 0x1000 - 1));
            }
            memcpy(data + bank * 0x1000 + location - 0xD000, src, length);
            break;
        }
        case 0xF:
            slowPath();
            break;
    }
}
