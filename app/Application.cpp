#include "Application.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../resourceManagement/BufferManager.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static void appPollWebGPUEvents(WGPUDevice device, bool yieldToWebBrowser) {
#if defined(WEBGPU_BACKEND_DAWN)
    wgpuDeviceTick(device);
#elif defined(WEBGPU_BACKEND_WGPU)
    wgpuDevicePoll(device, false, NULL);
#elif defined(WEBGPU_BACKEND_EMSCRIPTEN)
    if (yieldToWebBrowser) {
        emscripten_sleep(100);
    }
#else
    (void)device;
    (void)yieldToWebBrowser;
#endif
}

static void appOnReadbackMapped(
        WGPUMapAsyncStatus status,
        WGPUStringView message,
        void* userdata1,
        void* userdata2) {
    AppState* app = (AppState*)userdata1;
    (void)userdata2;
    app->readbackCompleted = true;

    if (status != WGPUMapAsyncStatus_Success) {
        fprintf(stderr, "Readback map failed. Status: %d\n", (int)status);
        if (message.data != NULL) {
            fprintf(stderr, "Reason: %.*s\n", (int)message.length, message.data);
        }
        return;
    }

    const uint8_t* bufferData =
            (const uint8_t*)wgpuBufferGetConstMappedRange(app->readbackBuffer, 0, app->config.stagingBufferSize);
    printf("bufferData = [");
    for (uint64_t i = 0; i < app->config.stagingBufferSize; ++i) {
        if (i > 0) {
            printf(", ");
        }
        printf("%d", (int)bufferData[i]);
    }
    printf("]\n");

    wgpuBufferUnmap(app->readbackBuffer);
}

static void appOnReadbackCopySubmitted(
        WGPUQueueWorkDoneStatus status,
        WGPUStringView message,
        void* userdata1,
        void* userdata2) {
    AppState* app = (AppState*)userdata1;
    (void)userdata2;

    if (status != WGPUQueueWorkDoneStatus_Success) {
        fprintf(stderr, "Readback copy completion callback failed. Status: %d\n", (int)status);
        if (message.data != NULL) {
            fprintf(stderr, "Reason: %.*s\n", (int)message.length, message.data);
        }
        return;
    }

    WGPUBufferMapCallbackInfo mapCallbackInfo = {};
    mapCallbackInfo.nextInChain = NULL;
    mapCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    mapCallbackInfo.callback = appOnReadbackMapped;
    mapCallbackInfo.userdata1 = app;
    mapCallbackInfo.userdata2 = NULL;
    wgpuBufferMapAsync(
            app->readbackBuffer, WGPUMapMode_Read, 0, app->config.stagingBufferSize, mapCallbackInfo);
}

static bool appBeginFrame(AppState* app, WGPUSurfaceTexture* surfaceTexture, WGPUTextureView* textureView) {
    *textureView = NULL;
    getNextSurfaceViewData(surfaceTexture, textureView);
    return *textureView != NULL;
}

static WGPUCommandEncoder appCreateCommandEncoder(const AppState* app) {
    WGPUCommandEncoderDescriptor encoderDesc = {};
    encoderDesc.nextInChain = NULL;
    encoderDesc.label = app->config.encoderLabel;
    return wgpuDeviceCreateCommandEncoder(app->gpu->device, &encoderDesc);
}

static WGPURenderPassEncoder appBeginRenderPass(
        const AppState* app,
        WGPUCommandEncoder encoder,
        WGPUTextureView textureView) {
    WGPURenderPassColorAttachment colorAttachment = {};
    colorAttachment.view = textureView;
    colorAttachment.resolveTarget = NULL;
    colorAttachment.loadOp = WGPULoadOp_Clear;
    colorAttachment.storeOp = WGPUStoreOp_Store;
    colorAttachment.clearValue = app->config.clearColor;
#ifndef WEBGPU_BACKEND_WGPU
    colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif

    WGPURenderPassDescriptor passDesc = {};
    passDesc.nextInChain = NULL;
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &colorAttachment;
    passDesc.depthStencilAttachment = NULL;
    passDesc.timestampWrites = NULL;

    return wgpuCommandEncoderBeginRenderPass(encoder, &passDesc);
}

