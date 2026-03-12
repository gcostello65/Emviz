//
// Created by Greg Costello on 3/8/26.
//

#include "ShaderManager.h"

WGPUShaderModule getShaderModule(WGPUDevice *device) {
    WGPUShaderModuleDescriptor shaderDesc{};
    const char* shaderSource = R"(
        /**
         * A structure with fields labeled with builtins and locations can also be used
         * as *output* of the vertex shader, which is also the input of the fragment
         * shader.
         */
        struct VertexOutput {
            @builtin(position) position: vec4f,
            // The location here does not refer to a vertex attribute, it just means
            // that this field must be handled by the rasterizer.
            // (It can also refer to another field of another struct that would be used
            // as input to the fragment shader.)
            @location(0) color: vec3f,
        };

        struct VertexInput {
            @location(0) position: vec2f,
            @location(1) color: vec3f,
        };

        @vertex
        fn vs_main(in: VertexInput) -> VertexOutput {
            var out: VertexOutput; // create the output struct
            out.position = vec4f(in.position, 0.0, 1.0); // same as what we used to directly return
            out.color = in.color; // forward the color attribute to the fragment shader
            return out;
        }

        @fragment
        fn fs_main(in: VertexOutput) -> @location(0) vec4f {
            return vec4f(in.color, 1.0); // use the interpolated color coming from the vertex shader
        }
        )";

    WGPUShaderSourceWGSL shaderCodeDesc{};
    shaderCodeDesc.chain.next = nullptr;
    shaderCodeDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
    shaderCodeDesc.code.data = shaderSource;
    shaderCodeDesc.code.length = WGPU_STRLEN;

    shaderDesc.nextInChain = &shaderCodeDesc.chain;

    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(*device, &shaderDesc);
    return shaderModule;
}
