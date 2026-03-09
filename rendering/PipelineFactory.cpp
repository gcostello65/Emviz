//
// Created by Greg Costello on 3/8/26.
//

#include "PipelineFactory.h"


WGPURenderPipeline getPipeline(WGPUDevice device, GPUContext *gpuContext) {
    WGPURenderPipelineDescriptor pipelineDesc{};
    pipelineDesc.nextInChain = nullptr;

    // Vertex Buffers and shader
    setVertexBuffer(&pipelineDesc);

    WGPUShaderModule shaderModule = getShaderModule(&device);
    setVertexShader(&pipelineDesc, &shaderModule/*shaderModule=*/);
    setPrimitive(&pipelineDesc);

    //Fragment state
    WGPUFragmentState fragmentState{};
    setFragmentState(&fragmentState, &shaderModule);
    setFragmentShader(&pipelineDesc, &fragmentState);

    // Stencil and Depth
    setStencilDepth(&pipelineDesc);

    // Blending
    WGPUBlendState blendState{};
    WGPUColorTargetState colorTarget{};
    setBlendState(&blendState, &colorTarget, &fragmentState, &gpuContext->surfaceFormat);

    //Sampling
    setMultiSample(&pipelineDesc);

    //set memory layout
    pipelineDesc.layout = nullptr;

    WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);
    wgpuShaderModuleRelease(shaderModule);
    return pipeline;
}

void setVertexBuffer(WGPURenderPipelineDescriptor *pipelineDesc) {
    // We do not use any vertex buffer for this first simplistic example
    pipelineDesc->vertex.bufferCount = 0;
    pipelineDesc->vertex.buffers = nullptr;
}

void setVertexShader(WGPURenderPipelineDescriptor *pipelineDesc, WGPUShaderModule *shaderModule) {
    // NB: We define the 'shaderModule' in the second part of this chapter.
// Here we tell that the programmable vertex shader stage is described
// by the function called 'vs_main' in that module.
    pipelineDesc->vertex.module = *shaderModule;
    pipelineDesc->vertex.entryPoint = {"vs_main", WGPU_STRLEN};
    pipelineDesc->vertex.constantCount = 0;
    pipelineDesc->vertex.constants = nullptr;
}

void setPrimitive(WGPURenderPipelineDescriptor *pipelineDesc) {
    // Each sequence of 3 vertices is considered as a triangle
    pipelineDesc->primitive.topology = WGPUPrimitiveTopology_TriangleList;

// We'll see later how to specify the order in which vertices should be
// connected. When not specified, vertices are considered sequentially.
    pipelineDesc->primitive.stripIndexFormat = WGPUIndexFormat_Undefined;

// The face orientation is defined by assuming that when looking
// from the front of the face, its corner vertices are enumerated
// in the counter-clockwise (CCW) order.
    pipelineDesc->primitive.frontFace = WGPUFrontFace_CCW;

// But the face orientation does not matter much because we do not
// cull (i.e. "hide") the faces pointing away from us (which is often
// used for optimization).
//TODO: Set this to front later
    pipelineDesc->primitive.cullMode = WGPUCullMode_None;
}

void setFragmentState(WGPUFragmentState *fragmentState, WGPUShaderModule *shaderModule) {
    fragmentState->module = *shaderModule;
    fragmentState->entryPoint = {"fs_main", WGPU_STRLEN};
    fragmentState->constantCount = 0;
    fragmentState->constants = nullptr;
}

void setFragmentShader(WGPURenderPipelineDescriptor *pipelineDesc, WGPUFragmentState *fragmentState) {
    // We tell that the programmable fragment shader stage is described
// by the function called 'fs_main' in the shader module.
//    {{We'll configure the blending stage here}}
    pipelineDesc->fragment = fragmentState;
}

void setStencilDepth(WGPURenderPipelineDescriptor *pipelineDesc) {
    // We do not use stencil/depth testing for now
    pipelineDesc->depthStencil = nullptr;
}

void setBlendState(WGPUBlendState *blendState, WGPUColorTargetState *colorTarget, WGPUFragmentState *fragmentState, const WGPUTextureFormat *surfaceFormat) {
    // Configure the color blend equation
    blendState->color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blendState->color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blendState->color.operation = WGPUBlendOperation_Add;

    // Configure the alpha blend equation
    blendState->alpha.srcFactor = WGPUBlendFactor_Zero;
    blendState->alpha.dstFactor = WGPUBlendFactor_One;
    blendState->alpha.operation = WGPUBlendOperation_Add;

    colorTarget->format = *surfaceFormat;
    colorTarget->blend = blendState;
    colorTarget->writeMask = WGPUColorWriteMask_All; // We could write to only some of the color channels.

    // We have only one target because our render pass has only one output color
    // attachment.
    fragmentState->targetCount = 1;
    fragmentState->targets = colorTarget;
}

void setMultiSample(WGPURenderPipelineDescriptor *pipelineDesc) {
    // Samples per pixel
    pipelineDesc->multisample.count = 1;
    // Default value for the mask, meaning "all bits on"
    pipelineDesc->multisample.mask = ~0u;
    // Default value as well (irrelevant for count = 1 anyways)
    pipelineDesc->multisample.alphaToCoverageEnabled = false;
}