static void appRunLoopSteps(AppState* app, WGPUCommandEncoder encoder, WGPURenderPassEncoder pass) {
    if (app->loopStepCount == 0) {
        wgpuRenderPassEncoderSetPipeline(pass, app->gpu->pipeline);
        wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
        return;
    }

    AppLoopContext ctx = {};
    ctx.gpu = app->gpu;
    ctx.encoder = encoder;
    ctx.renderPass = pass;

    for (uint32_t i = 0; i < app->loopStepCount; ++i) {
        AppLoopStepEntry* entry = &app->loopSteps[i];
        if (entry->fn != NULL) {
            entry->fn(&ctx, entry->userdata);
        }
    }
}

static void appRequestReadback(AppState* app) {
    if (app->readbackRequested || app->readbackBuffer == NULL) {
        return;
    }

    app->readbackRequested = true;
    app->readbackTimeoutLogged = false;
    app->framesSinceReadbackRequest = 0;

    WGPUQueueWorkDoneCallbackInfo callbackInfo = {};
    callbackInfo.nextInChain = NULL;
    callbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    callbackInfo.callback = appOnReadbackCopySubmitted;
    callbackInfo.userdata1 = app;
    callbackInfo.userdata2 = NULL;
    wgpuQueueOnSubmittedWorkDone(app->gpu->queue, callbackInfo);
}

static void appPollDeviceAndEvents(const AppState* app) {
    glfwPollEvents();
    wgpuInstanceProcessEvents(app->gpu->instance);
    appPollWebGPUEvents(app->gpu->device, true);
}

static void appReportReadbackTimeoutIfNeeded(AppState* app) {
    if (!app->readbackRequested || app->readbackCompleted) {
        return;
    }

    ++app->framesSinceReadbackRequest;
    if (app->config.readbackWarningFrameThreshold == 0) {
        return;
    }

    if (!app->readbackTimeoutLogged &&
        app->framesSinceReadbackRequest >= app->config.readbackWarningFrameThreshold) {
        fprintf(
                stderr,
                "Readback still pending after %llu frames. Continuing without blocking.\n",
                (unsigned long long)app->framesSinceReadbackRequest);
        app->readbackTimeoutLogged = true;
    }
}

static bool appCreateBuffers(AppState* app) {
    WGPUBufferDescriptor bufferDesc = {};
    populateBufferDesc(
            &bufferDesc,
            app->config.stagingBufferLabel,
            WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc,
            (uint32_t)app->config.stagingBufferSize);
    app->uploadBuffer = createBuffer(&app->gpu->device, &bufferDesc);
    if (app->uploadBuffer == NULL) {
        fprintf(stderr, "Failed to create upload buffer.\n");
        return false;
    }

    bufferDesc.label = app->config.readbackBufferLabel;
    bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    app->readbackBuffer = createBuffer(&app->gpu->device, &bufferDesc);
    if (app->readbackBuffer == NULL) {
        fprintf(stderr, "Failed to create readback buffer.\n");
        return false;
    }

    uint8_t* numbers = (uint8_t*)malloc((size_t)app->config.stagingBufferSize);
    if (numbers == NULL) {
        fprintf(stderr, "Failed to allocate staging data.\n");
        return false;
    }

    for (uint64_t i = 0; i < app->config.stagingBufferSize; ++i) {
        numbers[i] = (uint8_t)(i & 0xFFu);
    }
    wgpuQueueWriteBuffer(app->gpu->queue, app->uploadBuffer, 0, numbers, (size_t)app->config.stagingBufferSize);
    free(numbers);

    return true;
}

static void appReleaseBuffers(AppState* app) {
    if (app->uploadBuffer != NULL) {
        wgpuBufferRelease(app->uploadBuffer);
        app->uploadBuffer = NULL;
    }
    if (app->readbackBuffer != NULL) {
        wgpuBufferRelease(app->readbackBuffer);
        app->readbackBuffer = NULL;
    }
}

