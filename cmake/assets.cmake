# -----------------------------------------------------------------------------
# assets.cmake (refactored)
# -----------------------------------------------------------------------------

include_guard(GLOBAL)

# -----------------------------------------------------------------------------
# Helpers
# -----------------------------------------------------------------------------

function(_sanitize_identifier INPUT OUTPUT)
  get_filename_component(_name "${INPUT}" NAME_WE)
  string(REGEX REPLACE "[^A-Za-z0-9_]" "_" _name "${_name}")
  set(${OUTPUT} "${_name}" PARENT_SCOPE)
endfunction()

function(_asset_type_from_extension PATH OUTPUT)
  get_filename_component(_ext "${PATH}" EXT)
  string(TOLOWER "${_ext}" _ext)

  if(_ext MATCHES "\\.(png|jpg|jpeg|bmp|tga|gif)$")
    set(_type "ASSET_TYPE_TEXTURE")
  elseif(_ext MATCHES "\\.(glb|gltf)$")
    set(_type "ASSET_TYPE_MODEL")
  elseif(_ext MATCHES "\\.(ogg|wav|mp3|flac)$")
    set(_type "ASSET_TYPE_AUDIO")
  else()
    set(_type "ASSET_TYPE_BIN")
  endif()

  set(${OUTPUT} "${_type}" PARENT_SCOPE)
endfunction()

function(_asset_add TARGET PATH)
  get_target_property(_list ${TARGET} ASSETS_REGISTERED)
  if(NOT _list)
    set(_list "")
  endif()

  list(APPEND _list "${PATH}")
  set_target_properties(${TARGET} PROPERTIES ASSETS_REGISTERED "${_list}")
endfunction()

# -----------------------------------------------------------------------------
# Public API
# -----------------------------------------------------------------------------

function(embed_assets TARGET)
  foreach(PATH IN LISTS ARGN)
    _asset_add(${TARGET} "${PATH}")
  endforeach()
endfunction()

function(embed_asset_bundle TARGET BUNDLE_NAME)
  set(_assets "${ARGN}")

  foreach(PATH IN LISTS _assets)
    _asset_add(${TARGET} "${PATH}")
  endforeach()

  get_target_property(_bundles ${TARGET} ASSET_BUNDLES)
  if(NOT _bundles)
    set(_bundles "")
  endif()

  list(APPEND _bundles "${BUNDLE_NAME}")

  set_target_properties(${TARGET} PROPERTIES
    ASSET_BUNDLES "${_bundles}"
    ASSET_BUNDLE_${BUNDLE_NAME} "${_assets}"
  )
endfunction()

# -----------------------------------------------------------------------------
# Generate
# -----------------------------------------------------------------------------

