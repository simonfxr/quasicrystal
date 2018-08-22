#pragma once

#include <emmintrin.h>
#include <stdio.h>
#include <xmmintrin.h>

#ifdef USE_SSE4
#include <smmintrin.h>
#endif

#ifdef USE_SSE4
#define ON_SSE4(...) __VA_ARGS__
#define NO_SSE4(...)
#else
#define ON_SSE4(...)
#define NO_SSE4(...) __VA_ARGS__
#endif

typedef __m128 vec4f_data __attribute__((aligned(16)));
typedef __m128i vec4i_data __attribute__((aligned(16)));

struct vecf_t;
struct veci_t;

inline vecf_t
vecf(float);
inline vecf_t
vecf(const float *);
inline vecf_t vecf(vec4f_data);
inline vecf_t vecf(veci_t);

inline veci_t
veci(unsigned);
inline veci_t veci(vec4i_data);
inline veci_t veci(vecf_t);

#define ARG_VEC(arg) (arg).packed
#define ARG(arg) (arg)

#define DEF_VEC_OP(typer, typea, constr, op, arg, func)                        \
    typer operator op(const typea __param) const                               \
    {                                                                          \
        return constr(func(this->packed, arg(__param)));                       \
    }

#define DEF_VEC_SET_OP(typer, typea, op, arg, func)                            \
    typer &operator op(const typea __param)                                    \
    {                                                                          \
        this->packed = func(this->packed, arg(__param));                       \
        return *this;                                                          \
    }

#define DEF_VECF_OP(op, func)                                                  \
    DEF_VEC_OP(vecf_t, vecf_t, vecf, op, ARG_VEC, func)
#define DEF_VECF_SET_OP(op, func)                                              \
    DEF_VEC_SET_OP(vecf_t, vecf_t, op, ARG_VEC, func)

struct vecf_t
{
    vec4f_data packed;

    typedef float elem_type;
    static const unsigned size = 4;
    static const unsigned alignment = 16;

    DEF_VECF_OP(+, _mm_add_ps)
    DEF_VECF_OP(-, _mm_sub_ps)
    DEF_VECF_OP(*, _mm_mul_ps)

    DEF_VECF_SET_OP(+=, _mm_add_ps)
    DEF_VECF_SET_OP(-=, _mm_sub_ps)
    DEF_VECF_SET_OP(*=, _mm_mul_ps)
};

#undef DEF_VECF_OP
#undef DEF_VECF_SET_OP

#define DEF_VECI_OP(op, func)                                                  \
    DEF_VEC_OP(veci_t, veci_t, veci, op, ARG_VEC, func)
#define DEF_VECI_SET_OP(op, func)                                              \
    DEF_VEC_SET_OP(veci_t, veci_t, op, ARG_VEC, func)

inline unsigned __attribute__((always_inline))
vec4i_get_gen(vec4i_data v, int i)
{
    union
    {
        vec4i_data v;
        unsigned f[4];
    } x;

    x.v = v;
    return x.f[i];
}

inline unsigned __attribute__((always_inline)) vec4i_first(vec4i_data v)
{
    union
    {
        float f;
        unsigned i;
    } x;

    _mm_store_ss(&x.f, (vec4f_data) v);
    return x.i;
}

#ifndef USE_SSE4

inline unsigned __attribute__((always_inline)) vec4i_get(vec4i_data v, int i)
{
    //     if (__builtin_constant_p(i)) {

    //         char foo[i >= 0 && i < 4 ? 1 : -1];
    //         ((void) foo);

    //         if (i == 0)
    //             return vec4i_first(v);
    //         else if (i == 1)
    //             return vec4i_first(_mm_shuffle_epi32(v, _MM_SHUFFLE(1, 1, 1,
    //             1)));
    //         else if (i == 2)
    // //            return vec4i_first((vec4i_data) _mm_movehl_ps((vec4f_data)
    // v, (vec4f_data) v));
    //             return vec4i_first(_mm_unpackhi_epi32(v, v));
    //         else
    //             return vec4i_first(_mm_shuffle_epi32(v, _MM_SHUFFLE(3, 3, 3,
    //             3)));
    //     }

    return vec4i_get_gen(v, i);
}

