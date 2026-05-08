cmake_minimum_required(VERSION 3.21)
project(assets C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC OR WIN32)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

option(ASSETS_BUILD_EXAMPLES "Build examples" ON)

# -------------------------------------------------
# 1. Setup Generated Paths & Directory
# -------------------------------------------------
set(GEN_DIR "${CMAKE_BINARY_DIR}/generated")
set(GEN_ASSET_SRC "${GEN_DIR}/assets_generated.c")
file(MAKE_DIRECTORY "${GEN_DIR}")
file(TOUCH "${GEN_ASSET_SRC}")

# -------------------------------------------------
# 2. Dependencies
# -------------------------------------------------
find_path(CGLTF_INCLUDE_DIRS "cgltf.h")
find_path(MINIAUDIO_INCLUDE_DIRS "miniaudio.h")
find_package(Stb REQUIRED)

# -------------------------------------------------
# 3. Asset Logic (POPULATE DATA FIRST)
# -------------------------------------------------
include(cmake/assets.cmake)

# -------------------------------------------------
# 4. Library (INCLUDE THE GENERATED SOURCE)
# -------------------------------------------------
add_library(assets
    src/assets.c
    src/third_party_impl.c
    "${GEN_ASSET_SRC}" # This is what was missing!
)

target_include_directories(assets
PUBLIC
    include
    "${GEN_DIR}"
    ${CGLTF_INCLUDE_DIRS}
    ${MINIAUDIO_INCLUDE_DIRS}
    ${Stb_INCLUDE_DIR}
)



# -------------------------------------------------
# 5. Examples
# -------------------------------------------------
if(ASSETS_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
