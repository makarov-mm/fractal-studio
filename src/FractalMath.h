#pragma once
#include "Types.h"

// The escape-time iteration shared by every back-end. This header is Qt-free
// and compiles under both a normal C++ compiler and nvcc; the FR_HD macro
// expands to __host__ __device__ under nvcc and to nothing otherwise.
//
// The function is templated on the floating-point type so the same source
// provides the float path (fast, shallow zoom) and the double path (deep
// zoom; on consumer GPUs FP64 is heavily throttled, which the benchmark
// makes visible).

#ifdef __CUDACC__
    #define FR_HD __host__ __device__
#else
    #define FR_HD
#endif

template <typename T>
FR_HD inline T fr_abs(T v) { return v < (T)0 ? -v : v; }

// Runs the escape-time loop for one point. (cx, cy) is the additive constant,
// (zx, zy) is the starting z and receives the final z on exit (needed for
// smooth coloring). Returns the iteration count; iter == maxIter means the
// point did not escape.
template <typename T>
FR_HD inline int fr_iterate(int type, int power, int maxIter,
                            T cx, T cy, T& zx, T& zy) {
    const T BAILOUT = (T)65536.0;
    int iter = 0;

    while (iter < maxIter) {
        T nx, ny;
        switch (type) {
        case (int)FractalType::BurningShip: {
            T ax = fr_abs(zx), ay = fr_abs(zy);
            nx = ax * ax - ay * ay + cx;
            ny = (T)2.0 * ax * ay + cy;
            break;
        }
        case (int)FractalType::Tricorn:
            nx = zx * zx - zy * zy + cx;
            ny = (T)-2.0 * zx * zy + cy;
            break;
        case (int)FractalType::Multibrot: {
            // z^power by repeated complex multiplication (power is small).
            T wx = zx, wy = zy;
            for (int k = 1; k < power; ++k) {
                T tx = wx * zx - wy * zy;
                wy   = wx * zy + wy * zx;
                wx   = tx;
            }
            nx = wx + cx;
            ny = wy + cy;
            break;
        }
        default: // Mandelbrot and Julia share z^2 + c
            nx = zx * zx - zy * zy + cx;
            ny = (T)2.0 * zx * zy + cy;
            break;
        }
        zx = nx; zy = ny;
        if (zx * zx + zy * zy > BAILOUT) break;
        ++iter;
    }
    return iter;
}

// Effective exponent of the iteration formula, used as the log base of the
// smooth-coloring formula (2 for everything except Multibrot).
FR_HD inline int fr_effectivePower(int type, int power) {
    return (type == (int)FractalType::Multibrot) ? power : 2;
}
