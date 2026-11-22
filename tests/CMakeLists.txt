set(TARGET unit_tests)

# =========================
# Compilateur & Standard
# =========================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}) # TARGET destination

# =========================
# Requirement
# =========================
if(CMAKE_CROSSCOMPILING)
    find_package(GTest CONFIG QUIET) # built for the target in the sysroot (ex: mingw64-unit-tests image)
endif()
if(CMAKE_CROSSCOMPILING AND NOT GTest_FOUND)
    # No GTest package for the target (ex: MinGW): built from the sources
    include(FetchContent)
    FetchContent_Declare(googletest
        URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP ON
    )
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
elseif(NOT CMAKE_CROSSCOMPILING)
    find_package(GTest REQUIRED)
endif()

# =========================
# Sources
# =========================
# Project sources: the library target when it exists, else the shared list of the parent (never its entry point)
set(SRC_PROJECT "")
if(NOT XMOUSE_PACKAGE_LIB)
    foreach(source ${SRC_SHARED})
        if(IS_ABSOLUTE ${source})
            list(APPEND SRC_PROJECT ${source}) # moc headers, generated qrc
        else()
            list(APPEND SRC_PROJECT ${CMAKE_SOURCE_DIR}/${source})
        endif()
    endforeach()
endif()

set(SRC
    # Main
    xmouse/Node.cpp

    ## Core (sub-part: core)
    xmouse/core/State.cpp
    xmouse/core/Layout.cpp
    xmouse/core/Routing.cpp
    xmouse/core/Tools.cpp

    ## Security (sub-part: security)
    xmouse/security/Group.cpp

    ## Network (sub-part: net)
    xmouse/net/Mesh.cpp
    xmouse/net/Datagrams.cpp

    ## Platform (sub-part: platform)
    xmouse/platform/Keymap.cpp
)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND SRC xmouse/platform/XkbLayout.cpp) # Platform (sub-part: platform, the keyboard layout of linux)
endif()
add_executable(${TARGET} ${SRC} ${SRC_PROJECT})

# =========================
# Dependencies
# =========================
target_include_directories(${TARGET} PRIVATE ../include/ ${GENERATED_EXCEPTION_DIR} ../third_party .)
target_include_directories(${TARGET} SYSTEM PRIVATE ../third_party/libei)
target_link_libraries(${TARGET} PRIVATE ${XMOUSE_LIBS} GTest::gtest GTest::gtest_main)
target_compile_definitions(${TARGET} PRIVATE ${XMOUSE_DEFINITIONS})
if(XMOUSE_PACKAGE_LIB)
    target_link_libraries(${TARGET} PRIVATE ${TARGET_LIB})
endif()
add_dependencies(${TARGET} generated_external_exception_header)
if(WIN32)
    target_link_options(${TARGET} PRIVATE -static-libgcc -static-libstdc++ -static) # standalone .exe (no MinGW runtime dll)
endif()

# =========================
# Modes
# =========================
target_compile_options(${TARGET} PRIVATE -g -ggdb3)
if(NOT WIN32)
    # libutils_asan.a needs the AddressSanitizer runtime at the link
    target_compile_options(${TARGET} PRIVATE $<$<CONFIG:Asan>:-fsanitize=address -fno-omit-frame-pointer>)
    target_link_options(${TARGET} PRIVATE $<$<CONFIG:Asan>:-fsanitize=address>)
endif()

include(GoogleTest)
if(CMAKE_CROSSCOMPILING)
    # Listed when ctest runs (CMAKE_CROSSCOMPILING_EMULATOR, ex: wine), not at the build
    gtest_discover_tests(${TARGET}
        DISCOVERY_MODE PRE_TEST
        DISCOVERY_TIMEOUT 120 # first start of wine
    )
else()
    gtest_discover_tests(${TARGET})
endif()
