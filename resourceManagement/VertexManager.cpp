//
// Created by Greg Costello on 3/11/26.
//

#include "VertexManager.h"
#include "BufferManager.h"
#include "../gpuLifecycle/GPUContext.h"

void setAndReturnVertexBuffer(WGPURenderPipelineDescriptor *pipelineDesc,
                              const Vertex *vertexData,
                              uint64_t vertexCount,
                              GPUContext *gpuContext,
                              VertexContainer *vertexContainer) {
    WGPUBufferDescriptor vertexBufferDescriptor{};
    populateBufferDesc(
            &vertexBufferDescriptor,
            {"Vertex Buffer", WGPU_STRLEN},
            WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
            sizeof(Vertex) * vertexCount);

    WGPUBuffer buffer = createBuffer(&gpuContext->device, &vertexBufferDescriptor);
    vertexContainer->vertexBuffer = buffer;
    vertexContainer->vertexData = nullptr;
    vertexContainer->vertexCount = vertexCount;
    vertexContainer->vertexBufferDesc = vertexBufferDescriptor;

    pipelineDesc->vertex.bufferCount = 1;

    WGPUVertexBufferLayout vertexBufferLayout{};

    uint32_t attributeCount = 2;

    // == For each attribute, describe its layout, i.e., how to interpret the raw data ==
    // Corresponds to @location(...)
    vertexContainer->vertexAttributes[0].shaderLocation = 0;
    // Means vec2f in the shader
    vertexContainer->vertexAttributes[0].format = WGPUVertexFormat_Float32x2;
    // Index of the first element
    vertexContainer->vertexAttributes[0].offset = 0;

    // == For each attribute, describe its layout, i.e., how to interpret the raw data ==
    // Corresponds to @location(...)
    vertexContainer->vertexAttributes[1].shaderLocation = 1;
    // Means vec2f in the shader
    vertexContainer->vertexAttributes[1].format = WGPUVertexFormat_Float32x3;
    // Index of the first element
    vertexContainer->vertexAttributes[1].offset = 2 * sizeof(float);

    // == Common to attributes from the same buffer ==
    vertexBufferLayout.arrayStride = sizeof(Vertex);
    vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;

    vertexBufferLayout.attributeCount = attributeCount;
    vertexBufferLayout.attributes = vertexContainer->vertexAttributes;

    vertexContainer->vertexBufferLayout = vertexBufferLayout;

    pipelineDesc->vertex.buffers = &vertexContainer->vertexBufferLayout;

    wgpuQueueWriteBuffer(
            gpuContext->queue,
            vertexContainer->vertexBuffer,
            0,
            vertexData,
            sizeof(Vertex) * vertexCount);
}
