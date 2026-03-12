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
    auto* app = (AppState*)userdata1;
    (void)userdata2;
    app->readbackCompleted = true;

    if (status != WGPUMapAsyncStatus_Success) {
        fprintf(stderr, "Readback map failed. Status: %d\n", (int)status);
        if (message.data != NULL) {
            fprintf(stderr, "Reason: %.*s\n", (int)message.length, message.data);
        }
        return;
    }

    if (app->config.debugReadback.printMappedBytes) {
        const uint8_t* bufferData = (const uint8_t*)wgpuBufferGetConstMappedRange(
                app->debugReadbackBuffer, 0, app->config.debugReadback.bufferSize);
        printf("bufferData = [");
        for (uint64_t i = 0; i < app->config.debugReadback.bufferSize; ++i) {
            if (i > 0) {
                printf(", ");
            }
            printf("%d", (int)bufferData[i]);
        }
        printf("]\n");
    }

    wgpuBufferUnmap(app->debugReadbackBuffer);
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
            app->debugReadbackBuffer, WGPUMapMode_Read, 0, app->config.debugReadback.bufferSize, mapCallbackInfo);
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

static void appRunStepList(
        AppLoopStepEntry* list,
        uint32_t count,
        AppLoopContext* ctx) {
    for (uint32_t i = 0; i < count; ++i) {
        AppLoopStepEntry* entry = &list[i];
        if (entry->fn != NULL) {
            entry->fn(ctx, entry->userdata);
        }
    }
}

static void appRunRenderSteps(AppState* app, WGPUCommandEncoder encoder, WGPURenderPassEncoder pass) {
    if (app->renderStepCount == 0) {
        wgpuRenderPassEncoderSetPipeline(pass, app->gpu->pipeline);
        wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
        return;
    }

    AppLoopContext ctx = {};
    ctx.gpu = app->gpu;
    ctx.encoder = encoder;
    ctx.renderPass = pass;
    appRunStepList(app->renderSteps, app->renderStepCount, &ctx);
}

static void appRunPreRenderSteps(AppState* app, WGPUCommandEncoder encoder) {
    if (app->preRenderStepCount == 0) {
        return;
    }

    AppLoopContext ctx = {};
    ctx.gpu = app->gpu;
    ctx.encoder = encoder;
    ctx.renderPass = NULL;
    appRunStepList(app->preRenderSteps, app->preRenderStepCount, &ctx);
}

static void appRunPostRenderSteps(AppState* app, WGPUCommandEncoder encoder) {
    if (app->postRenderStepCount == 0) {
        return;
    }

    AppLoopContext ctx = {};
    ctx.gpu = app->gpu;
    ctx.encoder = encoder;
    ctx.renderPass = NULL;
    appRunStepList(app->postRenderSteps, app->postRenderStepCount, &ctx);
}

