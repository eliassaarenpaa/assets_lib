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
  elseif(_ext MATCHES "\\.(slang)$")
    set(_type "ASSET_TYPE_SHADER")
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

  set(GEN_H    "${GEN_DIR}/assets_generated.h")
  set(GEN_C    "${GEN_DIR}/assets_generated.c")
  set(GEN_WRAP "${GEN_DIR}/assets.h")

  # -------------------------------------------------------------------------
  # Locate slangc once — outside the loop
  # -------------------------------------------------------------------------

  find_program(SLANGC slangc REQUIRED)
  message(STATUS "slangc found: ${SLANGC}")
  target_compile_definitions(assets PRIVATE
    SLANGC_PATH="${SLANGC}"
  )

  # -------------------------------------------------------------------------
  # Buffers
  # -------------------------------------------------------------------------

  set(ENUMS "")
  set(TABLES "")
  set(EMBEDS "")
  set(_EMBED_TRACK "")

  foreach(PATH IN LISTS _assets)

    get_filename_component(ABS "${PATH}" ABSOLUTE)
    file(TO_CMAKE_PATH "${ABS}" PATH_NORM)

    _sanitize_identifier("${PATH_NORM}" ID)
    _asset_type_from_extension("${PATH_NORM}" TYPE)

    string(APPEND ENUMS "    ASSET_${ID},\n")

    if(DEFINED DEV AND DEV)
      # ── DEV mode ────────────────────────────────────────────────────────
      if(TYPE STREQUAL "ASSET_TYPE_SHADER")
        string(APPEND TABLES
          "    { .type = ${TYPE}, .shader = { .slang_path = \"${PATH_NORM}\" } },\n"
        )
      else()
        string(APPEND TABLES
          "    { .type = ${TYPE}, .raw = { .path = \"${PATH_NORM}\" } },\n"
        )
      endif()

    else()
      # ── RELEASE mode ────────────────────────────────────────────────────
      if(TYPE STREQUAL "ASSET_TYPE_SHADER")

        set(SHADER_DIR "${GEN_DIR}/shaders")
        file(MAKE_DIRECTORY "${SHADER_DIR}")

        set(BLOB_OUT "${SHADER_DIR}/${ID}.blob")
        set(JSON_OUT "${SHADER_DIR}/${ID}.reflect.json")

        set(BLOB_DATA "asset_${ID}_blob")
        set(BLOB_SIZE "asset_${ID}_blob_size")
        set(JSON_DATA "asset_${ID}_reflect")

        # ── Build-time shader compilation ──────────────────────────────
        # Paths are known at configure time; files are produced at build
        # time before the C compiler processes the #embed directives.
        add_custom_command(
          OUTPUT  "${BLOB_OUT}" "${JSON_OUT}"
          DEPENDS "${PATH_NORM}"
          COMMAND "${SLANGC}" "${PATH_NORM}"
                  -target ${SLANG_TARGET}
                  -o      "${BLOB_OUT}"
                  -reflection-json "${JSON_OUT}"
          COMMENT "Compiling shader ${ID}"
          VERBATIM
        )

        # Drive the custom command by attaching its outputs as sources
        # on a per-shader target, then make the assets library depend on it
        add_custom_target(shader_${ID}_compile
          DEPENDS "${BLOB_OUT}" "${JSON_OUT}"
        )
        add_dependencies(assets shader_${ID}_compile)

        string(APPEND TABLES
          "    { .type = ${TYPE}, .shader = { .data = { .blob = ${BLOB_DATA}, .blob_size = ${BLOB_SIZE}, .reflect = (const char *)${JSON_DATA} } } },\n"
        )

        if(NOT ID IN_LIST _EMBED_TRACK)
          list(APPEND _EMBED_TRACK ${ID})
          string(APPEND EMBEDS
            "static const unsigned char ${BLOB_DATA}[] = {\n"
            "    #embed \"${BLOB_OUT}\"\n"
            "};\n"
            "static const unsigned int ${BLOB_SIZE} = sizeof(${BLOB_DATA});\n"
            "static const unsigned char ${JSON_DATA}[] = {\n"
            "    #embed \"${JSON_OUT}\"\n"
            "    , 0\n"
            "};\n\n"
          )
        endif()

      else()
        # ── All other asset types — single #embed ──────────────────────
        set(DATA "asset_${ID}_data")
        set(SIZE "asset_${ID}_size")

        string(APPEND TABLES
          "    { .type = ${TYPE}, .raw = { .data = ${DATA}, .size = ${SIZE} } },\n"
        )

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
    endif()

  endforeach()

  string(APPEND ENUMS "    ASSET_COUNT\n")

  # -------------------------------------------------------------------------
  # Header
  # -------------------------------------------------------------------------

  file(WRITE "${GEN_H}" "#pragma once\n\n#include <assets.h>\n\n")
  file(APPEND "${GEN_H}" "enum {\n${ENUMS}};\n\n")
  file(APPEND "${GEN_H}" "extern asset_entry ASSET_TABLE[ASSET_COUNT];\n\n")

  get_target_property(_bundles ${TARGET} ASSET_BUNDLES)
  if(_bundles)
    foreach(B IN LISTS _bundles)
      file(APPEND "${GEN_H}" "extern asset_bundle ${B}_bundle;\n")
    endforeach()
    file(APPEND "${GEN_H}" "\n")
  endif()

  file(WRITE "${GEN_WRAP}"
    "#pragma once\n\n#include <assets_base.h>\n#include \"assets_generated.h\"\n"
  )

  # -------------------------------------------------------------------------
  # Source
  # -------------------------------------------------------------------------

  file(WRITE  "${GEN_C}" "#include \"assets_generated.h\"\n\n")
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
