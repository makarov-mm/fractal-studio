cmake_minimum_required(VERSION 3.20)
project(FractalStudio LANGUAGES CXX)

# Build with CUDA acceleration by default. Turn OFF to build a CPU-only version
# (the CUDA backend simply disappears from the UI):  cmake -DUSE_CUDA=OFF ...
option(USE_CUDA "Build with CUDA GPU acceleration" ON)

# Target GPU architecture. 89 = Ada (RTX 40xx). 86 = Ampere (RTX 30xx),
# 75 = Turing (RTX 20xx). Override with -DCUDA_ARCH=86 etc.
set(CUDA_ARCH "89" CACHE STRING "CUDA compute capability")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/FractalWidget.cpp
    src/FractalRenderer.cpp
    src/RenderWorker.cpp
)
set(HEADERS
    src/MainWindow.h
    src/FractalWidget.h
    src/FractalRenderer.h
    src/RenderWorker.h
    src/Types.h
    src/ColorMap.h
    src/FractalMath.h
)

if(USE_CUDA)
    enable_language(CUDA)
    list(APPEND SOURCES src/CudaFractal.cu)
    list(APPEND HEADERS src/CudaFractal.h)
endif()

add_executable(FractalStudio ${SOURCES} ${HEADERS})
target_link_libraries(FractalStudio PRIVATE Qt6::Widgets)

# Source files contain UTF-8 string literals; tell MSVC explicitly
# (only for C++ translation units - nvcc gets its host flags separately).
if(MSVC)
    target_compile_options(FractalStudio PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/utf-8>)
endif()

if(USE_CUDA)
    target_compile_definitions(FractalStudio PRIVATE HAVE_CUDA)
    set_target_properties(FractalStudio PROPERTIES
        CUDA_ARCHITECTURES "${CUDA_ARCH}"
        CUDA_SEPARABLE_COMPILATION OFF)
endif()

# Copy required Qt runtime DLLs next to the .exe on Windows (best-effort).
if(WIN32)
    find_program(WINDEPLOYQT_EXE windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT_EXE)
        add_custom_command(TARGET FractalStudio POST_BUILD
            COMMAND "${WINDEPLOYQT_EXE}" --no-translations "$<TARGET_FILE:FractalStudio>"
            COMMENT "Deploying Qt runtime with windeployqt")
    endif()
endif()
