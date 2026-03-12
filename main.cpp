#include "app/Application.h"

static void defaultDrawStep(AppLoopContext* context, void* userdata) {
    (void)userdata;
    wgpuRenderPassEncoderSetPipeline(context->renderPass, context->gpu->pipeline);

    // Set vertex buffer while encoding the render pass
    wgpuRenderPassEncoderSetVertexBuffer(context->renderPass, 0, context->gpu->vertexContainer.vertexBuffer, 0, wgpuBufferGetSize(context->gpu->vertexContainer.vertexBuffer));

    wgpuRenderPassEncoderDraw(context->renderPass, context->gpu->vertexContainer.vertexCount, 1, 0, 0);
}

int main() {
    AppState app = {};
    AppConfig config = appDefaultConfig();
    // Optional: enable toy readback probe for debugging.
     config.debugReadback.enabled = true;
     config.debugReadback.bufferSize = 16;

    if (!appInit(&app, &config)) {
        return 1;
    }

    appAddRenderStep(&app, defaultDrawStep, NULL);
    appRun(&app);
    appShutdown(&app);
    return 0;
}
