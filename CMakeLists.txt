cmake_minimum_required(VERSION 3.15)
project(IdeaWalker VERSION 0.1.5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DIDEAWALKER_VERSION="v0.1.5-beta")

# --- Debug & Sanitization (Uncomment to enable) ---
# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address,undefined -g -O1 -fno-omit-frame-pointer")
# set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address,undefined")


# --- Build Resilience Settings ---
# Prevent CMake from trying to contact git server on every run (avoids network hangs)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON) 
# Show download progress to debug hangs
set(FETCHCONTENT_QUIET OFF)

# Core system dependencies
find_package(Threads REQUIRED)
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

include(FetchContent)

# --- Dependencies ---

# 1. nlohmann_json (Has CMake: Use MakeAvailable with Shallow Clone)
message(STATUS "Fetching nlohmann_json...")
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
    GIT_SHALLOW TRUE # Speed up and reduce timeout risk
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(json)

# 2. cpp-httplib (Has CMake: Use MakeAvailable with Shallow Clone)
message(STATUS "Fetching cpp-httplib...")
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.1
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(httplib)

# 3. ImGui (Non-CMake project: Use Populate)
message(STATUS "Fetching imgui (v1.90.9)...")
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.9
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_Populate(imgui)

# Create explicit target for ImGui
if(NOT TARGET imgui)
    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui PUBLIC 
        ${imgui_SOURCE_DIR} 
        ${imgui_SOURCE_DIR}/backends
    )
    # ImGui needs SDL2 and OpenGL linked for its backends
    target_link_libraries(imgui PRIVATE SDL2::SDL2 OpenGL::GL)
    target_compile_definitions(imgui PUBLIC IMGUI_USE_WCHAR32 IMGUI_DEFINE_MATH_OPERATORS)
endif()
set(IMGUI_DIR ${imgui_SOURCE_DIR})

# 4. ImNodes (Non-CMake project: Use Populate)
message(STATUS "Fetching imnodes (pinned)...")
FetchContent_Declare(
    imnodes
    GIT_REPOSITORY https://github.com/nelarius/imnodes.git
    GIT_TAG master # Use master as stable hash fetch failed with shallow clone
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    GIT_SUBMODULES "" # Prevent fetching vcpkg or other submodules
)
FetchContent_Populate(imnodes)

if(NOT TARGET imnodes)
    add_library(imnodes STATIC ${imnodes_SOURCE_DIR}/imnodes.cpp)
    target_include_directories(imnodes PUBLIC ${imnodes_SOURCE_DIR})
    target_link_libraries(imnodes PUBLIC imgui)
endif()
set(IMNODES_DIR ${imnodes_SOURCE_DIR})

# 5. Whisper.cpp (Has CMake: Use MakeAvailable)
message(STATUS "Fetching whisper.cpp (v1.5.4)...")
FetchContent_Declare(
    whisper
    GIT_REPOSITORY https://github.com/ggerganov/whisper.cpp.git
    GIT_TAG v1.5.4
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(whisper)

# Robust target detection for whisper
if(TARGET whisper)
    set(WHISPER_TARGET whisper)
elseif(TARGET whisper.cpp)
    set(WHISPER_TARGET whisper.cpp)
else()
    message(FATAL_ERROR "Whisper target not found after fetch.")
endif()

# --- Main Executable ---

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
    src/application/SuggestionService.cpp
    src/infrastructure/FileSystemArtifactScanner.cpp
    src/infrastructure/EmbeddingCache.cpp
    src/infrastructure/PersistenceService.cpp
    src/infrastructure/writing/WritingEventStoreFs.cpp
    src/infrastructure/writing/WritingTrajectoryRepositoryFs.cpp
    src/application/writing/WritingTrajectoryService.cpp
    src/application/writing/ExportService.cpp
    src/ui/panels/WritingPanels.cpp
    src/ui/panels/DefensePanel.cpp

)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    nlohmann_json::nlohmann_json
    httplib::httplib
    SDL2::SDL2
    OpenGL::GL
    Threads::Threads
    dl
    ${WHISPER_TARGET}
    imgui
    imnodes
)

# --- Test Executable (Headless) ---
add_executable(ideawalker_test
    src/test/ConcurrencyTest.cpp
    src/application/ConversationService.cpp
    src/infrastructure/PathUtils.cpp
    src/infrastructure/PersistenceService.cpp
)

target_include_directories(ideawalker_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ideawalker_test PRIVATE
    Threads::Threads
    dl
)
