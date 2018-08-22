#include "render.hpp"

#include "euclidean2d.hpp"
#include "simd_vec.hpp"
#include "utils.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <condition_variable>
#include <mutex>

const uint32_t TILE_SIZE = CEIL_DIV(4 * 4096, sizeof(RGBA));

struct Barrier
{
    std::mutex mutex;
    std::condition_variable condition;
    bool open = false;

    void wait();
    void notify();
};

struct Worker
{

    enum State
    {
        New,
        Started,
        Initialized,
        ShuttingDown,
        Stopped
    };

    const Config &conf;
    State state = New;
    const int id;
    Renderer &renderer;
    std::thread thread;
    Barrier start_frame_barrier;
    Barrier done_frame_barrier;
    volatile bool shutdown = false;

    Image *image = nullptr;
    uint64_t version = 0;

    std::mutex mutex;

    volatile int done_flag;

    std::queue<Rect> jobs;

    Worker(const Config &conf, Renderer &renderer, int id)
      : conf(conf), id(id), renderer(renderer)
    {}

    bool start();
    bool wait_shutdown();
    void render_rects();

    bool is_coordinator() const { return id == 0; }

    bool get_work(Rect &rect);

    static int run_thread(void *);

    void run();

    void lock()
    {
        if (conf.nworkers > 1)
            mutex.lock();
    }

    void unlock()
    {
        if (conf.nworkers > 1)
            mutex.unlock();
    }
};

bool
Worker::start()
{
    dbg_assert(state == New);
    thread = std::thread(Worker::run_thread, this);
    state = Started;
    return true;
}

bool
Worker::wait_shutdown()
{
    dbg_assert(state != New);
    state = Stopped;
    thread.join();
    return true;
}

template<typename T>
bool
queue_get(std::queue<T> &q, T &res)
{
    if (q.empty())
        return false;
    res = q.front();
    q.pop();
    return true;
}

static void
enqueue_tiles(std::queue<Rect> &q, const Rect &rect)
{
    dbg_assert(rect.size % TILE_SIZE == 0);
    uint32_t ntiles = rect.size / TILE_SIZE;
    for (uint32_t i = 0; i < ntiles; ++i)
        q.push(Rect(rect.offset + i * TILE_SIZE, TILE_SIZE));
}

bool
Worker::get_work(Rect &rect)
{
    bool success = false;
    const auto nworkers = conf.nworkers;

    lock();
    success = queue_get(jobs, rect);
    if (success && rect.size > TILE_SIZE) {
        assert(jobs.empty());
        enqueue_tiles(jobs,
                      Rect(rect.offset + TILE_SIZE, rect.size - TILE_SIZE));
        rect.size = TILE_SIZE;
    }
    unlock();

    if (success)
        return true;

    // steal from other workers
    uint32_t me = id;
    for (uint32_t i = (me + 1) % nworkers; i != me; i = (i + 1) % nworkers) {
        Worker &w = *renderer.workers[i];

        w.lock();
        success = !w.done_flag && queue_get(w.jobs, rect);
        w.unlock();
        if (success) {
            if (rect.size > TILE_SIZE) {
                lock();
                enqueue_tiles(
                  jobs, Rect(rect.offset + TILE_SIZE, rect.size - TILE_SIZE));
                unlock();
                ;
            }
            rect.size = TILE_SIZE;
            return true;
        }
    }

    lock();
    done_flag = true;
    unlock();

    return false;
}

static vecf_t __attribute__((always_inline))
eval_cosine(const Uniforms &us, vecf_t t)
{

    // union {
    //     vecf_t::elem_type flat[vecf_t::size];
    //     vecf_t packed;
    // } x;

    // x.packed = t;
    // for (uint32_t i = 0; i < vecf_t::size; ++i)
    //     x.flat[i] = cosf(x.flat[i]);

    // return x.packed;

    const vecf_t num_cosines = vecf(float(us.num_cosines()));
    const vecf_t inv_tau = vecf(float(1.0 / (2.0 * M_PI)));

    t = fract(t * inv_tau);
    t *= num_cosines;

    veci_t i = veci(t);
    // veci_t j = i + veci(1);
    // vecf_t frac = t - vecf(i);

    const vecf_t a = index(us.cosine_table.data(), i);
    // const vecf_t b = index(us->cosine_table, j);

    return a;
    // return a + (b - a) * frac;
}

