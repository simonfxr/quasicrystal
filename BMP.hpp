#pragma once

#include "defs.hpp"

#include <cstdint>
#include <cstdio>

union RGBA
{
    uint32_t rgba;
    struct
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t a;
    };
};

bool
write_bmp(FILE *out, uint32_t w, uint32_t h, const RGBA *pixels);
