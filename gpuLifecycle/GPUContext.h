#pragma once

#include <webgpu/webgpu.h>
#include "../resourceManagement/VertexManager.h"

struct GPUContext {
    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUQueue queue;
    WGPUSurface surface;
    WGPUTextureFormat surfaceFormat;
    WGPURenderPipeline pipeline;
    VertexContainer vertexContainer;
    bool valid;
};

GPUContext *gpuInit();

GPUContext *gpuGet();

void gpuShutdown();
void getNextSurfaceViewData(WGPUSurfaceTexture *surfaceTexture, WGPUTextureView *textureView);