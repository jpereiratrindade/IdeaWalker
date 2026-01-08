cmake_minimum_required(VERSION 3.15)
project(IdeaWalker VERSION 0.1.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DIDEAWALKER_VERSION="v0.1.4-beta")

include(FetchContent)

# --- Dependencies ---

# 1. nlohmann_json
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

# 2. cpp-httplib
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.1
)
FetchContent_MakeAvailable(httplib)

# 3. ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG master
)
FetchContent_MakeAvailable(imgui)
set(IMGUI_DIR ${imgui_SOURCE_DIR})

# 4. ImNodes
FetchContent_Declare(
    imnodes
    GIT_REPOSITORY https://github.com/nelarius/imnodes.git
    GIT_TAG master
)
FetchContent_GetProperties(imnodes)
if(NOT imnodes_POPULATED)
    FetchContent_Populate(imnodes)
endif()
set(IMNODES_DIR ${imnodes_SOURCE_DIR})

# Whisper.cpp
FetchContent_Declare(
    whisper
    GIT_REPOSITORY https://github.com/ggerganov/whisper.cpp.git
    GIT_TAG master
)
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(whisper)

# --- Executable ---

add_executable(${PROJECT_NAME}
    main.cpp
    src/app/IdeaWalkerApp.cpp
    src/ui/AppState.cpp
    src/ui/UiRenderer.cpp
    src/ui/ConversationPanel.cpp
    src/infrastructure/OllamaAdapter.cpp
    src/infrastructure/FileRepository.cpp
    src/infrastructure/PathUtils.cpp
    src/infrastructure/WhisperCppAdapter.cpp
    src/application/OrganizerService.cpp
    src/application/ConversationService.cpp
    src/application/DocumentIngestionService.cpp
    src/application/ContextAssembler.cpp
    src/infrastructure/FileSystemArtifactScanner.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    ${IMNODES_DIR}/imnodes.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMNODES_DIR}
)

# --- Libraries ---
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
    nlohmann_json::nlohmann_json
    httplib::httplib
    SDL2::SDL2
    OpenGL::GL
    dl
    whisper
)

target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_USE_WCHAR32 IMGUI_DEFINE_MATH_OPERATORS)

# Copy backends headers if fetched
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.h")
    target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_IMPL_OPENGL_LOADER_GL3W)
endif()
