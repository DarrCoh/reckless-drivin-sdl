cmake_minimum_required(VERSION 3.16)

# Support macOS 11+ (Big Sur) — first version with Apple Silicon
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

project(RecklessDrivin VERSION 1.0.4 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Default to Release build (optimized)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Enable Link-Time Optimization for Release builds
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

# Find SDL2 - try framework first on macOS, then pkg-config, then find_package
if(APPLE)
    # Look for SDL2.framework in the build dir (placed by build-mac-app.sh)
    # or standard system locations. Only match actual .framework bundles.
    find_path(SDL2_FRAMEWORK_DIR SDL2.framework
        PATHS "${CMAKE_BINARY_DIR}" "${CMAKE_SOURCE_DIR}" "/Library/Frameworks"
        NO_DEFAULT_PATH
    )
    if(SDL2_FRAMEWORK_DIR)
        set(SDL2_FRAMEWORK "${SDL2_FRAMEWORK_DIR}/SDL2.framework")
        message(STATUS "Found SDL2.framework at: ${SDL2_FRAMEWORK}")
        # Official SDL2.framework is universal; build a fat binary
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
    endif()
endif()

if(NOT SDL2_FRAMEWORK)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(SDL2 IMPORTED_TARGET sdl2)
    endif()
    if(NOT SDL2_FOUND)
        find_package(SDL2 REQUIRED)
    endif()
endif()

# Source files
set(SOURCES
    src/main.c
    src/platform_sdl.c
    src/resources.c
    src/lzrw.c
    src/error.c
    src/packs.c
    src/gameframe.c
    src/gameinitexit.c
    src/objects.c
    src/objectPhysics.c
    src/objectCollision.c
    src/objectControl.c
    src/renderframe.c
    src/roaddraw.c
    src/sprites.c
    src/rle.c
    src/textfx.c
    src/particlefx.c
    src/screen.c
    src/screenfx.c
    src/renderframezoom.c
    src/sound.c
    src/input.c
    src/interface.c
    src/preferences.c
    src/random.c
    src/register.c
    src/high.c
    src/pause.c
    src/gameendseq.c
    src/textrender.c
)

# macOS app bundle: include icon as a resource
if(APPLE)
    set(ICON_FILE ${CMAKE_CURRENT_SOURCE_DIR}/packaging/osx/Reckless.icns)
    set_source_files_properties(${ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    add_executable(RecklessDrivin MACOSX_BUNDLE ${SOURCES} ${ICON_FILE})
    set_target_properties(RecklessDrivin PROPERTIES
        MACOSX_RPATH TRUE
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "@executable_path/../Frameworks"

        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/packaging/osx/Info.plist.in
        MACOSX_BUNDLE_EXECUTABLE_NAME RecklessDrivin
        MACOSX_BUNDLE_ICON_FILE Reckless
        MACOSX_BUNDLE_BUNDLE_NAME "Reckless Drivin'"
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    )
elseif(WIN32)
    add_executable(RecklessDrivin WIN32 ${SOURCES})
else()
    add_executable(RecklessDrivin ${SOURCES})
endif()

# Include directories: our port headers, internal source headers, and SDL2
target_include_directories(RecklessDrivin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SDL2_INCLUDE_DIRS}
)

# Preprocessor definitions
target_compile_definitions(RecklessDrivin PRIVATE SDL_PORT=1)

# Compiler warnings
if(MSVC)
    target_compile_options(RecklessDrivin PRIVATE /W3)
else()
    target_compile_options(RecklessDrivin PRIVATE -Wall)
endif()

# Link libraries
if(SDL2_FRAMEWORK)
    target_link_libraries(RecklessDrivin PRIVATE ${SDL2_FRAMEWORK}/SDL2)
    target_include_directories(RecklessDrivin PRIVATE ${SDL2_FRAMEWORK}/Headers)
elseif(TARGET PkgConfig::SDL2)
    target_link_libraries(RecklessDrivin PRIVATE PkgConfig::SDL2)
elseif(TARGET SDL2::SDL2)
    target_link_libraries(RecklessDrivin PRIVATE SDL2::SDL2)
else()
    target_link_libraries(RecklessDrivin PRIVATE ${SDL2_LIBRARIES})
    target_include_directories(RecklessDrivin PRIVATE ${SDL2_INCLUDE_DIRS})
endif()

# Math library (not needed on MSVC; linked by default)
if(NOT MSVC)
    target_link_libraries(RecklessDrivin PRIVATE m)
endif()

# Windows: link SDL2main for WinMain entry point
if(WIN32 AND TARGET SDL2::SDL2main)
    target_link_libraries(RecklessDrivin PRIVATE SDL2::SDL2main)
endif()

# macOS-specific: link Cocoa framework (needed by SDL2)
if(APPLE)
    target_link_libraries(RecklessDrivin PRIVATE "-framework Cocoa")
endif()

# Copy the Data file next to the executable (inside .app bundle on macOS)
if(APPLE)
    add_custom_command(
        TARGET RecklessDrivin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Data"
            "$<TARGET_FILE_DIR:RecklessDrivin>/../Resources/Data"
        COMMENT "Copying Data file into app bundle Resources"
    )
else()
    add_custom_command(
        TARGET RecklessDrivin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Data"
            "$<TARGET_FILE_DIR:RecklessDrivin>/Data"
        COMMENT "Copying Data file to build directory"
    )
endif()

# Windows: bundle MSVC runtime DLLs next to the exe so users don't need
# the Visual C++ Redistributable installed separately. Targets Windows 10/11,
# so the Universal CRT is assumed present and not bundled.
if(WIN32 AND MSVC)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    include(InstallRequiredSystemLibraries)
    foreach(_crt_lib ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS})
        add_custom_command(TARGET RecklessDrivin POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_crt_lib}" "$<TARGET_FILE_DIR:RecklessDrivin>"
            VERBATIM
        )
    endforeach()
endif()
