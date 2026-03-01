cmake_minimum_required(VERSION 3.10)
project(q16_engine C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Game data types (platform-independent headers)
# ---------------------------------------------------------------------------
include_directories(${CMAKE_SOURCE_DIR}/src/game)

# ---------------------------------------------------------------------------
# Foundation sources (platform-independent .c files)
# ---------------------------------------------------------------------------
set(FOUNDATION_SOURCES
    # Section 1 — Math Primitives
    src/game/types/fixed16.c
    src/game/math/trig_table.c
    src/game/math/core_math.c

    # Section 2 — Memory
    src/game/memory/memory_region.c
    src/game/memory/game_memory.c

    # Section 3 — Utilities
    src/game/util/strings.c

    # Section 4 — Archive & Filesystem
    src/game/archive/archive.c

    # Section 5 — I/O & Parsing
    src/game/io/stream.c
    src/game/io/text_parser.c

    # Section 6 — World / Level
    src/game/world/sector.c
    src/game/world/level_parser.c

    # Section 7 — Render Pipeline (CPU geometry)
    src/game/render/camera.c
    src/game/render/frustum.c
    src/game/render/depth.c
    src/game/render/window.c
    src/game/render/lighting.c
    src/game/render/sbuffer.c
    src/game/render/wall_process.c
    src/game/render/flat.c
    src/game/render/adjoin.c
    src/game/render/display_list.c
    src/game/render/object_sort.c
    src/game/render/render_sector.c
)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # -------------------------------------------------------------------
    # Win32 + Glide target (cross-compiled with MinGW)
    # -------------------------------------------------------------------
    include_directories(${CMAKE_SOURCE_DIR}/lib)

    add_executable(q16_engine WIN32
        src/win9x/main.c
        ${FOUNDATION_SOURCES}
    )

    target_include_directories(q16_engine PRIVATE
        ${CMAKE_SOURCE_DIR}/src/win9x
    )

    set_target_properties(q16_engine PROPERTIES
        LINK_FLAGS "-mwindows -static"
    )
else()
    # -------------------------------------------------------------------
    # Native dev build (Mac / Linux)
    # -------------------------------------------------------------------
    add_executable(q16_dev
        src/main_dev.c
        ${FOUNDATION_SOURCES}
    )

    target_compile_options(q16_dev PRIVATE -Wall -Wextra -Wno-unused-parameter)

    add_executable(q16_gob
        src/cli/main_gob.c
        ${FOUNDATION_SOURCES}
    )

    target_compile_options(q16_gob PRIVATE -Wall -Wextra -Wno-unused-parameter)

    add_executable(q16_lab
        src/cli/main_lab.c
        ${FOUNDATION_SOURCES}
    )

    target_compile_options(q16_lab PRIVATE -Wall -Wextra -Wno-unused-parameter)

    add_executable(q16_lev
        src/cli/main_lev.c
        ${FOUNDATION_SOURCES}
    )

    target_compile_options(q16_lev PRIVATE -Wall -Wextra -Wno-unused-parameter)

    # -------------------------------------------------------------------
    # Wireframe level viewer (SDL2 + OpenGL 3.3)
    # -------------------------------------------------------------------
    find_package(SDL2 QUIET)
    if(SDL2_FOUND)
        find_package(OpenGL REQUIRED)

        add_executable(q16_view
            src/dev/main_view.c
            src/dev/gl_backend.c
            src/dev/view_input.c
            lib/glad/src/glad.c
            ${FOUNDATION_SOURCES}
        )

        target_include_directories(q16_view PRIVATE
            ${CMAKE_SOURCE_DIR}/src/dev
            ${CMAKE_SOURCE_DIR}/lib/glad/include
            ${SDL2_INCLUDE_DIRS}
        )

        target_link_libraries(q16_view PRIVATE
            ${SDL2_LIBRARIES}
            OpenGL::GL
        )

        target_compile_options(q16_view PRIVATE -Wall -Wextra -Wno-unused-parameter)
    else()
        message(STATUS "SDL2 not found — skipping q16_view target")
    endif()

    # -------------------------------------------------------------------
    # Unit tests
    # -------------------------------------------------------------------
    set(TEST_SOURCES
        src/tests/test_runner.c
        # Math
        src/tests/test_fixed16.c
        src/tests/test_core_math.c
        src/tests/test_trig.c
        # Memory
        src/tests/test_memory_region.c
        src/tests/test_game_memory.c
        # Archive & Filesystem
        src/tests/test_archive.c
        # I/O & Parsing
        src/tests/test_text_parser.c
        # World / Level
        src/tests/test_level_parser.c
        # Render Pipeline
        src/tests/test_camera.c
        src/tests/test_frustum.c
        src/tests/test_depth_window.c
        src/tests/test_sbuffer.c
        src/tests/test_lighting.c
        src/tests/test_wall_process.c
        src/tests/test_adjoin.c
        src/tests/test_display_list.c
        src/tests/test_object_sort.c
        src/tests/test_render_sector.c
        src/tests/test_flat.c
    )

    add_executable(q16_tests
        ${TEST_SOURCES}
        ${FOUNDATION_SOURCES}
    )

    # Tests include test_harness.h from src/tests/ and game headers from src/game/.
    target_include_directories(q16_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src/tests
    )

    target_compile_options(q16_tests PRIVATE -Wall -Wextra -Wno-unused-parameter)

    enable_testing()
    add_test(NAME unit COMMAND q16_tests)
endif()
