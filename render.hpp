#pragma once

#include "BMP.hpp"
#include "Config.hpp"
#include "euclidean2d.hpp"

#include <cassert>
#include <cstdint>
#include <memory>
#include <queue>
#include <vector>

struct Worker;

void
delete_worker(Worker *);

namespace std {
template<>
struct default_delete<Worker>
{
    void operator()(Worker *x) const { delete_worker(x); }
};
} // namespace std

struct Image
{
    uint32_t w, h;
    std::unique_ptr<RGBA[]> _data;

    void init(uint32_t w, uint32_t h, uint32_t block_size)
    {
        this->w = w;
        this->h = h;
        auto img_size = CEIL_DIV(w * h, block_size) * block_size;
        _data.reset(new RGBA[img_size]);
    }

    RGBA &operator()(uint32_t x, uint32_t y)
    {
        return _data[INDEX_2D(x, y, w)];
    }

    const RGBA &operator()(uint32_t x, uint32_t y) const
    {
        return _data[INDEX_2D(x, y, w)];
    }

    const RGBA *data() const { return _data.get(); }

    RGBA *data() { return _data.get(); }
};

struct Rect
{
    uint32_t offset; // gets mapped to 2 dim later
    uint32_t size;
    Rect(uint32_t _offset, uint32_t _size) : offset(_offset), size(_size) {}
    Rect() {}
};

struct Transforms
{
    AffineTrafo2 inverseWorld;
    AffineTrafo2 rotation;

    void init(uint32_t w, uint32_t h);
};

struct Uniforms
{
    std::vector<float> sincos_table;
    std::vector<float> cosine_table;
    double time = 0;
    float rot_omega = 0;

    void init(uint32_t nangles, uint32_t ncosines);

    uint32_t num_angles() const { return sincos_table.size() / 2; }

    uint32_t num_cosines() const { return cosine_table.size() - 1; }
};

struct Renderer
{
    const Config &conf;
    Uniforms uniforms;
    Transforms trafos;
    Image images[2];
    Image *srcImage = nullptr;
    uint64_t render_version = 0;
    uint64_t srcVersion = 0;

    std::vector<std::unique_ptr<Worker>> workers;

    Renderer(const Config &conf) : conf(conf) {}

    bool init();
    bool init_workers();
    void shutdown();

    void start_new_frame();
    void render();
    bool save_screenshot(const char *path);

    bool is_capture_mode() const { return conf.ncapture > 0; }
};
