// examples/simple_example_src/main.c

#include <assets.h>

#include <stdio.h>

int main(void) {

  // Load single asset:

  int w = 0;
  int h = 0;
  int ch = 0;

  unsigned char *pixels = asset_load_texture(ASSET_texture_01, &w, &h, &ch);

  printf("Texture loaded: %dx%d (%d channels)\n", w, h, ch);

  asset_free(ASSET_texture_01);

  // Load bundle of assets:

  // Or create asset groups dynamically in the code:
  // ASSET_GROUP(StartupAssets, ASSET_texture_01, ASSET_Duck, ASSET_click_001);

  bundle_iter it = asset_bundle_iter(&test_bundle);

  while (asset_bundle_iter_next(&it)) {
    switch (it.type) {
    case ASSET_TYPE_TEXTURE:
      printf("Bundle texture: %dx%d\n", it.texture.w, it.texture.h);
      break;
    case ASSET_TYPE_MODEL:
      printf("Bundle model loaded: %p\n", (void *)it.model);
      break;
    case ASSET_TYPE_AUDIO:
      printf("Bundle audio loaded: %p\n", (void *)it.audio);
      break;
      /* case ASSET_TYPE_BIN: */
    default:
      break;
    }
  }

  bundle_free(&test_bundle);

  printf("Done!\n");

  return 0;
}
