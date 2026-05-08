# assets_lib

A C23 library that embeds binary assets — textures, 3D models, audio — directly into your executable at compile time using [`#embed`](https://en.cppreference.com/w/c/preprocessor/embed). No file I/O at runtime, no asset bundling step, no missing files in deployment.

```c
unsigned char *pixels = asset_load_texture(ASSET_my_texture, &w, &h, &ch);
```

---

## Table of Contents

- [Features](#features)
- [Requirements](#requirements)
- [Dependencies](#dependencies)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [CMake API](#cmake-api)
- [C API](#c-api)
- [Dev Mode vs Shipping Mode](#dev-mode-vs-shipping-mode)
- [Supported Asset Types](#supported-asset-types)
- [Project Layout](#project-layout)
- [Verifying Embedding](#verifying-embedding)
- [License](#license)

---

## Features

- **Zero runtime file I/O** — assets are `#embed`-ed as byte arrays in the compiled binary
- **Automatic type detection** — file extension maps to `ASSET_TYPE_TEXTURE`, `MODEL`, `AUDIO`, or `BIN`
- **Generated enum IDs** — each asset gets a named constant (`ASSET_my_texture`, `ASSET_Duck`, …) usable anywhere in C
- **Named bundles** — group related assets and iterate over them in one loop
- **Dev-mode hot reload** — when built without `-DSHIPPING`, freed assets reload from disk on next use
- **Single header API** — include `<assets.h>` and you're done

---

## Requirements

| Requirement | Minimum version |
|---|---|
| CMake | 3.21 |
| C standard | **C23** (required for `#embed`) |
| GCC | 15+ |
| Clang | 19+ |
| MSVC | Not yet supported (`#embed` support pending) |

> **Note:** C23 is non-negotiable. `#embed` is a C23 feature and the library will not compile under C17 or earlier.

---

## Dependencies

All three dependencies are **header-only** and managed via [vcpkg](https://vcpkg.io).

| Library | Purpose | vcpkg package |
|---|---|---|
| [stb_image](https://github.com/nothings/stb) | PNG/JPG decoding | `stb` |
| [cgltf](https://github.com/jkuhlmann/cgltf) | glTF/GLB parsing | `cgltf` |
| [miniaudio](https://miniaud.io) | OGG/WAV/MP3 decoding | `miniaudio` |

Install them all at once:

```bash
vcpkg install stb cgltf miniaudio
```

Or drop the included `vcpkg.json` into your project root for automatic manifest-mode installation.

---

## Installation

### 1. Clone or copy the library

```bash
# As a subdirectory of your project
git clone https://github.com/yourname/assets_lib vendor/assets_lib

# Or just copy the folder in
cp -r assets_lib/ your_project/vendor/
```

### 2. Configure vcpkg

Pass the vcpkg toolchain file when running CMake:

```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DASSETS_BUILD_EXAMPLES=OFF
```

### 3. Add to your `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.21)
project(MyGame C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(vendor/assets_lib)

add_executable(my_game main.c)
target_link_libraries(my_game PRIVATE assets)
```

---

## Quick Start

### Register assets in CMake

Call `embed_assets` and/or `embed_asset_bundle` after your executable target, then call `generate_assets_file()` once at the end.

```cmake
add_executable(my_game main.c)
target_link_libraries(my_game PRIVATE assets)

# Individual assets
embed_assets(my_game
    assets/player.png
    assets/level_01.bin
)

# Named bundle — accessible in C as `ui_bundle`
embed_asset_bundle(ui
    assets/font_atlas.png
    assets/click.ogg
    assets/logo.png
)

generate_assets_file()   # Must be last
```

### Use assets in C

```c
#include <assets.h>

int main(void) {

    // ── Single asset ────────────────────────────────────────────────────

    int w, h, ch;
    // Enum name is ASSET_<filename without extension>
    unsigned char *pixels = asset_load_texture(ASSET_player, &w, &h, &ch);
    printf("player.png: %dx%d, %d channels\n", w, h, ch);
    asset_free(ASSET_player);

    // ── Bundle ──────────────────────────────────────────────────────────

    // Bundle name comes from embed_asset_bundle(ui, ...) → ui_bundle
    bundle_iter it = asset_bundle_iter(&ui_bundle);
    while (asset_bundle_iter_next(&it)) {
        switch (it.type) {
        case ASSET_TYPE_TEXTURE:
            printf("Texture %dx%d\n", it.texture.w, it.texture.h);
            break;
        case ASSET_TYPE_AUDIO:
            printf("Audio decoder ready: %p\n", (void *)it.audio);
            break;
        default:
            break;
        }
    }
    bundle_free(&ui_bundle);

    // ── Ad-hoc group (defined in C, not CMake) ──────────────────────────

    ASSET_GROUP(StartupAssets, ASSET_player, ASSET_font_atlas, ASSET_click);
    bundle_iter it2 = asset_bundle_iter(&StartupAssets_bundle);
    // ... same loop as above
    bundle_free(&StartupAssets_bundle);

    return 0;
}
```

---

## CMake API

These functions are provided by `cmake/assets.cmake`, which is included automatically when you `add_subdirectory` the library.

---

### `embed_assets(TARGET [asset_path ...])`

Registers one or more asset files to be embedded. Asset type is inferred from the file extension. Each asset gets a generated enum constant `ASSET_<name>`.

```cmake
embed_assets(my_game
    assets/terrain.png      # → ASSET_terrain
    assets/skybox.glb       # → ASSET_skybox
    assets/ambient.ogg      # → ASSET_ambient
    assets/config.bin       # → ASSET_config
)
```

The `TARGET` argument is accepted for API clarity but does not affect which target the assets are linked to — all registered assets are compiled into the `assets` library.

---

### `embed_asset_bundle(BUNDLE_NAME [asset_path ...])`

Registers a named group of assets. Generates an `asset_bundle` extern in C named `<BUNDLE_NAME>_bundle`.

```cmake
embed_asset_bundle(level_01
    assets/level_01_mesh.glb
    assets/level_01_bg.ogg
    assets/level_01_sky.png
)
# → accessible in C as `level_01_bundle`
```

Registering the same bundle name twice is a CMake fatal error.

---

### `generate_assets_file()`

Writes the generated source and header files into `${CMAKE_BINARY_DIR}/generated/`. **Must be called once, after all `embed_*` calls.** Calling it too early will silently omit any assets registered afterward.

```cmake
embed_assets(...)
embed_asset_bundle(...)
generate_assets_file()   # ← always last
```

---

## C API

### Loading

```c
// Decode a PNG/JPG asset into RGBA pixel data via stb_image
unsigned char *asset_load_texture(asset_id id, int *w, int *h, int *ch);

// Parse a glTF/GLB asset via cgltf
cgltf_data *asset_load_model(asset_id id);

// Initialise a miniaudio decoder from an OGG/WAV/MP3 asset
ma_decoder *asset_load_audio(asset_id id);
```

All three functions are lazy — the raw bytes are already in the binary, but parsing happens on first call.

---

### Freeing

```c
void asset_free(asset_id id);
```

Frees the parsed representation (`stbi_image_free`, `cgltf_free`, `ma_decoder_uninit`). In dev mode (see below) also discards the raw data so the next load re-reads from disk.

---

### Bundles

```c
// Create an iterator for a bundle
bundle_iter asset_bundle_iter(asset_bundle *bundle);

// Advance the iterator; returns 1 while items remain, 0 when exhausted
// Loads and decodes each asset as it is visited
int asset_bundle_iter_next(bundle_iter *it);

// Free all assets in a bundle
void bundle_free(asset_bundle *bundle);
```

`bundle_iter` exposes the decoded result through a union:

```c
it.type            // ASSET_TYPE_TEXTURE | MODEL | AUDIO | BIN
it.texture.pixels  // unsigned char* (RGBA)
it.texture.w
it.texture.h
it.texture.ch
it.model           // cgltf_data*
it.audio           // ma_decoder*
```

---

### Ad-hoc groups (C macro)

Define a bundle inline in C without touching CMake:

```c
// Expands to a static asset_bundle named StartupAssets_bundle
ASSET_GROUP(StartupAssets, ASSET_player, ASSET_font_atlas, ASSET_click);
bundle_iter it = asset_bundle_iter(&StartupAssets_bundle);
```

---

## Dev Mode vs Shipping Mode

The library has two runtime behaviours controlled by the `SHIPPING` preprocessor macro.

| | Dev mode (default) | Shipping mode |
|---|---|---|
| Asset source | `#embed` bytes in binary | `#embed` bytes in binary |
| After `asset_free` | Raw bytes discarded; next load re-reads from disk | Raw bytes kept |
| Use case | Iterate fast, hot-reload textures | Final build, no disk access |

To build in shipping mode:

```cmake
target_compile_definitions(my_game PRIVATE SHIPPING)
```

Or globally:

```bash
cmake -B build -DCMAKE_C_FLAGS="-DSHIPPING"
```

> In shipping mode the `raw.path` fallback in `ensure_raw_data` is never reached because `raw.data` is always non-NULL (set by `#embed`). The `path` field exists solely for dev-mode disk reloads.

---

## Supported Asset Types

| Extension | Detected type | Load function |
|---|---|---|
| `.png`, `.jpg`, `.jpeg` | `ASSET_TYPE_TEXTURE` | `asset_load_texture` |
| `.glb`, `.gltf` | `ASSET_TYPE_MODEL` | `asset_load_model` |
| `.ogg`, `.wav`, `.mp3` | `ASSET_TYPE_AUDIO` | `asset_load_audio` |
| anything else | `ASSET_TYPE_BIN` | access `ASSET_TABLE[id].raw` directly |

For `BIN` assets, access the raw bytes manually:

```c
asset_entry *e = &ASSET_TABLE[ASSET_config];
// e->raw.data and e->raw.size are always populated for embedded assets
my_config_parse(e->raw.data, e->raw.size);
```

---

## Project Layout

```
assets_lib/
├── CMakeLists.txt          # Library build definition
├── vcpkg.json              # Dependency manifest (stb, cgltf, miniaudio)
├── cmake/
│   └── assets.cmake        # embed_assets / embed_asset_bundle / generate_assets_file
├── include/
│   └── assets.h            # Public API header
├── src/
│   ├── assets.c            # Runtime implementation
│   └── third_party_impl.c  # Single-file library implementations
└── examples/
    ├── CMakeLists.txt
    ├── main.c
    └── assets/
        ├── texture_01.png
        ├── Duck.glb
        └── click_001.ogg
```

Generated at build time (do not edit):

```
build/generated/
├── assets_generated.h   # Enum constants + bundle externs
└── assets_generated.c   # #embed byte arrays + ASSET_TABLE + bundle definitions
```

---

## Verifying Embedding

**Check the binary directly** — search for the magic bytes of a known asset:

```bash
xxd my_game | grep "8950 4e47"   # \x89PNG — confirms a PNG is embedded
xxd my_game | grep "4f676700"    # OggS  — confirms an OGG is embedded
```

**Delete the source assets and run** — if the program still works, the data is in the binary:

```bash
mv assets/texture_01.png /tmp/texture_01.png.bak
./my_game   # should succeed
mv /tmp/texture_01.png.bak assets/texture_01.png
```

**Assert C23 mode at compile time:**

```c
#if __STDC_VERSION__ < 202311L
#error "C23 required — #embed will not work"
#endif
```

---

## License

MIT — see [LICENSE](LICENSE) for details.