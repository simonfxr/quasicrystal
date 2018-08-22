#include "BMP.hpp"
#include "Config.hpp"
#include "euclidean2d.hpp"
#include "render.hpp"
#include "simd_vec.hpp"
#include "utils.hpp"

#include <SDL.h>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <vector>

struct Anim
{
    StopWatch watch;

    bool running = false;

    uint32_t screenshot_ser = 0;
    uint32_t screenshot_id = 0;
    uint32_t screenshot_max = 0;

    double anim_time = 0;

    const Config &conf;
    Renderer renderer;

    SDL_Surface *screen = nullptr;

    Anim(const Config &conf) : conf(conf), renderer(conf) {}

    bool init();

    bool handle_event(const SDL_Event &);
    bool handle_key_event(const SDL_KeyboardEvent &);
    void animation();
    void render();
    void draw();
    void write_screenshot(uint32_t ser, uint32_t id);
    bool resize(int, int);

    void shutdown();
};

static void
print_sdl_error(const char *prefix)
{
    fprintf(stderr, "%s: %s\n", prefix, SDL_GetError());
}

bool
Anim::resize(int w, int h)
{
    fprintf(stderr, "resized: %dx%d\n", w, h);

    if (!renderer.is_capture_mode()) {
        screen = SDL_SetVideoMode(
          w, h, 32, SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_RESIZABLE);

        if (screen == NULL) {
            print_sdl_error("Opening/Resizing window");
            return false;
        }
    }

    return true;
}

void
Anim::draw()
{
    double t0 = 0;
    if (conf.verbose)
        t0 = watch.now();

    uint32_t win_w = screen->w;
    uint32_t win_h = screen->h;

    if (SDL_MUSTLOCK(screen))
        SDL_LockSurface(screen);

    assert(screen->format->BytesPerPixel == 4);

    auto const &img = *renderer.srcImage;

    uint32_t w = std::min(win_w, img.w);
    uint32_t h = std::min(win_h, img.h);

    uint32_t stride = screen->pitch / 4;
    Uint32 *dest = (Uint32 *) screen->pixels;

    if (stride == w) {
        memcpy(dest, &img(0, 0), w * h * sizeof(RGBA));
    } else {
        for (uint32_t y = 0; y < h; ++y) {

            memcpy(dest, &img(0, y), w * sizeof(RGBA));
            dest += stride;

            // for (uint32_t x = 0; x < w; ++x) {
            //     put_pixel(screen, x, y,
            //               map_rgba(screen->format, srcImage(x, y)));
            // }
        }
    }

    if (SDL_MUSTLOCK(screen))
        SDL_UnlockSurface(screen);

    if (conf.verbose) {
        double T = watch.now() - t0;
        fprintf(stderr, "copying into SDL buffer took %f ms\n", T * 1000);
    }

    if (conf.verbose)
        t0 = watch.now();
    SDL_UpdateRect(screen, 0, 0, w, h);
    SDL_Flip(screen);
    if (conf.verbose) {
        double T = watch.now() - t0;
        fprintf(stderr, "SDL_Flip() took %f ms\n", T * 1000);
    }
}

void
Anim::write_screenshot(uint32_t ser, uint32_t id)
{
    std::string fn;
    {
        int ndigits = int(std::ceil(std::log10(std::max(1u, screenshot_max))));
        std::stringstream fnbuilder;
        fnbuilder << "screenshot_";
        fnbuilder << std::setw(3) << std::setfill('0') << int(ser + 1);
        fnbuilder << "_";
        fnbuilder << std::setw(ndigits) << std::setfill('0') << int(id);
        fnbuilder << ".bmp";
        fn = std::move(fnbuilder).str();
    }
    fprintf(stderr, "Writing %s\n", fn.c_str());
    FILE *out = fopen(fn.c_str(), "wb");
    bool ok = false;
    if (out) {
        const auto &img = *renderer.srcImage;
        ok = write_bmp(out, img.w, img.h, img.data());
        if (fclose(out) != 0)
            ok = false;
    }

    if (!ok) {
        fprintf(stderr, "Failed to write file %s\n", fn.c_str());
        return;
    }
}

bool
Anim::handle_event(const SDL_Event &ev)
{
    switch (ev.type) {
    case SDL_QUIT:
        return false;
    case SDL_VIDEORESIZE:
        return resize(ev.resize.w, ev.resize.h);
    // case SDL_VIDEOEXPOSE:
    //     break;
    case SDL_KEYUP:
    case SDL_KEYDOWN:
        return handle_key_event(ev.key);
    }
    return true;
}