static void __attribute__((noinline))
transform_points(const AffineTrafo2 &RESTRICT M,
                 uint32_t n,
                 vecf_t *RESTRICT x,
                 vecf_t *RESTRICT y)
{
    const vecf_t x_off = vecf(M.origin.coords.x);
    const vecf_t y_off = vecf(M.origin.coords.y);
    const vecf_t m11 = vecf(M.x.x);
    const vecf_t m12 = vecf(M.y.x);
    const vecf_t m21 = vecf(M.x.y);
    const vecf_t m22 = vecf(M.y.y);

    for (uint32_t i = 0; i < n; ++i) {
        vecf_t vx = x[i];
        vecf_t vy = y[i];
        x[i] = vx * m11 + vy * m12 + x_off;
        y[i] = vx * m21 + vy * m22 + y_off;
    }
}

static void __attribute__((noinline))
calculate_amplitudes(const Uniforms &us,
                     const uint32_t n,
                     const vecf_t *RESTRICT x,
                     const vecf_t *RESTRICT y,
                     vecf_t *RESTRICT amp)
{
    const vecf_t init_amp = vecf(float(us.num_angles()));

    for (uint32_t i = 0; i < n; ++i)
        amp[i] = init_amp;

    const vecf_t time = vecf(us.time);

    for (uint32_t a = 0; a < us.num_angles(); ++a) {
        vecf_t scale_y = vecf(us.sincos_table[2 * a]);
        vecf_t scale_x = vecf(us.sincos_table[2 * a + 1]);

        for (uint32_t k = 0; k < n; ++k) {
            vecf_t t = time;
            t += x[k] * scale_x;
            t += y[k] * scale_y;
            amp[k] += eval_cosine(us, t);
        }
    }
}

#if 0
static float
fractf(float x)
{
    if (x < 0)
        x = -x;
    return x - float(int(x));
}

static vec2
cart2polar(float x, float y)
{
    float r = sqrtf(x * x + y * y);
    float phi;

    if (r == 0) {
        phi = 0;
    } else if (y == 0 && x < 0) {
        phi = 0.5;
    } else {
        phi = atan2f(y, r + x) * (1 / M_PI);
    }

    phi *= float(2 * M_PI);
    vec2 v;
    v.x = phi;
    v.y = r;
    return v;
}
#endif

static float
sqr(float x)
{
    return x * x;
}

static void __attribute__((always_inline))
warp_world(uint32_t n, vecf_t *RESTRICT x, vecf_t *RESTRICT y)
{
    for (uint32_t i = 0; i < n; ++i) {

        union
        {
            vecf_t::elem_type flat[vecf_t::size];
            vecf_t packed;
        } x0, y0, x1, y1;

        x0.packed = x[i];
        y0.packed = y[i];

        const float dt = 0.08;
        const float scale = 0.5;

        for (uint32_t j = 0; j < vecf_t::size; ++j) {
            float x = x0.flat[j];
            float y = y0.flat[j];
            float xt, yt;

            float fx, fy;
            float r = sqrtf(sqr(x * (1 / 7)) + sqr(y * (1 / 2)));
            float d = r * 0.5 + cosf(r * 0.4) * 0.01;
            float invR = float(1) / (r + float(0.001));
            fx = d * invR * x;
            fy = d * invR * y;
            xt = x;
            yt = y;

            x1.flat[j] = (xt + fx * dt) * scale;
            y1.flat[j] = (yt + fy * dt) * scale;
        }

        x[i] = x1.packed;
        y[i] = y1.packed;
    }
}

