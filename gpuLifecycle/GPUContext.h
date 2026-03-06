#pragma once

#include <webgpu/webgpu.h>

struct GPUContext {
    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUQueue queue;
    WGPUSurface surface;
    bool valid;
};

GPUContext *gpuInit();

GPUContext *gpuGet();

void gpuShutdown();
WGPUCommandEncoder getEncoder();
WGPURenderPassEncoder getRenderPass(WGPUCommandEncoder *encoder, WGPUTextureView targetView, float green);
void getNextSurfaceViewData(WGPUSurfaceTexture *surfaceTexture, WGPUTextureView *textureView);