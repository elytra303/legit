cmake_minimum_required(VERSION 3.21)
project(SummerClient VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    # statically link the CRT so the DLL has no VCRUNTIME dependency
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)

# Dear ImGui is vendored in third_party/imgui (v1.92.0) so the build works
# offline; MinHook is still pulled from the network.
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
if(NOT EXISTS ${IMGUI_DIR}/imgui.h)
    message(FATAL_ERROR "Dear ImGui not found at ${IMGUI_DIR}")
endif()

FetchContent_Declare(MinHook GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git GIT_TAG v1.3.4)
FetchContent_MakeAvailable(MinHook)

set(SUMMER_SOURCES
    src/main.cpp
    src/mc/minecraft.cpp
    src/summer/client.cpp
    src/summer/config.cpp
    src/summer/jvm.cpp
    src/summer/module.cpp
    src/gui/gui.cpp
    src/overlay/overlay.cpp
    src/util/log.cpp
    src/util/random.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

add_library(SummerClient SHARED ${SUMMER_SOURCES})

target_include_directories(SummerClient PRIVATE
    src ${IMGUI_DIR} ${IMGUI_DIR}/backends
)

target_compile_definitions(SummerClient PRIVATE
    WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
)

find_path(JNI_INCLUDE_DIR jni.h HINTS "$ENV{JAVA_HOME}/include" REQUIRED)
find_path(JNI_MD_INCLUDE_DIR jni_md.h HINTS "$ENV{JAVA_HOME}/include/win32" REQUIRED)
target_include_directories(SummerClient PRIVATE ${JNI_INCLUDE_DIR} ${JNI_MD_INCLUDE_DIR})

target_link_libraries(SummerClient PRIVATE minhook opengl32 user32 gdi32 imm32)

set_target_properties(SummerClient PROPERTIES OUTPUT_NAME summer_client)
