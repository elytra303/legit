cmake_minimum_required(VERSION 3.21)
project(SummerClient VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)

FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.92.0)
FetchContent_Declare(MinHook GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git GIT_TAG v1.3.8)
FetchContent_Declare(gl3w GIT_REPOSITORY https://github.com/skaslev/gl3w.git GIT_TAG master)
FetchContent_MakeAvailable(imgui MinHook gl3w)

add_library(gl3w STATIC ${gl3w_SOURCE_DIR}/src/gl3w.c)
target_include_directories(gl3w PUBLIC ${gl3w_SOURCE_DIR})

set(SUMMER_SOURCES
    src/dllmain.cpp
    src/minecraft.cpp
    src/gl_hook.cpp
    src/gui.cpp
    src/modules.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
)

add_library(SummerClient SHARED ${SUMMER_SOURCES})

target_include_directories(SummerClient PRIVATE src ${imgui_SOURCE_DIR})

target_compile_definitions(SummerClient PRIVATE
    WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
    IMGUI_IMPL_OPENGL_LOADER_CUSTOM
)

if(MSVC)
    target_compile_options(SummerClient PRIVATE /FIgl3w.h)
endif()

find_path(JNI_INCLUDE_DIR jni.h HINTS "$ENV{JAVA_HOME}/include" REQUIRED)
find_path(JNI_MD_INCLUDE_DIR jni_md.h HINTS "$ENV{JAVA_HOME}/include/win32" REQUIRED)
target_include_directories(SummerClient PRIVATE ${JNI_INCLUDE_DIR} ${JNI_MD_INCLUDE_DIR})

target_link_libraries(SummerClient PRIVATE gl3w MinHook opengl32 user32 gdi32 imm32)

set_target_properties(SummerClient PROPERTIES OUTPUT_NAME summer_client)