AppConfig appDefaultConfig(void) {
    AppConfig config = {};
    config.clearColor = (WGPUColor){0.9, 0.1, 0.2, 1.0};
    config.stagingBufferSize = 16;
    config.readbackWarningFrameThreshold = 240;
    config.encoderLabel = (WGPUStringView){"Main encoder", WGPU_STRLEN};
    config.commandBufferLabel = (WGPUStringView){"Main command buffer", WGPU_STRLEN};
    config.stagingBufferLabel = (WGPUStringView){"Some GPU-side data buffer", WGPU_STRLEN};
    config.readbackBufferLabel = (WGPUStringView){"Output buffer", WGPU_STRLEN};
    return config;
}

bool appInit(AppState* app, const AppConfig* config) {
    if (app == NULL) {
        return false;
    }
    memset(app, 0, sizeof(*app));

    app->config = (config != NULL) ? *config : appDefaultConfig();
    if (app->config.stagingBufferSize == 0 || app->config.stagingBufferSize > (uint64_t)UINT_MAX) {
        fprintf(stderr, "Invalid stagingBufferSize. Expected range: 1..%u\n", UINT_MAX);
        return false;
    }

    app->window = windowInit();
    if (app->window == NULL || !app->window->valid || app->window->window == NULL) {
        fprintf(stderr, "Window initialization failed.\n");
        return false;
    }

    app->gpu = gpuInit();
    if (app->gpu == NULL || app->gpu->device == NULL || app->gpu->queue == NULL || app->gpu->pipeline == NULL) {
        fprintf(stderr, "GPU initialization failed.\n");
        return false;
    }

    if (!appCreateBuffers(app)) {
        appShutdown(app);
        return false;
    }

    return true;
}

void appRun(AppState* app) {
    if (app == NULL || app->window == NULL || app->window->window == NULL) {
        return;
    }

    while (!glfwWindowShouldClose(app->window->window)) {
        appRunFrame(app);
    }
}

bool appRunFrame(AppState* app) {
    if (app == NULL || app->gpu == NULL) {
        return false;
    }

    WGPUSurfaceTexture surfaceTexture = {};
    WGPUTextureView textureView = NULL;
    if (!appBeginFrame(app, &surfaceTexture, &textureView)) {
        appPollDeviceAndEvents(app);
        return false;
    }

    WGPUCommandEncoder encoder = appCreateCommandEncoder(app);

    const bool shouldEncodeReadbackCopy = !app->readbackRequested;
    if (shouldEncodeReadbackCopy) {
        wgpuCommandEncoderCopyBufferToBuffer(
                encoder, app->uploadBuffer, 0, app->readbackBuffer, 0, app->config.stagingBufferSize);
    }

    WGPURenderPassEncoder pass = appBeginRenderPass(app, encoder, textureView);
    appRunLoopSteps(app, encoder, pass);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    WGPUCommandBufferDescriptor cmdDesc = {};
    cmdDesc.nextInChain = NULL;
    cmdDesc.label = app->config.commandBufferLabel;
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdDesc);
    wgpuCommandEncoderRelease(encoder);

    wgpuQueueSubmit(app->gpu->queue, 1, &command);
    wgpuCommandBufferRelease(command);

    if (shouldEncodeReadbackCopy) {
        appRequestReadback(app);
    }
    appReportReadbackTimeoutIfNeeded(app);

#ifndef __EMSCRIPTEN__
    wgpuSurfacePresent(app->gpu->surface);
#endif
    wgpuTextureViewRelease(textureView);

    appPollDeviceAndEvents(app);
    return true;
}

void appShutdown(AppState* app) {
    if (app == NULL) {
        return;
    }

    appReleaseBuffers(app);
    if (app->gpu != NULL) {
        gpuShutdown();
        app->gpu = NULL;
    }
    if (app->window != NULL) {
        windowShutdown();
        app->window = NULL;
    }
}

bool appAddLoopStep(AppState* app, AppLoopStepFn fn, void* userdata) {
    if (app == NULL || fn == NULL) {
        return false;
    }
    if (app->loopStepCount >= APP_MAX_LOOP_STEPS) {
        return false;
    }

    app->loopSteps[app->loopStepCount].fn = fn;
    app->loopSteps[app->loopStepCount].userdata = userdata;
    ++app->loopStepCount;
    return true;
}
