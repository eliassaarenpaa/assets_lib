# examples/CMakeLists.txt

add_executable(simple_example
    simple_example/src/main.c
)

target_link_libraries(simple_example PRIVATE
    assets
)

embed_assets(simple_example
  assets/texture_01.png
)
embed_asset_bundle(test
  assets/Duck.glb
  assets/click_001.ogg
)
generate_assets_file()
