#include "Config.hpp"

#include "simd_vec.hpp"

#include <cstring>

std::optional<Config>
Config::parse_args(int argc, char **const argv)
{
    Config conf;
    bool need_arg = false;
    char optchar = 0;
    for (int i = 1; i < argc; ++i) {
        if (need_arg) {
            need_arg = false;
            switch (optchar) {
            case 'j':
            case 'n':
            case 'f':
            case 'C':
            case 'c': {
                char *endp = nullptr;
                auto n = strtoll(argv[i], &endp, 10);
                switch (optchar) {
                case 'j':
                    if (n < 1 || n > 256)
                        return {};
                    conf.nworkers = uint32_t(n);
                    break;
                case 'n':
                    if (n < 1 || n > 4096)
                        return {};
                    conf.nwaves = uint32_t(n);
                    break;
                case 'c':
                    if (n < 1 || n > (1 << 20))
                        return {};
                    conf.ncosines = uint32_t(n);
                    break;
                case 'f':
                    if (n < 1 || n > 4096)
                        return {};
                    conf.fps = uint32_t(n);
                    break;
                case 'C':
                    if (n < 0 || n > (1 << 30))
                        return {};
                    conf.ncapture = uint32_t(n);
                    break;
                }
                break;
            }
            case 's': {
                int w, h;
                if (sscanf(argv[i], "%dx%d", &w, &h) != 2)
                    return {};
                if (!(0 < w && w < 4096 && 0 < h && h < 4096))
                    return {};

                if (w % vecf_t::size != 0) {
                    fprintf(stderr,
                            "invalid image width, has to be divisble by %d\n",
                            int(vecf_t::size));
                    return {};
                }
                conf.img_w = uint32_t(w);
                conf.img_h = uint32_t(h);
                break;
            }
            }
        } else {
            if (strlen(argv[i]) == 2 && argv[i][0] == '-') {
                optchar = argv[i][1];
                if (optchar == 'v') {
                    conf.verbose = true;
                } else if (strchr("njscfC", optchar)) {
                    need_arg = true;
                } else {
                    return {};
                }
            } else {
                return {};
            }
        }
    }

    if (need_arg)
        return {};

    return { conf };
}

#define USAGE_STR                                                              \
    "Usage: crystal [ARGS]...\n"                                               \
    "\n"                                                                       \
    "Options:\n"                                                               \
    "  -v          Enable verbose output\n"                                    \
    "  -n NWAVES   Number of WAVES in the crystal\n"                           \
    "  -c NCOS     Size of cos(x) lookup table\n"                              \
    "  -j WORKERS  Use WORKERS number of threads\n"                            \
    "  -s WxH      Framebuffer size, W pixels wide and H pixels tall\n"        \
    "  -C N        Capture only: save N frames without opening a window\n"

void
Config::print_usage()
{
    fprintf(stderr, USAGE_STR);
}
