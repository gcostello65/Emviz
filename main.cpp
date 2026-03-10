#include "app/Application.h"

static void defaultDrawStep(AppLoopContext* context, void* userdata) {
    (void)userdata;
    wgpuRenderPassEncoderSetPipeline(context->renderPass, context->gpu->pipeline);
    wgpuRenderPassEncoderDraw(context->renderPass, 3, 1, 0, 0);
}

int main() {
    AppState app = {};
    AppConfig config = appDefaultConfig();

    if (!appInit(&app, &config)) {
        return 1;
    }

    appAddLoopStep(&app, defaultDrawStep, NULL);
    appRun(&app);
    appShutdown(&app);
    return 0;
}
