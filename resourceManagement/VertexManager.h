//
// Created by Greg Costello on 3/11/26.
//

#ifndef EM_SIM_VERTEXMANAGER_H
#define EM_SIM_VERTEXMANAGER_H

#include <dawn/webgpu.h>
#include <stddef.h>

struct GPUContext;

#define MAX_VERTEX_ATTRIBUTES 8

#define ATTRIB_T(type, location, fmt, member) \
    {                                          \
        .shaderLocation = (location),          \
        .format = (fmt),                       \
        .offset = offsetof(type, member),      \
    }

typedef struct {
    float x;
    float y;
    float r;
    float g;
    float b;
} Vertex;

typedef struct {
    WGPUBuffer vertexBuffer;
    WGPUBufferDescriptor vertexBufferDesc;
    WGPUVertexBufferLayout vertexBufferLayout;
    WGPUVertexAttribute vertexAttributes[MAX_VERTEX_ATTRIBUTES];
    Vertex *vertexData;
    uint64_t vertexCount;
} VertexContainer;

void setAndReturnVertexBuffer(WGPURenderPipelineDescriptor *pipelineDesc,
                              const Vertex *vertexData,
                              uint64_t vertexCount,
                              GPUContext *gpuContext,
                              VertexContainer *vertexContainer);

#endif //EM_SIM_VERTEXMANAGER_H
