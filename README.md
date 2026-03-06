# em_sim

A small C++ WebGPU sandbox that opens a GLFW window and renders a color-cleared frame using Dawn.

## What This Repo Contains

- `main.cpp`: Application loop and frame submission.
- `gpuLifecycle/`: WebGPU instance/adapter/device/surface setup and render-pass helpers.
- `windowLifecycle/`: GLFW window lifecycle.
- `docs/Fixes_for_gpu_init.md`: Notes about surface configuration and init-order fixes.
- `dawn/`: Dawn source (added as a local subdirectory in CMake).
- `glfw/`: GLFW source.
- `glfw3webgpu/`: GLFW-to-WebGPU surface bridge.

## Prerequisites

- CMake `>= 3.29`
- C++20 compiler (Xcode/Clang on macOS is expected)
- Git

## Build

From the repository root:

```bash
cmake -S . -B build
cmake --build build -j
```

## Run

```bash
./build/em_sim
```

## Current Platform/Config Notes

- `CMakeLists.txt` currently forces `arm64` via:
  - `set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)`
- The active backend is Dawn (`WEBGPU_BACKEND_DAWN`).
- There is commented-out Emscripten support in `CMakeLists.txt` and source, but it is not active in the current build configuration.

## Runtime Behavior

- Creates a `640x480` window titled `EM Sim`.
- Configures the surface and renders each frame by clearing to a color that gradually shifts the green channel.

## Known Limitations

- No shaders/pipeline yet; rendering is currently clear-pass only.
- Surface is configured with fixed `640x480` dimensions and does not yet handle resize/reconfigure.
- Minimal shutdown path in `main.cpp` (no explicit `windowShutdown()` / `gpuShutdown()` call).

## Troubleshooting

If you see errors such as:

- `Surface is not configured`
- `Render pass has no attachments`
- `Invalid CommandBuffer`

check `docs/Fixes_for_gpu_init.md` for the expected initialization and frame-order sequence.
