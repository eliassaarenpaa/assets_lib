include_guard(GLOBAL)

set_property(GLOBAL PROPERTY ASSET_ENUM_ENTRIES "")
set_property(GLOBAL PROPERTY ASSET_TABLE_ENTRIES "")
set_property(GLOBAL PROPERTY ASSET_EMBED_BLOCKS "")
set_property(GLOBAL PROPERTY ASSET_BUNDLE_NAMES "")

function(_asset_type_from_ext ASSET_PATH OUT_TYPE)
  get_filename_component(EXT "${ASSET_PATH}" EXT)
  string(TOLOWER "${EXT}" EXT)
  if(EXT MATCHES "\\.(png|jpg|jpeg)")
    set(${OUT_TYPE} "TEXTURE" PARENT_SCOPE)
  elseif(EXT MATCHES "\\.(glb|gltf)")
    set(${OUT_TYPE} "MODEL" PARENT_SCOPE)
  elseif(EXT MATCHES "\\.(ogg|wav|mp3)")
    set(${OUT_TYPE} "AUDIO" PARENT_SCOPE)
  else()
    set(${OUT_TYPE} "BIN" PARENT_SCOPE)
  endif()
endfunction()

function(_register_asset ASSET_PATH OUT_ENUM_NAME)
  _asset_type_from_ext("${ASSET_PATH}" TYPE)
  get_filename_component(ASSET_NAME "${ASSET_PATH}" NAME_WLE)
  get_filename_component(ABS_PATH   "${ASSET_PATH}" ABSOLUTE)
  file(TO_CMAKE_PATH "${ABS_PATH}" CLEAN_PATH)

  # Enum entry
  set_property(GLOBAL APPEND_STRING PROPERTY ASSET_ENUM_ENTRIES
    "    ASSET_${ASSET_NAME},\n")

  # Embed block:
  #   DEV    — only the path string, no binary data (loaded from disk at runtime)
  #   export — binary data via #embed, no path string
  set(EMBED_STR "#ifdef DEV\n")
  string(APPEND EMBED_STR "const char g_${ASSET_NAME}_path[] = \"${CLEAN_PATH}\";\n")
  string(APPEND EMBED_STR "#else\n")
  string(APPEND EMBED_STR "const unsigned char g_${ASSET_NAME}_data[] = {\n")
  string(APPEND EMBED_STR "    #embed \"${CLEAN_PATH}\"\n")
  string(APPEND EMBED_STR "};\n")
  string(APPEND EMBED_STR "const unsigned int g_${ASSET_NAME}_size = sizeof(g_${ASSET_NAME}_data);\n")
  string(APPEND EMBED_STR "#endif\n\n")
  set_property(GLOBAL APPEND_STRING PROPERTY ASSET_EMBED_BLOCKS "${EMBED_STR}")

  # Table entry:
  #   DEV    — NULL/0 for data/size, path set (loaded from disk at runtime)
  #   export — embedded data/size, NULL path
  set(TABLE_ENTRY "#ifdef DEV\n")
  string(APPEND TABLE_ENTRY "    [ASSET_${ASSET_NAME}] = { ASSET_TYPE_${TYPE}, { NULL, 0, g_${ASSET_NAME}_path } },\n")
  string(APPEND TABLE_ENTRY "#else\n")
  string(APPEND TABLE_ENTRY "    [ASSET_${ASSET_NAME}] = { ASSET_TYPE_${TYPE}, { g_${ASSET_NAME}_data, g_${ASSET_NAME}_size, NULL } },\n")
  string(APPEND TABLE_ENTRY "#endif\n")
  set_property(GLOBAL APPEND_STRING PROPERTY ASSET_TABLE_ENTRIES "${TABLE_ENTRY}")

  set(${OUT_ENUM_NAME} "ASSET_${ASSET_NAME}" PARENT_SCOPE)
endfunction()

function(embed_assets TARGET)
  foreach(ASSET_PATH IN LISTS ARGN)
    _register_asset("${ASSET_PATH}" _UNUSED)
  endforeach()
endfunction()

function(embed_asset_bundle BUNDLE_NAME)
  set(BUNDLE_ID_LIST "")

  foreach(ASSET_PATH IN LISTS ARGN)
    _register_asset("${ASSET_PATH}" ENUM_NAME)
    list(APPEND BUNDLE_ID_LIST "${ENUM_NAME}")
  endforeach()

  get_property(EXISTING_NAMES GLOBAL PROPERTY ASSET_BUNDLE_NAMES)
  if("${BUNDLE_NAME}" IN_LIST EXISTING_NAMES)
    message(FATAL_ERROR "embed_asset_bundle: bundle '${BUNDLE_NAME}' registered twice.")
  endif()

  set_property(GLOBAL APPEND PROPERTY ASSET_BUNDLE_NAMES "${BUNDLE_NAME}")
  set_property(GLOBAL PROPERTY "ASSET_BUNDLE_${BUNDLE_NAME}_IDS" "${BUNDLE_ID_LIST}")
endfunction()

function(generate_assets_file)
  get_property(ENUM_VAL     GLOBAL PROPERTY ASSET_ENUM_ENTRIES)
  get_property(TABL_VAL     GLOBAL PROPERTY ASSET_TABLE_ENTRIES)
  get_property(EMBD_VAL     GLOBAL PROPERTY ASSET_EMBED_BLOCKS)
  get_property(BUNDLE_NAMES GLOBAL PROPERTY ASSET_BUNDLE_NAMES)

  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")

  # ── Header ───────────────────────────────────────────────────────────────
  set(HEADER_CONTENT
"#pragma once
enum {
${ENUM_VAL}    ASSET_COUNT
};
")

  foreach(BNAME IN LISTS BUNDLE_NAMES)
    string(APPEND HEADER_CONTENT "extern asset_bundle ${BNAME}_bundle;\n")
  endforeach()

  file(WRITE "${CMAKE_BINARY_DIR}/generated/assets_generated.h" "${HEADER_CONTENT}")

  # ── Source ────────────────────────────────────────────────────────────────
  set(GEN_C_FILE "${CMAKE_BINARY_DIR}/generated/assets_generated.c")
  file(WRITE  "${GEN_C_FILE}" "#include <assets.h>\n#include \"assets_generated.h\"\n\n")
  file(APPEND "${GEN_C_FILE}" "${EMBD_VAL}")
  file(APPEND "${GEN_C_FILE}" "asset_entry ASSET_TABLE[ASSET_COUNT > 0 ? ASSET_COUNT : 1] = {\n${TABL_VAL}};\n")

  foreach(BNAME IN LISTS BUNDLE_NAMES)
    get_property(BIDS GLOBAL PROPERTY "ASSET_BUNDLE_${BNAME}_IDS")
    list(LENGTH BIDS BCOUNT)
    string(JOIN ", " BIDS_CSV ${BIDS})
    file(APPEND "${GEN_C_FILE}"
"\nstatic const asset_id _${BNAME}_ids[${BCOUNT}] = { ${BIDS_CSV} };
asset_bundle ${BNAME}_bundle = { _${BNAME}_ids, ${BCOUNT} };\n")
  endforeach()
endfunction()
