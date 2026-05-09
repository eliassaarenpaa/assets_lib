# examples/simple_example/CMakeLists.txt

add_executable(simple_example
    src/main.c
)

target_link_libraries(simple_example PRIVATE
    assets
)

embed_assets(simple_example
  ../assets/texture_01.png
)

embed_asset_bundle(simple_example test
  ../assets/Duck.glb
  ../assets/click_001.ogg
)

generate_assets_file(simple_example)



