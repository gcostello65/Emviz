//
// Created by Greg Costello on 3/8/26.
//

#include "ShaderManager.h"

WGPUShaderModule getShaderModule(WGPUDevice *device) {
    WGPUShaderModuleDescriptor shaderDesc{};
    const char* shaderSource = R"(
        @vertex
        fn vs_main(@builtin(vertex_index) vi : u32) -> @builtin(position) vec4f {
            var pos = array<vec2f, 3>(
                vec2f(0.0, 0.5),
                vec2f(-0.5, -0.5),
                vec2f(0.5, -0.5)
            );
            return vec4f(pos[vi], 0.0, 1.0);
        }

        @fragment
        fn fs_main() -> @location(0) vec4f {
            return vec4f(0.0, 0.4, 1.0, 1.0);
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
