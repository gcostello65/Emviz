//
// Created by Greg Costello on 3/4/26.
//

#include "GPUContext.h"
#include "../windowLifecycle/WindowContext.h"
#include "../rendering/PipelineFactory.h"
#include <iostream>
#include <cassert>
#include <glfw3webgpu.h>

static GPUContext gpuContext = {};

void gpuShutdown() {
    // We clean up the WebGPU instance
    wgpuInstanceRelease(gpuContext.instance);
    wgpuAdapterRelease(gpuContext.adapter);
    wgpuDeviceRelease(gpuContext.device);
    wgpuQueueRelease(gpuContext.queue);
    wgpuSurfaceUnconfigure(gpuContext.surface);
    wgpuSurfaceRelease(gpuContext.surface);
    wgpuRenderPipelineRelease(gpuContext.pipeline);
}

GPUContext* gpuGet() {
    return &gpuContext;
}

/**
 * Utility function to get a WebGPU device, so that
 *     WGPUDevice device = requestDeviceSync(adapter, options);
 * is roughly equivalent to
 *     const device = await adapter.requestDevice(descriptor);
 * It is very similar to requestAdapter
 */
WGPUDevice requestDeviceSync(WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor) {
    struct UserData {
        WGPUDevice device = nullptr;
        bool requestEnded = false;
    };
    UserData userData;

    auto onDeviceRequestEnded =
            [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void * userData1, void * userData2) {
                UserData& userData = *reinterpret_cast<UserData*>(userData1);
                if (status == WGPURequestDeviceStatus_Success) {
                    userData.device = device;
                } else {
                    std::cout << "Could not get WebGPU device: " << message.data << std::endl;
                }
                userData.requestEnded = true;
            };

    WGPURequestDeviceCallbackInfo callbackInfo = {};
    callbackInfo.callback = onDeviceRequestEnded;
    callbackInfo.userdata1 = &userData;
    callbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;

    wgpuAdapterRequestDevice(
            adapter,
            descriptor,
            callbackInfo
    );

#ifdef __EMSCRIPTEN__
    while (!userData.requestEnded) {
        emscripten_sleep(100);
    }
#endif // __EMSCRIPTEN__

    assert(userData.requestEnded);

    return userData.device;
}


/**
 * Utility function to get a WebGPU adapter, so that
 *     WGPUAdapter adapter = requestAdapterSync(options);
 * is roughly equivalent to
 *     const adapter = await navigator.gpu.requestAdapter(options);
 */
WGPUAdapter requestAdapterSync(WGPUInstance instance, WGPURequestAdapterOptions const *options) {
    // A simple structure holding the local information shared with the
    // onAdapterRequestEnded callback.
    struct UserData {
        WGPUAdapter adapter = nullptr;
        bool requestEnded = false;
    };
    UserData userData;

    // Callback called by wgpuInstanceRequestAdapter when the request returns
    // This is a C++ lambda function, but could be any function defined in the
    // global scope. It must be non-capturing (the brackets [] are empty) so
    // that it behaves like a regular C function pointer, which is what
    // wgpuInstanceRequestAdapter expects (WebGPU being a C API). The workaround
    // is to convey what we want to capture through the pUserData pointer,
    // provided as the last argument of wgpuInstanceRequestAdapter and received
    // by the callback as its last argument.
    auto onAdapterRequestEnded =
            [](WGPURequestAdapterStatus status,
               WGPUAdapter adapter,
               WGPUStringView message,
               void *userdata1,
               void *userdata2) {
                UserData &userData =
                        *reinterpret_cast<UserData *>(userdata1);

                if (status == WGPURequestAdapterStatus_Success) {
                    userData.adapter = adapter;
                } else {
                    std::cout << "Could not get WebGPU adapter: "
                              << std::string(message.data, message.length)
                              << std::endl;
                }

                userData.requestEnded = true;
            };

    WGPURequestAdapterCallbackInfo callbackInfo = {};
    callbackInfo.callback = onAdapterRequestEnded;
    callbackInfo.userdata1 = &userData;
    callbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;

    // Call to the WebGPU request adapter procedure
    wgpuInstanceRequestAdapter(
            instance /* equivalent of navigator.gpu */,
            options,
            callbackInfo
    );

    // We wait until userData.requestEnded gets true
    //{{Wait for request to end}}

    assert(userData.requestEnded);

    return userData.adapter;
}

