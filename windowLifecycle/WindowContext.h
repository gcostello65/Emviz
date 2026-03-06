//
// Created by Greg Costello on 3/4/26.
//

#ifndef EM_SIM_WINDOWCONTEXT_H
#define EM_SIM_WINDOWCONTEXT_H

#include <GLFW/glfw3.h>
#include <iostream>

#define RESOLUTION_WIDTH 640
#define RESOLUTION_HEIGHT 480
#define WINDOW_NAME "EM Sim"

struct WindowContext {
    GLFWwindow *window;
    bool valid = false;
};

WindowContext *windowInit();

WindowContext *windowGet();

void windowShutdown();

#endif //EM_SIM_WINDOWCONTEXT_H