static void __attribute__((noinline)) draw_crystal(const Uniforms &us,
                                                   const Transforms &trafos,
                                                   const Rect &RESTRICT rect,
                                                   Image &img)
{

    dbg_assert(rect.size == TILE_SIZE);
    dbg_assert(img.w % 4 == 0);
    dbg_assert(rect.offset % TILE_SIZE == 0);
    dbg_assert(TILE_SIZE % veci_t::size == 0 && TILE_SIZE % vecf_t::size == 0);

    const uint32_t y0 = rect.offset / img.w;
    const uint32_t x0 = rect.offset % img.w;

    const uint32_t yn = (rect.offset + rect.size) / img.w;
    const uint32_t xn = (rect.offset + rect.size) % img.w;

    dbg_assert(xn % 4 == 0);

    union
    {
        RGBA *data;
        veci_t *packed;
    } color_buf;

    color_buf.data = &img(x0, y0);
    dbg_assert((uintptr_t) color_buf.packed % sizeof(vecf_t) == 0);

    const auto TILE_SIZE4 = TILE_SIZE / vecf_t::size;
    vecf_t xcoord[TILE_SIZE4];
    vecf_t ycoord[TILE_SIZE4];

    {
        uint32_t off4 = 0;
        const uint32_t w4 = img.w / vecf_t::size;
        dbg_assert(img.w % vecf_t::size == 0);

        vecf_t::elem_type x_init_data[vecf_t::size];
        for (uint32_t i = 0; i < vecf_t::size; ++i)
            x_init_data[i] = float(i);

        const vecf_t x_init = vecf(x_init_data);
        const vecf_t x_step = vecf(float(vecf_t::size));
        const vecf_t y_step = vecf(float(1));

        vecf_t xf = vecf(float(x0)) + x_init;
        vecf_t yf = vecf(float(y0));
        auto yfirst = (x0 == 0 && y0 < yn) ? y0 : y0 + 1;

        if (y0 < yfirst) {
            for (uint32_t i = 0; i < std::min(TILE_SIZE4, w4 - x0 / 4);
                 ++i, ++off4) {
                xcoord[off4] = xf;
                ycoord[off4] = yf;
                xf += x_step;
            }
        }

        yf += y_step;
        for (uint32_t y = yfirst; y < yn; ++y) {
            xf = x_init;
            for (uint32_t i = 0; i < w4; ++i, ++off4) {
                xcoord[off4] = xf;
                ycoord[off4] = yf;
                xf += x_step;
            }
            yf += y_step;
        }

        if (yn > y0) {
            xf = x_init;
            for (uint32_t i = 0; i < xn / 4; ++i, ++off4) {
                xcoord[off4] = xf;
                ycoord[off4] = yf;
                xf += x_step;
            }
        }

        dbg_assert(off4 == TILE_SIZE4);
    }

    auto trans = trafos.rotation * trafos.inverseWorld;
    transform_points(trans, TILE_SIZE / vecf_t::size, xcoord, ycoord);
    warp_world(TILE_SIZE / vecf_t::size, xcoord, ycoord);

    vecf_t amp[TILE_SIZE4];
    calculate_amplitudes(us, TILE_SIZE / vecf_t::size, xcoord, ycoord, amp);

    const vecf_t max_lum = vecf(float(255));
    const veci_t alpha = veci((int) (255U << 24));

    for (uint32_t i = 0; i < TILE_SIZE4; ++i) {
        vecf_t x = amp[i] * vecf(float(0.5));
        vecf_t t = fract_positive(x);

        vecf_t lum = t * t * (vecf(3) - vecf(2) * t) * max_lum;
        veci_t lumi = veci(lum);

        lumi = lumi | (lumi << 8) | (lumi << 16);
        color_buf.packed[i] = lumi | alpha;
    }
}

void
Worker::render_rects()
{
    done_flag = false;

    if (!is_coordinator())
        start_frame_barrier.wait();
    else
        ++renderer.render_version;

    if (shutdown)
        return;

    image = &renderer.images[image == &renderer.images[0]];
    ++version;

    for (;;) {

        Rect rect;
        if (!get_work(rect))
            break;
        draw_crystal(renderer.uniforms, renderer.trafos, rect, *image);
    }

    if (!is_coordinator())
        done_frame_barrier.notify();
}

int
Worker::run_thread(void *w)
{
    static_cast<Worker *>(w)->run();
    return 0;
}

void
Worker::run()
{
    dbg_assert(!is_coordinator());
    while (!shutdown)
        render_rects();
    fprintf(stderr, "worker %d exiting\n", id);
}

