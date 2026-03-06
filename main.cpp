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

int main() {
    WindowContext *window = windowInit();
    GPUContext *gpuContext = gpuInit();
    float green = 0.0;

    while (!glfwWindowShouldClose(window->window)) {
        WGPUSurfaceTexture surfaceTexture = {};
        WGPUTextureView textureView = nullptr;

        getNextSurfaceViewData(&surfaceTexture, &textureView);

        if (!textureView) {
            glfwPollEvents();
            continue;
        }

        WGPUCommandEncoder encoder = getEncoder();

        wgpuCommandEncoderInsertDebugMarker(encoder, {"Do one thing", 12});
        wgpuCommandEncoderInsertDebugMarker(encoder, {"Do another thing", 16});

        WGPURenderPassEncoder renderPass = getRenderPass(&encoder, textureView, green);
        if (green < 1.0) green = (float)green + 1.0/1000;
        wgpuRenderPassEncoderEnd(renderPass);
        wgpuRenderPassEncoderRelease(renderPass);

        WGPUCommandBufferDescriptor cmdBufferDescriptor = {};
        cmdBufferDescriptor.nextInChain = nullptr;
        cmdBufferDescriptor.label = {"Command buffer", 14};

        WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDescriptor);
        wgpuCommandEncoderRelease(encoder);

        std::cout << "Submitting command..." << std::endl;
        wgpuQueueSubmit(gpuContext->queue, 1, &command);
        wgpuCommandBufferRelease(command);
        std::cout << "Command submitted." << std::endl;

        wgpuSurfacePresent(gpuContext->surface);

        wgpuTextureViewRelease(textureView);

        glfwPollEvents();
    }

    return 0;
}