void showLimits(WGPUAdapter adapter) {
#ifndef __EMSCRIPTEN__
    WGPULimits supportedLimits = {};
    supportedLimits.nextInChain = nullptr;

#ifdef WEBGPU_BACKEND_DAWN
    bool success = wgpuAdapterGetLimits(adapter, &supportedLimits) == WGPUStatus_Success;
#else
    bool success = wgpuAdapterGetLimits(adapter, &supportedLimits);
#endif

    if (success) {
        std::cout << "Adapter limits:" << std::endl;
        std::cout << " - maxTextureDimension1D: " << supportedLimits.maxTextureDimension1D << std::endl;
        std::cout << " - maxTextureDimension2D: " << supportedLimits.maxTextureDimension2D << std::endl;
        std::cout << " - maxTextureDimension3D: " << supportedLimits.maxTextureDimension3D << std::endl;
        std::cout << " - maxTextureArrayLayers: " << supportedLimits.maxTextureArrayLayers << std::endl;
        std::cout << " - maxComputeWorkgroupSizeX: " << supportedLimits.maxComputeWorkgroupSizeX << std::endl;
        std::cout << " - maxComputeInvocationsPerWorkgroup: " << supportedLimits.maxComputeInvocationsPerWorkgroup << std::endl;
        std::cout << " - maxComputeWorkgroupsPerDimension: " << supportedLimits.maxComputeWorkgroupsPerDimension << std::endl;
        std::cout << " - maxStorageBufferBindingSize: " << supportedLimits.maxStorageBufferBindingSize << std::endl;

    }
#endif // NOT __EMSCRIPTEN__

    WGPUSupportedFeatures supported = {0};

// Call the function a first time with a null return address, just to get
// the entry count.
    wgpuAdapterGetFeatures(adapter, &supported);

    supported.features = (WGPUFeatureName*)malloc(supported.featureCount * sizeof(WGPUFeatureName));

    wgpuAdapterGetFeatures(adapter, &supported);

    std::cout << "Adapter features:" << std::endl;
    std::cout << std::hex; // Write integers as hexadecimal to ease comparison with webgpu.h literals
    for (int i = 0; i < supported.featureCount; i++) {
        std::cout << " - 0x" << supported.features[i] << std::endl;
    }
    std::cout << std::dec; // Restore decimal numbers

    WGPUAdapterInfo info = {0};
    wgpuAdapterGetInfo(adapter, &info);

    printf("Vendor: %.*s\n", (int)info.vendor.length, info.vendor.data);
    printf("Device: %.*s\n", (int)info.device.length, info.device.data);
    printf("Arch: %.*s\n", (int)info.architecture.length, info.architecture.data);
    printf("Description: %.*s\n", (int)info.description.length, info.description.data);
}

// We also add an inspect device function:
void inspectDevice(WGPUDevice device) {
    WGPUSupportedFeatures features = {};
    features.features = nullptr;

    wgpuDeviceGetFeatures(device, &features);

    if (features.featureCount == 0) {
        std::cout << "Device features: (none or query returned 0)" << std::endl;
        return;
    }

    features.features = (WGPUFeatureName*)malloc(
            features.featureCount * sizeof(WGPUFeatureName)
    );

    if (!features.features) {
        std::cerr << "malloc failed for device features" << std::endl;
        return;
    }

    wgpuDeviceGetFeatures(device, &features);

    std::cout << "Device features:" << std::endl;
    std::cout << std::hex;
    for (uint32_t i = 0; i < features.featureCount; i++) {
        std::cout << " - 0x" << features.features[i] << std::endl;
    }
    std::cout << std::dec;

    WGPULimits supportedLimits = {0};
    supportedLimits.nextInChain = nullptr;

#ifdef WEBGPU_BACKEND_DAWN
    bool success = wgpuDeviceGetLimits(device, &supportedLimits) == WGPUStatus_Success;
#else
    bool success = wgpuDeviceGetLimits(device, &limits);
#endif

    if (success) {
        std::cout << "Device limits:" << std::endl;
        std::cout << " - maxTextureDimension1D: " << supportedLimits.maxTextureDimension1D << std::endl;
        std::cout << " - maxTextureDimension2D: " << supportedLimits.maxTextureDimension2D << std::endl;
        std::cout << " - maxTextureDimension3D: " << supportedLimits.maxTextureDimension3D << std::endl;
        std::cout << " - maxTextureArrayLayers: " << supportedLimits.maxTextureArrayLayers << std::endl;
        std::cout << " - maxComputeWorkgroupSizeX: " << supportedLimits.maxComputeWorkgroupSizeX << std::endl;
        std::cout << " - maxComputeInvocationsPerWorkgroup: " << supportedLimits.maxComputeInvocationsPerWorkgroup << std::endl;
        std::cout << " - maxComputeWorkgroupsPerDimension: " << supportedLimits.maxComputeWorkgroupsPerDimension << std::endl;
        std::cout << " - maxStorageBufferBindingSize: " << supportedLimits.maxStorageBufferBindingSize << std::endl;
    }
}

