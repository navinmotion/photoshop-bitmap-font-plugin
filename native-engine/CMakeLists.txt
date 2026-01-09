cmake_minimum_required(VERSION 3.15)
project(TextAtlasEngine)

set(CMAKE_CXX_STANDARD 20)

# --- Dependencies (via vcpkg) ---

# 1. JSON
find_package(nlohmann_json CONFIG REQUIRED)

# 2. FreeType
find_package(Freetype REQUIRED)

# 3. HarfBuzz
find_package(harfbuzz CONFIG REQUIRED)

# 4. ZLib (Required by uWebSockets for compression)
find_package(ZLIB REQUIRED)

# 5. LibUV (Required by uSockets for the event loop)
find_package(libuv CONFIG REQUIRED)

# 6. uSockets (Core networking)
find_library(USOCKETS_LIBRARY 
    NAMES uSockets usockets
    HINTS "${CMAKE_TOOLCHAIN_FILE}/../../installed/x64-windows/lib"
)

find_path(USOCKETS_INCLUDE_DIR 
    NAMES libusockets.h
    HINTS "${CMAKE_TOOLCHAIN_FILE}/../../installed/x64-windows/include"
)

if(USOCKETS_LIBRARY AND USOCKETS_INCLUDE_DIR)
    message(STATUS "Found uSockets: ${USOCKETS_LIBRARY}")
else()
    message(FATAL_ERROR "Could not find uSockets. Run 'vcpkg install usockets'.")
endif()

# 7. uWebSockets (C++ wrapper)
find_path(UWEBSOCKETS_INCLUDE_DIR 
    NAMES uWebSockets/App.h
    HINTS "${CMAKE_TOOLCHAIN_FILE}/../../installed/x64-windows/include"
)

if(UWEBSOCKETS_INCLUDE_DIR)
    message(STATUS "Found uWebSockets headers in: ${UWEBSOCKETS_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "Could not find uWebSockets. Run 'vcpkg install uwebsockets'.")
endif()

# --- Executable ---
add_executable(TextAtlasEngine TextEngine.cpp)

# --- Linking ---
target_link_libraries(TextAtlasEngine
    PRIVATE
    nlohmann_json::nlohmann_json
    Freetype::Freetype
    harfbuzz::harfbuzz
    ZLIB::ZLIB        
    libuv::uv      
    ${USOCKETS_LIBRARY}
)

# Explicitly add the include directories found manually
target_include_directories(TextAtlasEngine PRIVATE 
    ${USOCKETS_INCLUDE_DIR}
    ${UWEBSOCKETS_INCLUDE_DIR}
)

# Windows specific
if(WIN32)
    target_link_libraries(TextAtlasEngine PRIVATE ws2_32)
endif()