bool
Anim::handle_key_event(const SDL_KeyboardEvent &ev)
{
    if (ev.state != SDL_PRESSED)
        return true;
    switch (ev.keysym.sym) {
    case SDLK_ESCAPE:
        return false;
    case SDLK_s:
        if (screenshot_id == 0 && screenshot_max == 0) {
            if (ev.keysym.mod & KMOD_SHIFT) {
                screenshot_max = 3 * conf.fps;
            } else {
                screenshot_max = 1;
            }
        }
        return true;
    default:
        return true;
    }
}

void
Anim::animation()
{
    SDL_Event ev;

    const double fps = conf.fps;
    const double frame_time = 1 / fps;
    double next_frame;
    const double draw_stats_cycle = float(1.5);

    const double start_time = watch.now();
    running = true;
    double draw_stats_next = 0.5f;
    double draw_stats_last = 0;
    uint32_t num_frames = 0;

    while (running) {
        double real_time = watch.now() - start_time;

        if (real_time > draw_stats_next) {
            double diff = real_time - draw_stats_last;
            double frame_time = diff / num_frames;
            fprintf(stderr,
                    "frame time: %lf sec, fps: %lf\n",
                    frame_time,
                    1 / frame_time);
            draw_stats_next = real_time + draw_stats_cycle;
            draw_stats_last = real_time;
            num_frames = 0;
        }

        next_frame = real_time + frame_time;
        if (!renderer.is_capture_mode()) {
            while (SDL_PollEvent(&ev)) {
                if (!handle_event(ev)) {
                    running = false;
                    break;
                }
            }
        } else {
            if (screenshot_id >= screenshot_max) {
                running = false;
                break;
            }
        }

        if (!running)
            break;

        anim_time += frame_time;
        renderer.uniforms.time = anim_time * conf.time_speed + conf.time_t0;
        renderer.trafos.rotation =
          AffineTrafo2::rotation(renderer.uniforms.rot_omega * anim_time);

        render();
        ++num_frames;

        if (!running)
            break;

        sleep(next_frame - watch.now());
    }
}

static void
shutdown_sdl(void)
{
    fprintf(stderr, "shutting down SDL ...\n");
    SDL_Quit();
}

void
Transforms::init(uint32_t w, uint32_t h)
{
    float scale = float(150);

    float invX = scale / float(w);
    float invY = scale / float(h);
    inverseWorld.x = vec2{ invX, 0 };
    inverseWorld.y = vec2{ 0, invY };
    inverseWorld.origin = point2{ -0.5f * scale * vec2{ 1, 1 } };
}

bool
Anim::init()
{
    if (!renderer.init())
        return false;

    if (conf.ncapture > 0)
        screenshot_max = conf.ncapture;

    return resize(conf.img_w, conf.img_h);
}

void
Anim::render()
{
    renderer.render();
    if (!renderer.is_capture_mode())
        draw();

    if (screenshot_id < screenshot_max) {
        write_screenshot(screenshot_ser, screenshot_id);
        screenshot_id++;
        if (screenshot_id >= screenshot_max) {
            screenshot_id = 0;
            screenshot_max = 0;
            screenshot_ser++;
        }
    }
}

int
main(int argc, char *argv[])
{
    auto opt_conf = Config::parse_args(argc, argv);

    if (!opt_conf) {
        fprintf(stderr, "Invalid arguments received\n\n");
        Config::print_usage();
        return 1;
    }

    const auto &conf = *opt_conf;
    printf("Config:\n");
    printf("  verbose:    %s\n", conf.verbose ? "true" : "false");
    printf("  nwaves:     %u\n", unsigned(conf.nwaves));
    printf("  nworkers:   %u\n", unsigned(conf.nworkers));
    printf("  fps:        %u\n", unsigned(conf.fps));
    printf("  image size: %ux%u\n", unsigned(conf.img_w), unsigned(conf.img_h));

    if (SDL_Init(
          conf.ncapture > 0 ? 0 : SDL_INIT_VIDEO | SDL_INIT_EVENTTHREAD) != 0) {
        print_sdl_error("Unable to initialize");
        return 1;
    }

    atexit(shutdown_sdl);

    Anim anim(conf);
    if (!anim.init())
        return 1;

    anim.animation();
    anim.shutdown();

    return 0;
}

void
Anim::shutdown()
{
    renderer.shutdown();
}
