//
// Created by Greg Costello on 3/8/26.
//

#ifndef EM_SIM_SHADERMANAGER_H
#define EM_SIM_SHADERMANAGER_H

#include <dawn/webgpu.h>

WGPUShaderModule getShaderModule(WGPUDevice *device);

#endif //EM_SIM_SHADERMANAGER_H
