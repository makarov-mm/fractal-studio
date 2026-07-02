#pragma once
#include <cstdint>
#include <cmath>

// Smooth coloring shared by both the CPU and CUDA back-ends so that the two
// produce pixel-identical output. The FR_HD macro expands to __host__ __device__
// when compiled by nvcc, and to nothing when compiled by a normal C++ compiler.
#ifdef __CUDACC__
    #define FR_HD __host__ __device__
#else
    #define FR_HD
#endif

FR_HD inline uint32_t fr_packRGB(int r, int g, int b) {
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

// Normalized smooth iteration count (fractional escape "time").
// `power` is the exponent of the iteration formula (2 for z^2 + c).
FR_HD inline float fr_smoothValue(int iter, float zx, float zy, int power) {
    float mag2 = zx * zx + zy * zy;
    if (mag2 < 1.0000001f) mag2 = 1.0000001f;      // keep log() in a safe domain
    float logZn = 0.5f * logf(mag2);               // == log(|z|)
    float nu    = logf(logZn / logf(2.0f)) / logf((float)power);
    return (float)iter + 1.0f - nu;
}

// Number of palettes exposed in the UI. Keep in sync with fr_colorize().
#define FR_PALETTE_COUNT 6

// Map a cyclic parameter t in [0,1) to an RGB color for the chosen scheme.
FR_HD inline uint32_t fr_colorize(float t, int scheme) {
    float r, g, b;
    const float TAU = 6.28318530718f;
    switch (scheme) {
    case 1:                                         // fire
        r = fminf(1.0f, t * 3.0f);
        g = fminf(1.0f, fmaxf(0.0f, t * 3.0f - 1.0f));
        b = fminf(1.0f, fmaxf(0.0f, t * 3.0f - 2.0f));
        break;
    case 2:                                         // ocean
        r = t * t;
        g = t;
        b = sqrtf(fmaxf(0.0f, t));
        break;
    case 3: {                                       // twilight (purple-orange)
        r = 0.5f + 0.5f * cosf(TAU * (t + 0.00f));
        g = 0.5f + 0.5f * cosf(TAU * (t + 0.10f) * 1.0f + 1.0f);
        b = 0.5f + 0.5f * cosf(TAU * (t + 0.20f) + 2.0f);
        break;
    }
    case 4: {                                       // neon (green-magenta)
        r = 0.5f + 0.5f * cosf(TAU * (1.0f * t + 0.30f));
        g = 0.5f + 0.5f * cosf(TAU * (1.0f * t + 0.90f));
        b = 0.5f + 0.5f * cosf(TAU * (2.0f * t + 0.60f));
        break;
    }
    case 5: {                                       // grayscale (triangle wave)
        float s = t * 2.0f;
        r = g = b = (s < 1.0f) ? s : 2.0f - s;
        break;
    }
    default:                                        // electric
        r = 0.5f + 0.5f * cosf(TAU * t + 0.0f);
        g = 0.5f + 0.5f * cosf(TAU * t + 2.094f);
        b = 0.5f + 0.5f * cosf(TAU * t + 4.188f);
        break;
    }
    int R = (int)(fminf(1.0f, fmaxf(0.0f, r)) * 255.0f + 0.5f);
    int G = (int)(fminf(1.0f, fmaxf(0.0f, g)) * 255.0f + 0.5f);
    int B = (int)(fminf(1.0f, fmaxf(0.0f, b)) * 255.0f + 0.5f);
    return fr_packRGB(R, G, B);
}

// Full pipeline from a finished iteration to a pixel: smooth value, palette
// density / offset, palette lookup. Shared verbatim by CPU and GPU.
FR_HD inline uint32_t fr_shade(int iter, int maxIter, float zx, float zy,
                               int power, int scheme,
                               float density, float offset) {
    if (iter >= maxIter)
        return 0xFF000000u;                         // inside the set -> black
    float mu = fr_smoothValue(iter, zx, zy, power);
    float t  = mu * 0.02f * density + offset;
    t -= floorf(t);                                 // cyclic wrap into [0,1)
    return fr_colorize(t, scheme);
}

// Average an ss x ss block of 0xAARRGGBB pixels (box filter used by both the
// CPU and the GPU down-sampling paths when supersampling is enabled).
FR_HD inline uint32_t fr_averageBlock(const uint32_t* src, int srcWidth,
                                      int x0, int y0, int ss) {
    int r = 0, g = 0, b = 0;
    for (int dy = 0; dy < ss; ++dy)
        for (int dx = 0; dx < ss; ++dx) {
            uint32_t c = src[(y0 + dy) * srcWidth + (x0 + dx)];
            r += (int)((c >> 16) & 0xFF);
            g += (int)((c >> 8)  & 0xFF);
            b += (int)( c        & 0xFF);
        }
    int n = ss * ss;
    return fr_packRGB(r / n, g / n, b / n);
}
