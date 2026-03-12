//
// Created by Greg Costello on 3/9/26.
//

#ifndef EM_SIM_BUFFERMANAGER_H
#define EM_SIM_BUFFERMANAGER_H

#include <dawn/webgpu.h>

WGPUBuffer createBuffer(WGPUDevice *device, WGPUBufferDescriptor *bufferDesc);
void populateBufferDesc(WGPUBufferDescriptor *bufferDesc, WGPUStringView label, WGPUBufferUsage usage, uint64_t size);



#endif //EM_SIM_BUFFERMANAGER_H
