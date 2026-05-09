#include <cgltf.h>

#include <assets.h>
#include <stdio.h>
#include <stdlib.h>

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

int main(void) {
#ifdef DEV
  printf("assets_lib hot-reload example (DEV)n");
  printf("Edit any asset file on disk and watch it reload.\n");
  printf("Press ENTER to poll for changes, q+ENTER to quit.\n\n");

  // Initial load — reads from disk in DEV mode
  load_texture(ASSET_texture_01);
  load_audio(ASSET_click_001);
  load_model(ASSET_Duck);

  printf("\n");

  char buf[8];
  while (1) {
    // In a real game loop this would be called once per frame with no
    // blocking — here we block on input so the example is easy to drive
    // manually without a render loop.
    printf("[ press ENTER to poll, q to quit ]\n");
    if (!fgets(buf, sizeof(buf), stdin))
      break;
    if (buf[0] == 'q')
      break;

    // Check every registered asset's file timestamp.
    // Any asset whose file changed on disk since the last poll is freed
    // automatically — the next asset_load_* call re-reads it.
    asset_dev_poll();

    // Re-load all assets. If nothing changed, the cached parsed pointer
    // is returned immediately. If asset_dev_poll freed an asset becaload
    // its file changed, this re-reads and re-decodes it from disk.
    load_texture(ASSET_texture_01);
    load_audio(ASSET_click_001);
    load_model(ASSET_Duck);

    printf("\n");
  }

  // Clean up
  asset_free(ASSET_texture_01);
  asset_free(ASSET_click_001);
  asset_free(ASSET_Duck);

#else
  printf("assets_lib hot-reload example (export)\n");
  printf("Assets are baked into the binar, no disk access, no hot reload.\n\n");

  load_texture(ASSET_texture_01);
  load_audio(ASSET_click_001);
  load_model(ASSET_Duck);

  asset_free(ASSET_texture_01);
  asset_free(ASSET_click_001);
  asset_free(ASSET_Duck);
#endif

  printf("\nDone.\n");
  // return 0;
}
