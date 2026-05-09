cmake_minimum_required(VERSION 3.21)
project(assets C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC OR WIN32)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

option(DEV "Development mode" OFF)
option(ASSETS_BUILD_EXAMPLES "Build examples" ON)

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
)

target_include_directories(assets
PUBLIC
    include
    "${GEN_DIR}"
    ${CGLTF_INCLUDE_DIRS}
    ${MINIAUDIO_INCLUDE_DIRS}
    ${Stb_INCLUDE_DIR}
)



if(DEV)
  target_compile_definitions(assets PUBLIC DEV)
endif()

# -------------------------------------------------
# 5. Examples
# -------------------------------------------------
if(ASSETS_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
