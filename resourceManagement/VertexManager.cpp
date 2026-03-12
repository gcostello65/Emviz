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
    const uint32_t attributeCount = 2;
    const WGPUVertexAttribute attributes[] = {
            ATTRIB_T(Vertex, 0, WGPUVertexFormat_Float32x2, x),
            ATTRIB_T(Vertex, 1, WGPUVertexFormat_Float32x3, r),
    };
    for (uint32_t i = 0; i < attributeCount; ++i) {
        vertexContainer->vertexAttributes[i] = attributes[i];
    }

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