#else

inline unsigned __attribute__((always_inline)) vec4i_get(vec4i_data v, int i)
{
    // if (__builtin_constant_p(i)) {
    //     char foo[i >= 0 && i < 4 ? 1 : -1];
    //     ((void) foo);

    //     if (i == 0)
    //         return vec4i_first(v);
    //     else if (i == 1)
    //         return unsigned(_mm_extract_epi32(v, 1));
    //     else if (i == 2)
    //         return unsigned(_mm_extract_epi32(v, 2));
    //     else
    //         return unsigned(_mm_extract_epi32(v, 3));
    // }

    return vec4i_get_gen(v, i);
}

#endif

struct veci_t
{
    vec4i_data packed;

    typedef unsigned elem_type;
    static const unsigned size = 4;
    static const unsigned alignment = 16;

    DEF_VECI_OP(+, _mm_add_epi32)
    DEF_VECI_OP(-, _mm_sub_epi32)
    DEF_VEC_OP(veci_t, int, veci, >>, ARG, _mm_srli_epi32)
    DEF_VEC_OP(veci_t, int, veci, <<, ARG, _mm_slli_epi32)
    DEF_VECI_OP(|, _mm_or_si128)

    DEF_VECI_SET_OP(+=, _mm_add_epi32)
    DEF_VECI_SET_OP(-=, _mm_sub_epi32)
    DEF_VEC_SET_OP(veci_t, int, >>=, ARG, _mm_srli_epi32)
    DEF_VEC_SET_OP(veci_t, int, <<=, ARG, _mm_slli_epi32)
    DEF_VECI_SET_OP(|=, _mm_or_si128)

    elem_type __attribute__((always_inline)) operator[](unsigned i) const
    {
        return vec4i_get(packed, i);
    }
};

#undef DEF_VECI_OP
#undef DEF_VECI_SET_OP

#undef DEF_VEC_OP
#undef DEF_VEC_SET_OP

inline vecf_t
vecf(float x)
{
    return vecf(_mm_set1_ps(x));
}

inline vecf_t
vecf(const float *data)
{
    vecf_t v = { { data[0], data[1], data[2], data[3] } };
    return v;
}

inline vecf_t
vecf(vec4f_data p)
{
    vecf_t v;
    v.packed = p;
    return v;
}

inline vecf_t
vecf(veci_t i)
{
    return vecf(_mm_cvtepi32_ps(i.packed));
}

inline veci_t
veci(unsigned x)
{
    return veci(_mm_set1_epi32(x));
}

inline veci_t
veci(vec4i_data p)
{
    veci_t v;
    v.packed = p;
    return v;
}

inline veci_t
veci(vecf_t f)
{
    return veci(_mm_cvttps_epi32(f.packed));
}

inline veci_t
coerce_veci(vecf_t v)
{
    veci_t u;
    u.packed = (vec4i_data) v.packed;
    return u;
}

// the fractional part of a number, with a minor glitch: the result may also be
// one
inline vecf_t
fract(const vecf_t v)
{
    return v - vecf(veci(v) - (coerce_veci(v) >> 31));
}

// like fract, but v is assumed to be non negative
inline vecf_t
fract_positive(vecf_t v)
{
    return v - vecf(veci(v));
}

template<typename V, unsigned N = V::size>
struct IndexVector
{
    static void __attribute__((always_inline))
    index(const typename V::elem_type *data,
          typename V::elem_type *value,
          const veci_t i)
    {
        value[V::size - N] = data[i[V::size - N]];
        IndexVector<V, N - 1>::index(data, value, i);
    }
};

template<typename V>
struct IndexVector<V, 0>
{
    static void __attribute__((always_inline))
    index(const typename V::elem_type *, typename V::elem_type *, const veci_t)
    {}
};

inline vecf_t __attribute__((always_inline))
index(const vecf_t::elem_type *data, const veci_t i)
{
    vecf_t::elem_type value[vecf_t::size];
    IndexVector<vecf_t>::index(data, value, i);
    return vecf(value);
}
