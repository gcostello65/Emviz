//
// Created by Greg Costello on 3/4/26.
//

#include "WindowContext.h"


static WindowContext window;

WindowContext *windowInit() {
    if (!glfwInit()) {
        std::cerr << "Could not initialize GLFW!" << std::endl;
        return &window;
    }

    // WebGPU uses the platform's native surface, not an OpenGL context.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window.window = glfwCreateWindow(RESOLUTION_WIDTH, RESOLUTION_HEIGHT, WINDOW_NAME, nullptr, nullptr);


    if (!window.window) {
        std::cerr << "Could not open window!" << std::endl;
        glfwTerminate();
        return &window;
    };

    window.valid = true;
    glfwShowWindow(window.window);

    return &window;
}

WindowContext *windowGet() {
    return &window;
}

void windowShutdown() {
    glfwDestroyWindow(window.window);
    glfwTerminate();
}
