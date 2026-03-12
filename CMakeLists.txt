cmake_minimum_required(VERSION 3.29)
project(em_sim)

set(CMAKE_CXX_STANDARD 20)

 set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)

 set(DAWN_FETCH_DEPENDENCIES ON)
 set(DAWN_BUILD_TESTS OFF)
 set(TINT_BUILD_TESTS OFF)
 set(TINT_BUILD_IR_BINARY OFF)

add_subdirectory(dawn)

add_executable(em_sim main.cpp
        app/Application.cpp
        app/Application.h
        gpuLifecycle/GPUContext.cpp
        windowLifecycle/WindowContext.cpp
        rendering/PipelineFactory.cpp
        rendering/ShaderManager.h
        rendering/ShaderManager.cpp
        resourceManagement/BufferManager.cpp
        resourceManagement/BufferManager.h
        resourceManagement/VertexManager.cpp)

#if (EMSCRIPTEN)
#
# target_compile_definitions(em_sim PRIVATE WEBGPU_BACKEND_EMSCRIPTEN)
#
# # Produce a runnable webpage
# set_target_properties(em_sim PROPERTIES SUFFIX ".html")
#
#else()

 target_compile_definitions(em_sim PRIVATE WEBGPU_BACKEND_DAWN)

 find_package(glfw3 REQUIRED)
 add_subdirectory(glfw3webgpu)

target_compile_definitions(glfw3webgpu PRIVATE WGPU_TARGET_METAL)

 target_link_libraries(em_sim
         PRIVATE
         glfw
         dawn::webgpu_dawn
         dawn::dawn_proc
         glfw3webgpu
 )

#endif()

## The application's binary must find wgpu.dll or libwgpu.so at runtime,
## so we automatically copy it (it's called WGPU_RUNTIME_LIB in general)
## next to the binary.
#target_copy_webgpu_binaries(App)
