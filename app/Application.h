#pragma once

#include <stdint.h>
#include <webgpu/webgpu.h>

#include "../gpuLifecycle/GPUContext.h"
#include "../windowLifecycle/WindowContext.h"

#define APP_MAX_LOOP_STEPS 16u

typedef struct AppLoopContext {
    GPUContext* gpu;
    WGPUCommandEncoder encoder;
    WGPURenderPassEncoder renderPass;
} AppLoopContext;

typedef void (*AppLoopStepFn)(AppLoopContext* context, void* userdata);

typedef struct AppLoopStepEntry {
    AppLoopStepFn fn;
    void* userdata;
} AppLoopStepEntry;

typedef struct AppDebugReadbackConfig {
    bool enabled;
    bool printMappedBytes;
    uint64_t bufferSize;
    uint64_t warningFrameThreshold;
    WGPUStringView uploadBufferLabel;
    WGPUStringView readbackBufferLabel;
} AppDebugReadbackConfig;

typedef struct AppConfig {
    WGPUColor clearColor;
    WGPUStringView encoderLabel;
    WGPUStringView commandBufferLabel;
    AppDebugReadbackConfig debugReadback;
} AppConfig;

typedef struct AppState {
    WindowContext* window;
    GPUContext* gpu;
    AppConfig config;

    AppLoopStepEntry preRenderSteps[APP_MAX_LOOP_STEPS];
    uint32_t preRenderStepCount;
    AppLoopStepEntry renderSteps[APP_MAX_LOOP_STEPS];
    uint32_t renderStepCount;
    AppLoopStepEntry postRenderSteps[APP_MAX_LOOP_STEPS];
    uint32_t postRenderStepCount;

    WGPUBuffer debugUploadBuffer;
    WGPUBuffer debugReadbackBuffer;

    bool readbackRequested;
    bool readbackCompleted;
    bool readbackTimeoutLogged;
    uint64_t framesSinceReadbackRequest;
} AppState;

AppConfig appDefaultConfig(void);
bool appInit(AppState* app, const AppConfig* config);
void appRun(AppState* app);
void appShutdown(AppState* app);
bool appRunFrame(AppState* app);
bool appAddPreRenderStep(AppState* app, AppLoopStepFn fn, void* userdata);
bool appAddRenderStep(AppState* app, AppLoopStepFn fn, void* userdata);
bool appAddPostRenderStep(AppState* app, AppLoopStepFn fn, void* userdata);
WGPULimits getRequiredLimits(WGPUAdapter adapter);
void setDefault(WGPULimits &limits);
