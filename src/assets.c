#include "assets.h"

#include <assert.h>
#include <cgltf.h>
#include <miniaudio.h>
#include <stb_image.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef DEV
static void asset_load_from_disk(asset_entry *e) {
  if (e->raw.data || !e->raw.path)
    return;
  FILE *f = fopen(e->raw.path, "rb");
  assert(f && "asset_load_from_disk: file not found");
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  rewind(f);
  unsigned char *buf = malloc(sz);
  fread(buf, 1, sz, f);
  fclose(f);
  e->raw.data = buf;
  e->raw.size = (unsigned int)sz;
}
#endif

unsigned char *asset_load_texture(asset_id id, int *w, int *h, int *ch) {
  asset_entry *e = &ASSET_TABLE[id];
#ifdef DEV
  asset_load_from_disk(e);
#endif
  e->parsed = stbi_load_from_memory(e->raw.data, e->raw.size, w, h, ch, 4);
  return (unsigned char *)e->parsed;
}
cgltf_data *asset_load_model(asset_id id) {
  asset_entry *e = &ASSET_TABLE[id];
#ifdef DEV
  asset_load_from_disk(e);
#endif
  cgltf_options opts = {0};
  cgltf_parse(&opts, e->raw.data, e->raw.size, (cgltf_data **)&e->parsed);
  return (cgltf_data *)e->parsed;
}
ma_decoder *asset_load_audio(asset_id id) {
  asset_entry *e = &ASSET_TABLE[id];
#ifdef DEV
  asset_load_from_disk(e);
#endif
  e->parsed = malloc(sizeof(ma_decoder));
  ma_decoder_init_memory(e->raw.data, e->raw.size, NULL,
                         (ma_decoder *)e->parsed);
  return (ma_decoder *)e->parsed;
}
void asset_free(asset_id id) {
  asset_entry *e = &ASSET_TABLE[id];
  if (!e->parsed)
    return;
  if (e->type == ASSET_TYPE_TEXTURE)
    stbi_image_free(e->parsed);
  else if (e->type == ASSET_TYPE_MODEL)
    cgltf_free((cgltf_data *)e->parsed);
  else if (e->type == ASSET_TYPE_AUDIO) {
    ma_decoder_uninit(e->parsed);
    free(e->parsed);
  }
  e->parsed = NULL;
#ifdef DEV
  // Discard raw data in dev mode to allow hot-reloading from disk on next load
  if (e->raw.path) {
    free((void *)e->raw.data);
    e->raw.data = NULL;
  }
#endif
}
bundle_iter asset_bundle_iter(asset_bundle *bundle) {
  bundle_iter it = {0};
  it.bundle = bundle;
  return it;
}
int asset_bundle_iter_next(bundle_iter *it) {
  if (it->index >= it->bundle->count)
    return 0;
  asset_id id = it->bundle->ids[it->index++];
  asset_entry *e = &ASSET_TABLE[id];
  it->type = e->type;
  if (e->type == ASSET_TYPE_TEXTURE)
    it->texture.pixels =
        asset_load_texture(id, &it->texture.w, &it->texture.h, &it->texture.ch);
  else if (e->type == ASSET_TYPE_MODEL)
    it->model = asset_load_model(id);
  else if (e->type == ASSET_TYPE_AUDIO)
    it->audio = asset_load_audio(id);
  return 1;
}
void bundle_free(asset_bundle *bundle) {
  for (int i = 0; i < bundle->count; i++)
    asset_free(bundle->ids[i]);
}
