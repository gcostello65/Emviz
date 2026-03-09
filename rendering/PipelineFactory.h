//
// Created by Greg Costello on 3/8/26.
//

#ifndef EM_SIM_PIPELINEFACTORY_H
#define EM_SIM_PIPELINEFACTORY_H
#include "dawn/webgpu.h"
#include "ShaderManager.h"
#include "../gpuLifecycle/GPUContext.h"

WGPURenderPipeline getPipeline(WGPUDevice device, GPUContext *gpuContext);
static void setVertexBuffer(WGPURenderPipelineDescriptor *pipelineDesc);
static void setVertexShader(WGPURenderPipelineDescriptor *pipelineDesc, WGPUShaderModule *shaderModule);
static void setPrimitive(WGPURenderPipelineDescriptor *pipelineDesc);
static void setFragmentShader(WGPURenderPipelineDescriptor *pipelineDesc, WGPUFragmentState *fragmentState);
static void setFragmentState(WGPUFragmentState *fragmentState, WGPUShaderModule *shaderModule);
static void setStencilDepth(WGPURenderPipelineDescriptor *pipelineDesc);
static void setBlendState(WGPUBlendState *blendState, WGPUColorTargetState *colorTarget, WGPUFragmentState *fragmentState, const WGPUTextureFormat *surfaceFormat);
static void setMultiSample(WGPURenderPipelineDescriptor *pipelineDesc);

#endif //EM_SIM_PIPELINEFACTORY_H