WGPUDevice requestDevice(WGPUAdapter adapter) {
    std::cout << "Requesting device..." << std::endl;

    WGPUDeviceDescriptor deviceDesc = {};
    deviceDesc.nextInChain = nullptr;
    deviceDesc.label = {"My Device", 9} ; // anything works here, that's your call
    deviceDesc.requiredFeatureCount = 0; // we do not require any specific feature
    deviceDesc.requiredLimits = nullptr; // we do not require any specific limit
    deviceDesc.defaultQueue.nextInChain = nullptr;
    deviceDesc.defaultQueue.label = {"The default queue", 17};

    WGPUDeviceLostCallbackInfo lostCallbackInfo = {};
    lostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    lostCallbackInfo.callback =
            [](WGPUDevice const *device, WGPUDeviceLostReason reason,
               WGPUStringView message,
               void* userdata1,
               void* userdata2)
            {
                std::cout << "Device lost: reason " << reason;
                if (message.data) {
                    std::cout << " (" << std::string(message.data, message.length) << ")";
                }
                std::cout << std::endl;
            };
    lostCallbackInfo.userdata1 = nullptr;
    lostCallbackInfo.userdata2 = nullptr;
    // A function that is invoked whenever the device stops being available.
    deviceDesc.deviceLostCallbackInfo = lostCallbackInfo;

    WGPUUncapturedErrorCallbackInfo uncapturedErrorCallbackInfo = {};
    uncapturedErrorCallbackInfo.nextInChain = nullptr;
    uncapturedErrorCallbackInfo.callback = [](WGPUDevice const *device, WGPUErrorType type, WGPUStringView message, void* /* pUserData */, void*) {
        std::cout << "Uncaptured device error: type " << type;
        if (message.data) std::cout << " (" << message.data << ")";
        std::cout << std::endl;
    };

    deviceDesc.uncapturedErrorCallbackInfo = uncapturedErrorCallbackInfo;


    WGPUDevice device = requestDeviceSync(adapter, &deviceDesc);

    std::cout << "Got device: " << device << std::endl;

    return device;
}

WGPURenderPassEncoder getRenderPass(WGPUCommandEncoder *encoder, WGPUTextureView targetView, float green) {
    WGPURenderPassDescriptor renderPassDesc = {};
    renderPassDesc.nextInChain = nullptr;

// [...] Describe Render Pass
    WGPURenderPassColorAttachment renderPassColorAttachment = {};

    // [...] Describe the attachment
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;
    renderPassColorAttachment.view = targetView;

    //TODO: Fix this later when we go into mipmapping
    renderPassColorAttachment.resolveTarget = nullptr;
    renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
    renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
    renderPassColorAttachment.clearValue = WGPUColor{ 1.0, green, 0.4, 1.0 };
    renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    renderPassDesc.depthStencilAttachment = nullptr;
    renderPassDesc.timestampWrites = nullptr;

    return wgpuCommandEncoderBeginRenderPass(*encoder, &renderPassDesc);
}

WGPUCommandEncoder getEncoder() {
    WGPUCommandEncoderDescriptor encoderDesc = {};
    encoderDesc.nextInChain = nullptr;
    encoderDesc.label = {"My command encoder"};
    return wgpuDeviceCreateCommandEncoder(gpuContext.device, &encoderDesc);
}

void initQueue() {
    gpuContext.queue = wgpuDeviceGetQueue(gpuContext.device);

    WGPUQueueWorkDoneCallbackInfo callbackInfo = {};

    callbackInfo.nextInChain = nullptr;
    callbackInfo.mode = WGPUCallbackMode_WaitAnyOnly;
    callbackInfo.callback = [](WGPUQueueWorkDoneStatus status, WGPUStringView message, void* /* pUserData */, void *) {
        std::cout << "Queued work finished with status: " << status << std::endl;
    };

    wgpuQueueOnSubmittedWorkDone(gpuContext.queue, callbackInfo);
}

