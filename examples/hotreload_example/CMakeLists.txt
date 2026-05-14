# examples/hotreload_example/CMakeLists.txt

add_executable(hotreload_example
    src/main.c
)

target_link_libraries(hotreload_example PRIVATE
    assets
)

embed_assets(hotreload_example
    ../assets/texture_01.png
    ../assets/hello-world.slang
)

embed_asset_bundle(hotreload_example test
    ../assets/Duck.glb
    ../assets/click_001.ogg
)

generate_assets_file(hotreload_example)