static void appRequestReadback(AppState* app) {
    if (!app->config.debugReadback.enabled || app->readbackRequested || app->debugReadbackBuffer == NULL) {
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
    if (!app->config.debugReadback.enabled || !app->readbackRequested || app->readbackCompleted) {
        return;
    }

    ++app->framesSinceReadbackRequest;
    if (app->config.debugReadback.warningFrameThreshold == 0) {
        return;
    }

    if (!app->readbackTimeoutLogged &&
        app->framesSinceReadbackRequest >= app->config.debugReadback.warningFrameThreshold) {
        fprintf(
                stderr,
                "Readback still pending after %llu frames. Continuing without blocking.\n",
                (unsigned long long)app->framesSinceReadbackRequest);
        app->readbackTimeoutLogged = true;
    }
}

static bool appCreateBuffers(AppState* app) {
    if (!app->config.debugReadback.enabled) {
        return true;
    }

    WGPUBufferDescriptor bufferDesc = {};
    populateBufferDesc(
            &bufferDesc,
            app->config.debugReadback.uploadBufferLabel,
            WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc,
            (uint32_t)app->config.debugReadback.bufferSize);
    app->debugUploadBuffer = createBuffer(&app->gpu->device, &bufferDesc);
    if (app->debugUploadBuffer == NULL) {
        fprintf(stderr, "Failed to create upload buffer.\n");
        return false;
    }

    bufferDesc.label = app->config.debugReadback.readbackBufferLabel;
    bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    app->debugReadbackBuffer = createBuffer(&app->gpu->device, &bufferDesc);
    if (app->debugReadbackBuffer == NULL) {
        fprintf(stderr, "Failed to create readback buffer.\n");
        return false;
    }

    uint8_t* numbers = (uint8_t*)malloc((size_t)app->config.debugReadback.bufferSize);
    if (numbers == NULL) {
        fprintf(stderr, "Failed to allocate staging data.\n");
        return false;
    }

    for (uint64_t i = 0; i < app->config.debugReadback.bufferSize; ++i) {
        numbers[i] = (uint8_t)(i & 0xFFu);
    }
    wgpuQueueWriteBuffer(
            app->gpu->queue, app->debugUploadBuffer, 0, numbers, (size_t)app->config.debugReadback.bufferSize);
    free(numbers);

    return true;
}

static void appReleaseBuffers(AppState* app) {
    if (app->debugUploadBuffer != NULL) {
        wgpuBufferRelease(app->debugUploadBuffer);
        app->debugUploadBuffer = NULL;
    }
    if (app->debugReadbackBuffer != NULL) {
        wgpuBufferRelease(app->debugReadbackBuffer);
        app->debugReadbackBuffer = NULL;
    }
}

AppConfig appDefaultConfig(void) {
    AppConfig config = {};
    config.clearColor = (WGPUColor){0.0, 0.0, 0.0, 1.0};
    config.encoderLabel = (WGPUStringView){"Main encoder", WGPU_STRLEN};
    config.commandBufferLabel = (WGPUStringView){"Main command buffer", WGPU_STRLEN};
    config.debugReadback.enabled = false;
    config.debugReadback.printMappedBytes = true;
    config.debugReadback.bufferSize = 16;
    config.debugReadback.warningFrameThreshold = 240;
    config.debugReadback.uploadBufferLabel = (WGPUStringView){"Debug upload buffer", WGPU_STRLEN};
    config.debugReadback.readbackBufferLabel = (WGPUStringView){"Debug readback buffer", WGPU_STRLEN};
    return config;
}

bool appInit(AppState* app, const AppConfig* config) {
    if (app == NULL) {
        return false;
    }
    memset(app, 0, sizeof(*app));

    app->config = (config != NULL) ? *config : appDefaultConfig();
    if (app->config.debugReadback.enabled &&
        (app->config.debugReadback.bufferSize == 0 || app->config.debugReadback.bufferSize > (uint64_t)UINT_MAX)) {
        fprintf(stderr, "Invalid debugReadback.bufferSize. Expected range: 1..%u\n", UINT_MAX);
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

    // This is just the tutorial, it is just a debug if we want to toggle. Will probably have codex remove this
    const bool shouldEncodeReadbackCopy = app->config.debugReadback.enabled && !app->readbackRequested;
    if (shouldEncodeReadbackCopy) {
        wgpuCommandEncoderCopyBufferToBuffer(
                encoder,
                app->debugUploadBuffer,
                0,
                app->debugReadbackBuffer,
                0,
                app->config.debugReadback.bufferSize);
    }

    appRunPreRenderSteps(app, encoder);

    WGPURenderPassEncoder pass = appBeginRenderPass(app, encoder, textureView);
    appRunRenderSteps(app, encoder, pass);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    appRunPostRenderSteps(app, encoder);

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

static bool appAddStep(
        AppLoopStepEntry* list,
        uint32_t* count,
        AppLoopStepFn fn,
        void* userdata) {
    if (*count >= APP_MAX_LOOP_STEPS) {
        return false;
    }
    list[*count].fn = fn;
    list[*count].userdata = userdata;
    ++(*count);
    return true;
}

bool appAddPreRenderStep(AppState* app, AppLoopStepFn fn, void* userdata) {
    if (app == NULL || fn == NULL) {
        return false;
    }
    return appAddStep(app->preRenderSteps, &app->preRenderStepCount, fn, userdata);
}

bool appAddRenderStep(AppState* app, AppLoopStepFn fn, void* userdata) {
    if (app == NULL || fn == NULL) {
        return false;
    }
    return appAddStep(app->renderSteps, &app->renderStepCount, fn, userdata);
}

bool appAddPostRenderStep(AppState* app, AppLoopStepFn fn, void* userdata) {
    if (app == NULL || fn == NULL) {
        return false;
    }
    return appAddStep(app->postRenderSteps, &app->postRenderStepCount, fn, userdata);
}

// If you do not use webgpu.hpp, I suggest you create a function to init the
// WGPULimits structure:
void setDefault(WGPULimits &limits) {
    limits.maxTextureDimension1D = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxTextureDimension2D = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxTextureDimension3D = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxTextureArrayLayers = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxBindGroups = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxBindGroupsPlusVertexBuffers = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxBindingsPerBindGroup = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxDynamicUniformBuffersPerPipelineLayout = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxDynamicStorageBuffersPerPipelineLayout = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxSampledTexturesPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxSamplersPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxStorageBuffersPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxStorageTexturesPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxUniformBuffersPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxUniformBufferBindingSize = WGPU_LIMIT_U64_UNDEFINED;
    limits.maxStorageBufferBindingSize = WGPU_LIMIT_U64_UNDEFINED;
    limits.minUniformBufferOffsetAlignment = WGPU_LIMIT_U32_UNDEFINED;
    limits.minStorageBufferOffsetAlignment = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxVertexBuffers = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxBufferSize = WGPU_LIMIT_U64_UNDEFINED;
    limits.maxVertexAttributes = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxVertexBufferArrayStride = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxInterStageShaderVariables = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxColorAttachments = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxColorAttachmentBytesPerSample = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxComputeWorkgroupStorageSize = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxComputeInvocationsPerWorkgroup = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxComputeWorkgroupSizeX = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxComputeWorkgroupSizeY = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxComputeWorkgroupSizeZ = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxComputeWorkgroupsPerDimension = WGPU_LIMIT_U32_UNDEFINED;
    limits.maxImmediateSize = WGPU_LIMIT_U32_UNDEFINED;
}
