#pragma once

#include "defs.hpp"

#include <cstdint>
#include <optional>

struct Config
{
    bool verbose = false;
    uint32_t nworkers = 1;
    uint32_t img_w = 800;
    uint32_t img_h = 600;
    uint32_t nwaves = 7;
    uint32_t ncosines = 1024;
    uint32_t fps = 30;
    float time_speed = 0.25;
    float time_t0 = 0;
    uint32_t ncapture = 0;

    static std::optional<Config> parse_args(int argc, char *argv[]);

    static void print_usage();
};