function(generate_assets_file TARGET)
  if(NOT TARGET ${TARGET})
    message(FATAL_ERROR "Target '${TARGET}' does not exist")
  endif()

  get_target_property(_assets ${TARGET} ASSETS_REGISTERED)
  if(NOT _assets)
    message(STATUS "No assets for ${TARGET}")
    return()
  endif()

  list(REMOVE_DUPLICATES _assets)

  set(GEN_DIR "${CMAKE_BINARY_DIR}/generated/assets/${TARGET}")
  file(MAKE_DIRECTORY "${GEN_DIR}")

  set(GEN_H "${GEN_DIR}/assets_generated.h")
  set(GEN_C "${GEN_DIR}/assets_generated.c")
  set(GEN_WRAP "${GEN_DIR}/assets.h")

  # -------------------------------------------------------------------------
  # Buffers
  # -------------------------------------------------------------------------

  set(ENUMS "")
  set(TABLES "")
  set(EMBEDS "")

  # track emitted embed symbols (avoid duplicates)
  set(_EMBED_TRACK "")

  foreach(PATH IN LISTS _assets)

    get_filename_component(ABS "${PATH}" ABSOLUTE)
    file(TO_CMAKE_PATH "${ABS}" PATH_NORM)

    _sanitize_identifier("${PATH_NORM}" ID)
    _asset_type_from_extension("${PATH_NORM}" TYPE)

    string(APPEND ENUMS "    ASSET_${ID},\n")

    if(DEFINED DEV AND DEV)
      string(APPEND TABLES
        "    { .type = ${TYPE}, .raw = { .path = \"${PATH_NORM}\" } },\n"
      )
    else()

      # embed symbols
      set(DATA "asset_${ID}_data")
      set(SIZE "asset_${ID}_size")

      string(APPEND TABLES
        "    { .type = ${TYPE}, .raw = { .data = ${DATA}, .size = ${SIZE} } },\n"
      )

      # ensure single emission
      if(NOT ID IN_LIST _EMBED_TRACK)
        list(APPEND _EMBED_TRACK ${ID})

        string(APPEND EMBEDS
          "static const unsigned char ${DATA}[] = {\n"
          "    #embed \"${PATH_NORM}\"\n"
          "};\n"
          "static const unsigned int ${SIZE} = sizeof(${DATA});\n\n"
        )
      endif()

    endif()

  endforeach()

  string(APPEND ENUMS "    ASSET_COUNT\n")

  # -------------------------------------------------------------------------
  # Header
  # -------------------------------------------------------------------------

  file(WRITE "${GEN_H}" "#pragma once\n\n#include <assets.h>\n\n")

  file(APPEND "${GEN_H}" "enum {\n${ENUMS}};\n\n")
  file(APPEND "${GEN_H}" "extern asset_entry ASSET_TABLE[ASSET_COUNT];\n\n")

  # bundles
  get_target_property(_bundles ${TARGET} ASSET_BUNDLES)
  if(_bundles)
    foreach(B IN LISTS _bundles)
      file(APPEND "${GEN_H}" "extern asset_bundle ${B}_bundle;\n")
    endforeach()
    file(APPEND "${GEN_H}" "\n")
  endif()

  # wrapper
  file(WRITE "${GEN_WRAP}" "#pragma once\n\n#include <assets_base.h>\n#include \"assets_generated.h\"\n")

  # -------------------------------------------------------------------------
  # Source
  # -------------------------------------------------------------------------

  file(WRITE "${GEN_C}" "#include \"assets_generated.h\"\n\n")

  file(APPEND "${GEN_C}" "${EMBEDS}\n")

  file(APPEND "${GEN_C}"
    "asset_entry ASSET_TABLE[ASSET_COUNT] = {\n"
    "${TABLES}"
    "};\n\n"
  )

  file(APPEND "${GEN_C}" "time_t _asset_mtimes[ASSET_COUNT];\n\n")
  file(APPEND "${GEN_C}" "const int ASSET_COUNT_VALUE = ASSET_COUNT;\n\n")

  # -------------------------------------------------------------------------
  # Bundles
  # -------------------------------------------------------------------------

  if(_bundles)
    foreach(B IN LISTS _bundles)

      get_target_property(_assets_bundle ${TARGET} ASSET_BUNDLE_${B})

      set(_ids "")
      foreach(PATH IN LISTS _assets_bundle)
        _sanitize_identifier("${PATH}" ID)
        string(APPEND _ids "    ASSET_${ID},\n")
      endforeach()

      list(LENGTH _assets_bundle CNT)

      file(APPEND "${GEN_C}"
        "static asset_id ${B}_ids[] = {\n${_ids}};\n\n"
        "asset_bundle ${B}_bundle = {\n"
        "    .ids = ${B}_ids,\n"
        "    .count = ${CNT}\n"
        "};\n\n"
      )

    endforeach()
  endif()

  # -------------------------------------------------------------------------
  # Attach
  # -------------------------------------------------------------------------

  target_sources(${TARGET} PRIVATE "${GEN_C}")
  target_include_directories(${TARGET} PUBLIC "${GEN_DIR}")

endfunction()
