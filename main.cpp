#include <iostream>
#include <webgpu/webgpu.h>

#include <vector>
#include "gpuLifecycle/GPUContext.h"
#include "windowLifecycle/WindowContext.h"
#include "resourceManagement/BufferManager.h"

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif // __EMSCRIPTEN__

//// If using wgpu-native
//#define WEBGPU_BACKEND_WGPU
//
//// If using emscripten
//#define WEBGPU_BACKEND_EMSCRIPTEN

// We define a function that hides implementation-specific variants of device polling:
void wgpuPollEvents([[maybe_unused]] WGPUDevice device, [[maybe_unused]] bool yieldToWebBrowser) {
#if defined(WEBGPU_BACKEND_DAWN)
    wgpuDeviceTick(device);
#elif defined(WEBGPU_BACKEND_WGPU)
    wgpuDevicePoll(device, false, nullptr);
#elif defined(WEBGPU_BACKEND_EMSCRIPTEN)
    if (yieldToWebBrowser) {
        emscripten_sleep(100);
    }
#endif
}

void mainLoop(GPUContext *gpuContext) {


    // Playing with buffers in the tutorial
    WGPUBufferDescriptor bufferDesc = {};
    populateBufferDesc(&bufferDesc, {"Some GPU-side data buffer"},
                       WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc, 16);

    WGPUBuffer buffer1 = createBuffer(&gpuContext->device, &bufferDesc);

    bufferDesc.label = {"Output buffer"};
    bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;

    WGPUBuffer buffer2 = createBuffer(&gpuContext->device, &bufferDesc);

// Create some CPU-side data buffer (of size 16 bytes)
    std::vector<uint8_t> numbers(16);
    for (uint8_t i = 0; i < 16; ++i) numbers[i] = i;
// `numbers` now contains [ 0, 1, 2, ... ]

// Copy this from `numbers` (RAM) to `buffer1` (VRAM)
    wgpuQueueWriteBuffer(gpuContext->queue, buffer1, 0, numbers.data(), numbers.size());


    // The context shared between this main function and the callback.
    struct Context {
        bool ready;
        WGPUBuffer buffer;
    };

    auto onBuffer2Mapped = [](WGPUMapAsyncStatus status, WGPUStringView message, void* pUserData, void* /* pUserData */) {
        auto* context = reinterpret_cast<Context*>(pUserData);
        context->ready = true;
        std::cout << "Buffer 2 mapped with status " << status << std::endl;
        if (status != WGPUMapAsyncStatus_Success) return;

        // Get a pointer to wherever the driver mapped the GPU memory to the RAM
        auto* bufferData = (uint8_t*)wgpuBufferGetConstMappedRange(context->buffer, 0, 16);

        std::cout << "bufferData = [";
        for (int i = 0; i < 16; ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << (int)bufferData[i];
        }
        std::cout << "]" << std::endl;

        // Then do not forget to unmap the memory
        wgpuBufferUnmap(context->buffer);
    };

    // Create the Context instance
    Context context = { false, buffer2 };

    WGPUBufferMapCallbackInfo callbackInfo {};
    callbackInfo.callback = onBuffer2Mapped;
    callbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    callbackInfo.nextInChain = nullptr;
    callbackInfo.userdata1 = (void*)&context;

    wgpuBufferMapAsync(buffer2, WGPUMapMode_Read, 0, 16, callbackInfo);

    // TODO: Make sure to clean up all these objects above and below. This should be easy to do with codex. Make sure it is an extensible too.


    //{{Define callback and start mapping buffer}}

    while (!context.ready) {
        wgpuPollEvents(gpuContext->device, true /* yieldToBrowser */);
    }

    // Surface operations

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

    // After creating the command encoder
    wgpuCommandEncoderCopyBufferToBuffer(encoder, buffer1, 0, buffer2, 0, 16);

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

    // In Terminate()
    wgpuBufferRelease(buffer1);
    wgpuBufferRelease(buffer2);

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
