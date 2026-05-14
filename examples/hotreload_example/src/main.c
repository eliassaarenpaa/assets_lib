#include <cgltf.h>

#include <assets.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simulates your game using an asset — prints basic info about it.
// In a real game this would upload to GPU, play audio, etc.
static void load_texture(asset_id id) {
  int w, h, ch;
  unsigned char *pixels = asset_load_texture(id, &w, &h, &ch);
  if (!pixels) {
    fprintf(stderr, "[example] failed to load texture %d\n", id);
    return;
  }
  // Sample the top-left pixel so we can see when the image actually changed
  printf("[texture] %dx%d (%d ch)  top-left rgba=(%d,%d,%d,%d)\n", w, h, ch,
         pixels[0], pixels[1], pixels[2], pixels[3]);
}

static void load_audio(asset_id id) {
  ma_decoder *dec = asset_load_audio(id);
  if (!dec) {
    fprintf(stderr, "[example] failed to load audio %d\n", id);
    return;
  }
  printf("[audio]   decoder ready at %p\n", (void *)dec);
}

static void load_model(asset_id id) {
  cgltf_data *model = asset_load_model(id);
  if (!model) {
    fprintf(stderr, "[example] failed to load model %d\n", id);
    return;
  }
  printf("[model]   %zu mesh(es)\n", model->meshes_count);
}

static void load_shader(asset_id id) {
  shader_t *shader = asset_load_shader(id);
  if (!shader) {
    fprintf(stderr, "[example] failed to load shader %d\n", id);
    return;
  }
  printf("[shader]   reflect bytes: %zu\n",
         shader ? strlen(shader->reflect) : 0);
}

#ifdef DEV
extern void dev_on_asset_changed(asset_type type, asset_id id) {
  asset_free(id);
  switch (type) {
  case ASSET_TYPE_SHADER:
    load_shader(id);
    break;
  case ASSET_TYPE_TEXTURE:
    load_texture(id);
    break;
  case ASSET_TYPE_MODEL:
    load_model(id);
    break;
  case ASSET_TYPE_AUDIO:
    load_audio(id);
    break;
  case ASSET_TYPE_BIN:
    break;
  }
}
#endif

int main(void) {
#ifdef DEV
  printf("assets_lib hot-reload example (DEV)n");
  printf("Edit any asset file on disk and watch it reload.\n");

  // Initial load — reads from disk in DEV mode
  load_texture(ASSET_texture_01);
  load_audio(ASSET_click_001);
  load_model(ASSET_Duck);
  load_shader(ASSET_hello_world);

  printf("\n");

  while (true) {
    dev_poll_asset_changes();
  }

  // Clean up
  asset_free(ASSET_texture_01);
  asset_free(ASSET_click_001);
  asset_free(ASSET_Duck);
  asset_free(ASSET_hello_world);

#else
  printf("assets_lib hot-reload example (export)\n");
  printf("Assets are baked into the binar, no disk access, no hot reload.\n\n");

  load_texture(ASSET_texture_01);
  load_audio(ASSET_click_001);
  load_model(ASSET_Duck);
  load_shader(ASSET_hello_world);

  asset_free(ASSET_texture_01);
  asset_free(ASSET_click_001);
  asset_free(ASSET_Duck);
  asset_free(ASSET_hello_world);
#endif

  printf("\nDone.\n");
  return 0;
}
