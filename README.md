# Fractal Studio - CPU vs CUDA

An interactive Mandelbrot / Julia set explorer built with **Qt 6 (C++17)** that
runs the *same* computation on three different back-ends and compares their
performance live in the UI:

- **CPU (single-threaded)** - a straightforward baseline
- **CPU (multi-threaded)** - the same math parallelized across rows with `std::thread`
- **CUDA (GPU)** - one thread per pixel, computed and colored on the GPU

All three back-ends use identical math and the identical coloring function
(`ColorMap.h`), so their output is **pixel-for-pixel identical** - what you
compare is speed, not results.

![screenshot](docs/screenshot.png)

## Features

- Switch between Mandelbrot and Julia sets (with adjustable Julia constant `c`)
- Mouse-wheel zoom (zooms toward the cursor) and drag-to-pan
- Adjustable iteration count and color palette (Electric / Fire / Ocean)
- **Benchmark all back-ends** button - runs every available back-end on the
  current view and reports each timing plus the speedup relative to the
  single-threaded CPU baseline
- Dark Fusion theme for clean screenshots

## Requirements

- **Qt 6** (Widgets module)
- **CMake 3.20+**
- A C++17 compiler (MSVC 2022 / GCC / Clang)
- For the GPU back-end: **NVIDIA CUDA Toolkit** and an NVIDIA GPU
  (without them the project builds in CPU-only mode - see below)

## Building a Visual Studio solution (Windows, with CUDA)

The easiest path - no hand-written project files, CMake configures Qt's MOC step,
the CUDA build and all include/lib paths for you:

1. Open `generate_vs_solution.bat`, set `QT_DIR` to your Qt installation
   (e.g. `C:\Qt\6.7.2\msvc2022_64`), and save.
2. Double-click `generate_vs_solution.bat`. It produces `build\FractalStudio.sln`.
   (CMake auto-detects your installed Visual Studio, so this works with both
   VS 2022 and VS 2026.)
3. Open `build\FractalStudio.sln`, set **FractalStudio** as the startup project,
   choose **Release | x64**, and build.

The project targets architecture `sm_89` (RTX 40xx / Ada) by default. Change it
by editing `CUDA_ARCH` in the `.bat` (e.g. `86` for Ampere, `75` for Turing).

### Alternative: open the folder directly

Modern Visual Studio can open the project as a CMake folder:
**File → Open → Folder…** and pick the project root. `CMakePresets.json`
provides ready-made *vs2022-cuda* and *vs2022-cpu* configurations (adjust the
Qt path inside the preset first).

### Alternative: command line

```powershell
cmake -B build -S . -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64" -DCUDA_ARCH=89
cmake --build build --config Release
```

## Building without CUDA (CPU only)

If the CUDA Toolkit is not installed, or you want a pure CPU build:

```powershell
cmake -B build -S . -DUSE_CUDA=OFF -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
cmake --build build --config Release
```

The CUDA back-end simply disappears from the UI; the two CPU back-ends remain.

## Usage

1. Pick a fractal and a palette.
2. Scroll to zoom in/out; drag to pan.
3. Switch **Backend** to see each one's render time.
4. Click **Benchmark all backends** for a summary: timings and GPU-vs-CPU speedup.

## Implementation notes

- `Types.h` and `ColorMap.h` are Qt-free and compile under both a normal C++
  compiler and nvcc. The `FR_HD` macro expands to `__host__ __device__` under
  nvcc and to nothing otherwise, so a single `colorize()` function runs on both
  the CPU and the GPU.
- Coloring happens on the device: the GPU returns a finished image, and the
  reported time includes both the kernel and the device-to-host copy - an honest
  "time to get a displayable image".
- The CUDA and Qt layers are kept strictly separate (nvcc and Qt's MOC don't
  mix): the GPU code talks to the app through the Qt-free `CudaFractal.h`
  interface only.
- The iteration math runs in `float`. That's the honest choice on a consumer
  GPU - FP64 is throttled roughly 1:64, so `double` would erase most of the GPU
  advantage. The tradeoff is zoom depth (float bottoms out around 1e-5 of span).

## Possible improvements

- Asynchronous rendering on a worker thread so the UI never blocks on heavy CPU runs
- CUDA–OpenGL interop: render straight from GPU memory with no host copy
- Double precision / perturbation theory for deep zoom
- High-resolution PNG frame export

## License

MIT - do whatever you like, no warranty.
