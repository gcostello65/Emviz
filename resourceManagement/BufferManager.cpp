//
// Created by Greg Costello on 3/9/26.
//

#include "BufferManager.h"

void populateBufferDesc(WGPUBufferDescriptor *bufferDesc, WGPUStringView label, WGPUBufferUsage usage, uint32_t size) {
    bufferDesc->nextInChain = nullptr;
    bufferDesc->label = label;
    bufferDesc->usage = usage;
    bufferDesc->size = size;
    bufferDesc->mappedAtCreation = false;
}

WGPUBuffer createBuffer(WGPUDevice *device, WGPUBufferDescriptor *bufferDesc) {
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(*device, bufferDesc);
    return buffer;
}