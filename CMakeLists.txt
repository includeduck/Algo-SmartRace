cmake_minimum_required(VERSION 3.20)
project(SmartRacingChampionship CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
    # We deliberately use plain C stdio (fopen, fgets, vsnprintf) rather than
    # the MSVC-only "_s" variants, for portability — silence that warning.
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# smartrace_core — the backend library (owned by Wasay, 24I-3071).
# No STL, no <algorithm>, no smart pointers (see ProjectRestrictions.txt),
# and NO dependency on any GUI library. This is the only library the GUI
# (src/app/, owned by Rayyan, 24I-3000) needs to link, through the single
# header src/api/SmartRaceApi.h.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SMARTRACE_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/core/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/model/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/algo/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/sim/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/io/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/api/*.cpp"
)

add_library(smartrace_core STATIC ${SMARTRACE_CORE_SOURCES})
target_include_directories(smartrace_core PUBLIC "${CMAKE_SOURCE_DIR}/src")

# ---------------------------------------------------------------------------
# smartrace_tests — unit + integration tests (console exe, no GUI deps)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SMARTRACE_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/tests/*.cpp"
)
if(SMARTRACE_TEST_SOURCES)
    add_executable(smartrace_tests ${SMARTRACE_TEST_SOURCES})
    target_link_libraries(smartrace_tests PRIVATE smartrace_core)
endif()

# ---------------------------------------------------------------------------
# smartrace_cli — headless driver / API usage sample (console exe)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SMARTRACE_CLI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/cli/*.cpp"
)
if(SMARTRACE_CLI_SOURCES)
    add_executable(smartrace_cli ${SMARTRACE_CLI_SOURCES})
    target_link_libraries(smartrace_cli PRIVATE smartrace_core)
endif()

# ---------------------------------------------------------------------------
# smartrace_bench — the 5 required experiments (Project.pdf section 7)
# ---------------------------------------------------------------------------
if(EXISTS "${CMAKE_SOURCE_DIR}/bench")
    file(GLOB_RECURSE SMARTRACE_BENCH_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/bench/*.cpp"
    )
    if(SMARTRACE_BENCH_SOURCES)
        add_executable(smartrace_bench ${SMARTRACE_BENCH_SOURCES})
        target_link_libraries(smartrace_bench PRIVATE smartrace_core)
    endif()
endif()

# ---------------------------------------------------------------------------
# smartrace — the GUI executable (owned by Rayyan, src/app/). This block
# only activates once src/app/ exists, so the project configures and builds
# cleanly before his code lands too.
# ---------------------------------------------------------------------------
if(EXISTS "${CMAKE_SOURCE_DIR}/src/app")
    file(GLOB_RECURSE SMARTRACE_APP_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/src/app/*.cpp"
    )
    if(SMARTRACE_APP_SOURCES)
        add_executable(smartrace WIN32 ${SMARTRACE_APP_SOURCES})
        target_link_libraries(smartrace PRIVATE smartrace_core)
        # Rayyan: add your GUI library here, e.g.
        #   target_include_directories(smartrace PRIVATE ${CMAKE_SOURCE_DIR}/third_party/raylib/include)
        #   target_link_libraries(smartrace PRIVATE ${CMAKE_SOURCE_DIR}/third_party/raylib/lib/raylib.lib winmm)
    endif()
endif()
