cmake_minimum_required(VERSION 3.16)

# Support macOS 11+ (Big Sur) — first version with Apple Silicon
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

project(RecklessDrivin C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Default to Release build (optimized)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Enable Link-Time Optimization for Release builds
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

# Find SDL2 - try pkg-config first, then find_package
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(SDL2 IMPORTED_TARGET sdl2)
endif()
if(NOT SDL2_FOUND)
    find_package(SDL2 REQUIRED)
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
    set(ICON_FILE ${CMAKE_CURRENT_SOURCE_DIR}/Reckless.icns)
    set_source_files_properties(${ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    add_executable(RecklessDrivin MACOSX_BUNDLE ${SOURCES} ${ICON_FILE})
    set_target_properties(RecklessDrivin PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
        MACOSX_BUNDLE_EXECUTABLE_NAME RecklessDrivin
        MACOSX_BUNDLE_ICON_FILE Reckless
        MACOSX_BUNDLE_BUNDLE_NAME "Reckless Drivin'"
    )
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
target_compile_options(RecklessDrivin PRIVATE -Wall)

# Link libraries
if(TARGET PkgConfig::SDL2)
    target_link_libraries(RecklessDrivin PRIVATE PkgConfig::SDL2 m)
elseif(TARGET SDL2::SDL2)
    target_link_libraries(RecklessDrivin PRIVATE SDL2::SDL2 m)
else()
    target_link_libraries(RecklessDrivin PRIVATE ${SDL2_LIBRARIES} m)
    target_include_directories(RecklessDrivin PRIVATE ${SDL2_INCLUDE_DIRS})
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
    # Bundle SDL2 dylib so the .app is fully self-contained
    find_library(SDL2_DYLIB_PATH SDL2 HINTS
        /opt/homebrew/lib /usr/local/lib /opt/local/lib
        ENV DYLD_LIBRARY_PATH
    )
    if(SDL2_DYLIB_PATH)
        # Resolve symlinks and get the install name at configure time
        get_filename_component(SDL2_DYLIB_REAL "${SDL2_DYLIB_PATH}" REALPATH)
        execute_process(
            COMMAND otool -D "${SDL2_DYLIB_REAL}"
            OUTPUT_VARIABLE SDL2_INSTALL_NAME
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        # otool -D output: first line is the file path, second line is the install name
        string(REGEX REPLACE ".*\n" "" SDL2_INSTALL_NAME "${SDL2_INSTALL_NAME}")
        message(STATUS "SDL2 dylib: ${SDL2_DYLIB_REAL}")
        message(STATUS "SDL2 install name: ${SDL2_INSTALL_NAME}")
        add_custom_command(
            TARGET RecklessDrivin POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_DIR:RecklessDrivin>/Contents/Frameworks"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SDL2_DYLIB_REAL}"
                "$<TARGET_BUNDLE_DIR:RecklessDrivin>/Contents/Frameworks/libSDL2.dylib"
            COMMAND chmod u+w
                "$<TARGET_BUNDLE_DIR:RecklessDrivin>/Contents/Frameworks/libSDL2.dylib"
            COMMAND install_name_tool -change
                "${SDL2_INSTALL_NAME}"
                "@executable_path/../Frameworks/libSDL2.dylib"
                "$<TARGET_FILE:RecklessDrivin>"
            COMMAND install_name_tool -id
                "@executable_path/../Frameworks/libSDL2.dylib"
                "$<TARGET_BUNDLE_DIR:RecklessDrivin>/Contents/Frameworks/libSDL2.dylib"
            COMMENT "Bundling SDL2 dylib into app"
        )
    endif()
    # Ad-hoc sign so macOS doesn't show the prohibition overlay
    add_custom_command(
        TARGET RecklessDrivin POST_BUILD
        COMMAND xattr -cr "$<TARGET_BUNDLE_DIR:RecklessDrivin>"
        COMMAND codesign --force --deep -s - "$<TARGET_BUNDLE_DIR:RecklessDrivin>"
        COMMENT "Ad-hoc signing app bundle"
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
