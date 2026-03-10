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

typedef struct AppConfig {
    WGPUColor clearColor;
    uint64_t stagingBufferSize;
    uint64_t readbackWarningFrameThreshold;
    WGPUStringView encoderLabel;
    WGPUStringView commandBufferLabel;
    WGPUStringView stagingBufferLabel;
    WGPUStringView readbackBufferLabel;
} AppConfig;

typedef struct AppState {
    WindowContext* window;
    GPUContext* gpu;
    AppConfig config;

    AppLoopStepEntry loopSteps[APP_MAX_LOOP_STEPS];
    uint32_t loopStepCount;

    WGPUBuffer uploadBuffer;
    WGPUBuffer readbackBuffer;

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
bool appAddLoopStep(AppState* app, AppLoopStepFn fn, void* userdata);