void configureSurface() {
    WGPUSurfaceConfiguration config = {};
    config.nextInChain = nullptr;
    config.device = gpuContext.device;
    config.width = 640;
    config.height = 480;
    config.usage = WGPUTextureUsage_RenderAttachment;
    config.presentMode = WGPUPresentMode_Fifo;
    config.alphaMode = WGPUCompositeAlphaMode_Auto;

    WGPUSurfaceCapabilities caps = {};
    wgpuSurfaceGetCapabilities(gpuContext.surface, gpuContext.adapter, &caps);

    config.format = caps.formats[0];
    config.viewFormatCount = 0;
    config.viewFormats = nullptr;

    //TODO: Make sure this is more configurable. This is where I am setting the format since it used elsewhere
    gpuContext.surfaceFormat = caps.formats[0];

    wgpuSurfaceConfigure(gpuContext.surface, &config);
}

void getNextSurfaceViewData(WGPUSurfaceTexture* surfaceTexture, WGPUTextureView* textureView) {
    *textureView = nullptr;

    wgpuSurfaceGetCurrentTexture(gpuContext.surface, surfaceTexture);
    if (surfaceTexture->status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        surfaceTexture->status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        return;
    }

    WGPUTextureViewDescriptor viewDescriptor = {};
    viewDescriptor.nextInChain = nullptr;
    viewDescriptor.label = {"Surface texture view", 20};
    viewDescriptor.format = wgpuTextureGetFormat(surfaceTexture->texture);
    viewDescriptor.dimension = WGPUTextureViewDimension_2D;
    viewDescriptor.baseMipLevel = 0;
    viewDescriptor.mipLevelCount = 1;
    viewDescriptor.baseArrayLayer = 0;
    viewDescriptor.arrayLayerCount = 1;
    viewDescriptor.aspect = WGPUTextureAspect_All;

    *textureView = wgpuTextureCreateView(surfaceTexture->texture, &viewDescriptor);

    wgpuTextureRelease(surfaceTexture->texture);
}

void deviceLoggingCallback(
        WGPULoggingType type,
        WGPUStringView message,
        void* /*userdata1*/,
        void* /*userdata2*/) {
    printf("WebGPU log (%d): %.*s\n", type, (int)message.length, message.data ? message.data : "");
}

GPUContext* gpuInit() {
    WGPUInstanceDescriptor desc = {};
    desc.nextInChain = nullptr;

#ifdef WEBGPU_BACKEND_DAWN
    WGPUDawnTogglesDescriptor toggles = {};
    toggles.chain.next = nullptr;
    toggles.chain.sType = WGPUSType_DawnTogglesDescriptor;
    toggles.disabledToggleCount = 0;
    toggles.enabledToggleCount = 1;
    const char* toggleName = "enable_immediate_error_handling";
    toggles.enabledToggles = &toggleName;
    desc.nextInChain = &toggles.chain;
#endif

    gpuContext.instance = wgpuCreateInstance(&desc);

    if (!gpuContext.instance) {
        std::cerr << "Could not initialize WebGPU!" << std::endl;
        gpuContext.valid = false;
        return &gpuContext;
    }

    std::cout << "WGPU instance: " << gpuContext.instance << std::endl;

    gpuContext.surface = glfwGetWGPUSurface(gpuContext.instance, windowGet()->window);

    std::cout << "Requesting adapter..." << std::endl;
    WGPURequestAdapterOptions adapterOpts = {};
    adapterOpts.nextInChain = nullptr;
    adapterOpts.compatibleSurface = gpuContext.surface;
    gpuContext.adapter = requestAdapterSync(gpuContext.instance, &adapterOpts);

    std::cout << "Got adapter: " << gpuContext.adapter << std::endl;
    showLimits(gpuContext.adapter);

    gpuContext.device = requestDevice(gpuContext.adapter);
    WGPULoggingCallbackInfo loggingCallbackInfo = {};
    loggingCallbackInfo.nextInChain = nullptr;
    loggingCallbackInfo.callback = deviceLoggingCallback;
    loggingCallbackInfo.userdata1 = nullptr;
    loggingCallbackInfo.userdata2 = nullptr;
    wgpuDeviceSetLoggingCallback(gpuContext.device, loggingCallbackInfo);
    inspectDevice(gpuContext.device);

    configureSurface();
    initQueue();

    gpuContext.pipeline = getPipeline(gpuContext.device, &gpuContext);

    return &gpuContext;
}