void
Uniforms::init(uint32_t nangles, uint32_t ncosines)
{
    rot_omega = float(0);

    sincos_table.resize(2 * nangles);
    for (uint32_t i = 0; i < nangles; ++i) {
        float theta = float(i) * float(2 * M_PI / nangles);
        sincos_table[i * 2] = std::sin(theta);
        sincos_table[i * 2 + 1] = std::cos(theta);
    }

    // we could take advantage of symmetry
    // also most precision is needed near the zeroes, so it would be
    // beneficial to stretch the domain accordingly
    cosine_table.resize(ncosines + 1);

    for (uint32_t i = 0; i < ncosines; ++i)
        cosine_table[i] = cosf(float(i) * float(2 * M_PI / ncosines));

    cosine_table[ncosines] = cosine_table[0]; // wrap around
}

void
Renderer::start_new_frame()
{
    uint32_t ntiles = CEIL_DIV(conf.img_w * conf.img_h, TILE_SIZE);
    uint32_t slice = ntiles / conf.nworkers;
    uint32_t rest = ntiles % conf.nworkers;
    uint32_t offset = 0;

    for (uint32_t i = 0; i < conf.nworkers; ++i) {
        // invariant: Workers are all blocked on work_barrier
        // we still need to lock, to ensure proper memory ordering
        uint32_t sz =
          TILE_SIZE * (i >= conf.nworkers - rest ? slice + 1 : slice);
        if (sz > 0) {
            workers[i]->lock();
            workers[i]->jobs.push(Rect(offset, sz));
            workers[i]->unlock();
        }
        offset += sz;
    }

    for (uint32_t i = 1; i < conf.nworkers; ++i)
        workers[i]->start_frame_barrier.notify();
}

void
Renderer::render()
{
    workers[0]->render_rects();

    for (uint32_t i = 1; i < conf.nworkers; ++i)
        workers[i]->done_frame_barrier.wait();

#ifdef DEBUG_IMAGE_VERSION
    for (uint32_t i = 1; i < conf.nworkers; ++i) {
        workers[i]->lock();
        assert(workers[i]->version == workers[0]->version);
        assert(workers[i]->image == workers[0]->image);
        workers[i]->unlock();
    }
#endif

    start_new_frame();

    int imidx = (workers[0]->image == &images[0]) ? 0 : 1;
    assert(srcImage != &images[imidx]);
    srcImage = &images[imidx];
    assert(srcVersion + 1 == render_version);
    srcVersion = render_version;
}

bool
Renderer::init_workers()
{
    for (uint32_t i = 0; i < conf.nworkers; ++i)
        workers.push_back(std::make_unique<Worker>(conf, *this, i));

    uint32_t i;

    // dont start thread 0, thread 0's work will be executed by the
    // main thread
    for (i = 1; i < conf.nworkers; ++i)
        if (!workers[i]->start())
            goto shutdown;

    start_new_frame();
    return true;

shutdown:

    for (uint32_t k = 1; k < i; ++k) {
        workers[k]->shutdown = true;
        workers[k]->start_frame_barrier.notify();
    }

    for (uint32_t k = 1; k < i; ++k)
        workers[k]->wait_shutdown();

    return false;
}

void
Renderer::shutdown()
{
    for (uint32_t i = 1; i < conf.nworkers; ++i) {
        workers[i]->shutdown = true;
        workers[i]->start_frame_barrier.notify();
    }

    for (uint32_t i = 1; i < conf.nworkers; ++i)
        workers[i]->wait_shutdown();
}

bool
Renderer::init()
{
    uint32_t img_w = conf.img_w;
    uint32_t img_h = conf.img_h;

    images[0].init(img_w, img_h, TILE_SIZE);
    images[1].init(img_w, img_h, TILE_SIZE);

    uniforms.init(conf.nwaves, conf.ncosines);
    trafos.init(img_w, img_h);

    return init_workers();
}

void
delete_worker(Worker *w)
{
    delete w;
}

void
Barrier::wait()
{
    std::unique_lock lk(mutex);
    condition.wait(lk, [this] { return open; });
    open = false;
}

void
Barrier::notify()
{
    {
        std::lock_guard lk(mutex);
        open = true;
    }
    condition.notify_one();
}
