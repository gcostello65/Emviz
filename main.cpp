#include <iostream>
#include <webgpu/webgpu.h>

#include <vector>
#include "gpuLifecycle/GPUContext.h"
#include "windowLifecycle/WindowContext.h"

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif // __EMSCRIPTEN__

//// If using wgpu-native
//#define WEBGPU_BACKEND_WGPU
//
//// If using emscripten
//#define WEBGPU_BACKEND_EMSCRIPTEN

void mainLoop(GPUContext *gpuContext) {

    WGPUSurfaceTexture surfaceTexture = {};
    WGPUTextureView textureView = nullptr;

    getNextSurfaceViewData(&surfaceTexture, &textureView);

    if (!textureView) {
        glfwPollEvents();
        return;
    }

    // Create a command encoder for the draw call
    WGPUCommandEncoderDescriptor encoderDesc = {};
    encoderDesc.nextInChain = nullptr;
    encoderDesc.label = {"My command encoder"};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(gpuContext->device, &encoderDesc);

    // Create the render pass that clears the screen with our color
    WGPURenderPassDescriptor renderPassDesc = {};
    renderPassDesc.nextInChain = nullptr;

    // The attachment part of the render pass descriptor describes the target texture of the pass
    WGPURenderPassColorAttachment renderPassColorAttachment = {};
    renderPassColorAttachment.view = textureView;
    renderPassColorAttachment.resolveTarget = nullptr;
    renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
    renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
    renderPassColorAttachment.clearValue = WGPUColor{ 0.9, 0.1, 0.2, 1.0 };
#ifndef WEBGPU_BACKEND_WGPU
    renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // NOT WEBGPU_BACKEND_WGPU

    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;
    renderPassDesc.depthStencilAttachment = nullptr;
    renderPassDesc.timestampWrites = nullptr;

    WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);

    // Select which render pipeline to use
    wgpuRenderPassEncoderSetPipeline(renderPass, gpuContext->pipeline);
    // Draw 1 instance of a 3-vertices shape
    wgpuRenderPassEncoderDraw(renderPass, 3, 1, 0, 0);

    wgpuRenderPassEncoderEnd(renderPass);
    wgpuRenderPassEncoderRelease(renderPass);

    // Encode and submit the render pass
    WGPUCommandBufferDescriptor cmdBufferDescriptor = {};
    cmdBufferDescriptor.nextInChain = nullptr;
    cmdBufferDescriptor.label = {"Command buffer"};
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDescriptor);
    wgpuCommandEncoderRelease(encoder);

    std::cout << "Submitting command..." << std::endl;
    wgpuQueueSubmit(gpuContext->queue, 1, &command);
    wgpuCommandBufferRelease(command);
    std::cout << "Command submitted." << std::endl;

    // At the enc of the frame
    wgpuTextureViewRelease(textureView);
#ifndef __EMSCRIPTEN__
    wgpuSurfacePresent(gpuContext->surface);
#endif

    glfwPollEvents();

#if defined(WEBGPU_BACKEND_DAWN)
    wgpuDeviceTick(gpuContext->device);
#elif defined(WEBGPU_BACKEND_WGPU)
    wgpuDevicePoll(device, false, nullptr);
#endif
}


int main() {
    WindowContext *window = windowInit();
    GPUContext *gpuContext = gpuInit();

    while (!glfwWindowShouldClose(window->window)) {
        mainLoop(gpuContext);
    }

    return 0;
}
