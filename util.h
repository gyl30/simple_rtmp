#ifndef __UTIL_H__
#define __UTIL_H__

#include <cstdint>

static inline void be_write_uint16(uint8_t* ptr, uint16_t val)
{
    ptr[0] = static_cast<uint8_t>((val >> 8) & 0xFF);
    ptr[1] = static_cast<uint8_t>(val & 0xFF);
}

static inline void be_write_uint24(uint8_t* ptr, uint32_t val)
{
    ptr[0] = static_cast<uint8_t>((val >> 16) & 0xFF);
    ptr[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
    ptr[2] = static_cast<uint8_t>(val & 0xFF);
}

static inline void be_write_uint32(uint8_t* ptr, uint32_t val)
{
    ptr[0] = static_cast<uint8_t>((val >> 24) & 0xFF);
    ptr[1] = static_cast<uint8_t>((val >> 16) & 0xFF);
    ptr[2] = static_cast<uint8_t>((val >> 8) & 0xFF);
    ptr[3] = static_cast<uint8_t>(val & 0xFF);
}

static inline void be_read_uint16(const uint8_t* ptr, uint16_t* val) { *val = (ptr[0] << 8) | ptr[1]; }

static inline void be_read_uint24(const uint8_t* ptr, uint32_t* val) { *val = (ptr[0] << 16) | (ptr[1] << 8) | ptr[2]; }

static inline void be_read_uint32(const uint8_t* ptr, uint32_t* val)
{
    *val = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
}

static inline void le_write_uint32(uint8_t* ptr, uint32_t val)
{
    ptr[3] = static_cast<uint8_t>((val >> 24) & 0xFF);
    ptr[2] = static_cast<uint8_t>((val >> 16) & 0xFF);
    ptr[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
    ptr[0] = static_cast<uint8_t>(val & 0xFF);
}

static inline void le_read_uint32(const uint8_t* ptr, uint32_t* val)
{
    *val = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
}

#endif    // __UTIL_H__
