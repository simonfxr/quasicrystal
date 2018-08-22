#include "BMP.hpp"

#include <cstring>
#include <type_traits>
#include <utility>

// bmp uses little endian encoding!

struct BMPHeader
{
    uint32_t byte_size;
    uint32_t reserved;
    uint32_t payload_byte_offset;
};

static_assert(sizeof(BMPHeader) == 12);

struct BMPInfo
{
    uint32_t info_byte_size;
    int32_t pixel_width;
    int32_t pixel_height;
    uint16_t planes;
    uint16_t bbp;
    uint32_t compression_type;
    uint32_t payload_byte_size;
    int32_t pixelsX_per_meter;
    int32_t pixelsY_per_meter;
    uint32_t color_table_size;
    uint32_t used_colors;
};

static_assert(sizeof(BMPInfo) == 40);

struct Pixel24
{
    uint8_t b, g, r;
};

static_assert(sizeof(Pixel24) == 3);

static const uint16_t MAGIC = 19778; // ascii "BM"

static const unsigned BUF_SIZE = 16 * 1024;

inline bool
is_little_endian()
{
    union
    {
        uint16_t u16;
        uint8_t u8[2];
    } tmp;
    tmp.u16 = 1;
    return tmp.u8[0] != 0;
}

template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
T
encodeLE(T x)
{
    if (is_little_endian())
        return x;
    union
    {
        T value;
        uint8_t bytes[sizeof(T)];
    } buf;
    buf.value = x;

    for (uint32_t i = 0; i < sizeof(T) / 2; ++i) {

        std::swap(buf.bytes[i], buf.bytes[sizeof(T) - 1 - i]);
    }

    return buf.value;
}

#define DEF_ENCODE_LE_SPEC(pre, T)                                             \
    inline T encodeLE##pre(T x) { return encodeLE(x); }

DEF_ENCODE_LE_SPEC(u16, uint16_t)
DEF_ENCODE_LE_SPEC(u32, uint32_t)
DEF_ENCODE_LE_SPEC(i32, int32_t)

static void
convert_pixels(const RGBA *src, Pixel24 *dst, uint32_t size)
{
    bool is_le = is_little_endian();
    for (unsigned i = 0; i < size; ++i) {
        RGBA p = src[i];
        if (is_le) {
            dst[i].b = p.b;
            dst[i].g = p.g;
            dst[i].r = p.r;
        } else {
            dst[i].r = p.b;
            dst[i].g = p.g;
            dst[i].b = p.r;
        }
    }
}

bool
write_bmp(FILE *out, uint32_t w, uint32_t h, const RGBA *pixels)
{
    auto magic = encodeLE(MAGIC);
    if (fwrite(&magic, sizeof magic, 1, out) != 1)
        return false;

    BMPHeader header;
    const auto meta_data_size =
      sizeof(MAGIC) + sizeof(BMPHeader) + sizeof(BMPInfo);
    memset(&header, 0, sizeof header);
    header.byte_size = meta_data_size + w * h * sizeof(Pixel24);
    header.payload_byte_offset = encodeLEu32(meta_data_size);

    if (fwrite(&header, sizeof header, 1, out) != 1)
        return false;

    BMPInfo info;
    memset(&info, 0, sizeof info);
    info.info_byte_size = encodeLEu32(40);
    info.pixel_width = encodeLEi32(w);
    info.pixel_height = encodeLEi32(h);
    info.planes = encodeLEu16(1);
    info.bbp = encodeLEu16(24);

    if (fwrite(&info, sizeof info, 1, out) != 1)
        return false;

    Pixel24 pixel_buf[BUF_SIZE];
    unsigned i;
    for (i = 0; i < w * h / BUF_SIZE; ++i) {
        convert_pixels(pixels + i * BUF_SIZE, pixel_buf, BUF_SIZE);
        if (fwrite(pixel_buf, sizeof pixel_buf, 1, out) != 1)
            return false;
    }

    unsigned offset = i * BUF_SIZE;
    unsigned rem_size = w * h - offset;
    if (rem_size > 0) {
        convert_pixels(pixels + offset, pixel_buf, rem_size);
        if (fwrite(pixel_buf, rem_size * sizeof *pixel_buf, 1, out) != 1)
            return false;
    }

    return true;
}
