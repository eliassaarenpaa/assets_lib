# examples/CMakeLists.txt

add_executable(asset_example
    main.c
)

target_link_libraries(asset_example PRIVATE
    assets
)

embed_assets(asset_example
  assets/texture_01.png
)
embed_asset_bundle(test
  assets/Duck.glb
  assets/click_001.ogg
)
generate_assets_file()
