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
FR_HD inline float fr_smoothValue(int iter, float zx, float zy) {
    float mag2 = zx * zx + zy * zy;
    if (mag2 < 1.0000001f) mag2 = 1.0000001f;      // keep log() in a safe domain
    float logZn = 0.5f * logf(mag2);               // == log(|z|)
    float nu    = logf(logZn / logf(2.0f)) / logf(2.0f);
    return (float)iter + 1.0f - nu;
}

// Map a cyclic parameter t in [0,1) to an RGB color for the chosen scheme.
FR_HD inline uint32_t fr_colorize(float t, int scheme) {
    float r, g, b;
    const float TAU = 6.28318530718f;
    if (scheme == 1) {                              // fire
        r = fminf(1.0f, t * 3.0f);
        g = fminf(1.0f, fmaxf(0.0f, t * 3.0f - 1.0f));
        b = fminf(1.0f, fmaxf(0.0f, t * 3.0f - 2.0f));
    } else if (scheme == 2) {                       // ocean
        r = t * t;
        g = t;
        b = sqrtf(fmaxf(0.0f, t));
    } else {                                        // electric (default)
        r = 0.5f + 0.5f * cosf(TAU * t + 0.0f);
        g = 0.5f + 0.5f * cosf(TAU * t + 2.094f);
        b = 0.5f + 0.5f * cosf(TAU * t + 4.188f);
    }
    int R = (int)(fminf(1.0f, fmaxf(0.0f, r)) * 255.0f + 0.5f);
    int G = (int)(fminf(1.0f, fmaxf(0.0f, g)) * 255.0f + 0.5f);
    int B = (int)(fminf(1.0f, fmaxf(0.0f, b)) * 255.0f + 0.5f);
    return fr_packRGB(R, G, B);
}
