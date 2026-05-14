#include "assets_base.h"

#include <assert.h>
#include <cgltf.h>
#include <miniaudio.h>
#include <stb_image.h>
#include <stdio.h>
#include <stdlib.h>

// ── Dev mode: disk loading & hot reload ──────────────────────────────────────

#ifdef DEV
#include <sys/stat.h>
#include <time.h>

static time_t _get_mtime(const char *path) {
  struct stat st;
  if (stat(path, &st) != 0)
    return 0;
  return st.st_mtime;
}

static void _asset_load_from_disk(asset_entry *e) {
  if (e->raw.data || !e->raw.path)
    return;

  FILE *f = fopen(e->raw.path, "rb");
  if (!f) {
    fprintf(stderr, "[assets] failed to open: %s\n", e->raw.path);
    return;
  }
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  rewind(f);

  unsigned char *buf = malloc((size_t)sz);
  fread(buf, 1, (size_t)sz, f);
  fclose(f);

  e->raw.data = buf;
  e->raw.size = (unsigned int)sz;
}

void asset_dev_poll(void) {
  for (int i = 0; i < ASSET_COUNT_VALUE; i++) {
    // printf("i: %i\n", i);
    asset_entry *e = &ASSET_TABLE[i];
    if (!e->raw.path)
      continue;

    time_t mtime = _get_mtime(e->raw.path);

    // First poll — just record the timestamp, don't reload
    if (_asset_mtimes[i] == 0) {
      _asset_mtimes[i] = mtime;
      continue;
    }

    if (mtime != _asset_mtimes[i]) {
      _asset_mtimes[i] = mtime;
      printf("[assets] changed: %s\n", e->raw.path);
      asset_free(i); // frees parsed + raw, leaving path intact
    }
  }
}
#endif // DEV

// ── Internal: ensure raw bytes are present before decoding ───────────────────

static void _ensure_raw(asset_entry *e) {
#ifdef DEV
  _asset_load_from_disk(e);
#endif
  assert(e->raw.data &&
         "asset has no data — missing embed or disk load failed");
}

// ── Public API ───────────────────────────────────────────────────────────────

unsigned char *asset_load_texture(asset_id id, int *w, int *h, int *ch) {
  asset_entry *e = &ASSET_TABLE[id];

  if (e->parsed) {
#ifdef DEV
    // For hot-reload
    asset_free(id);
#else
    // Non-dev builds just return cached
    return (unsigned char *)e->parsed;
#endif
  }

  _ensure_raw(e);
  e->parsed = stbi_load_from_memory(e->raw.data, (int)e->raw.size, w, h, ch, 4);
  return (unsigned char *)e->parsed;
}

cgltf_data *asset_load_model(asset_id id) {
  asset_entry *e = &ASSET_TABLE[id];
  if (e->parsed)
    return (cgltf_data *)e->parsed;
  _ensure_raw(e);
  cgltf_options opts = {0};
  cgltf_parse(&opts, e->raw.data, e->raw.size, (cgltf_data **)&e->parsed);
  return (cgltf_data *)e->parsed;
}

ma_decoder *asset_load_audio(asset_id id) {
  asset_entry *e = &ASSET_TABLE[id];
  if (e->parsed)
    return (ma_decoder *)e->parsed;
  _ensure_raw(e);
  e->parsed = malloc(sizeof(ma_decoder));
  ma_decoder_init_memory(e->raw.data, e->raw.size, NULL,
                         (ma_decoder *)e->parsed);
  return (ma_decoder *)e->parsed;
}

void asset_free(asset_id id) {
  asset_entry *e = &ASSET_TABLE[id];

  // Free parsed representation
  if (e->parsed) {
    if (e->type == ASSET_TYPE_TEXTURE)
      stbi_image_free(e->parsed);
    else if (e->type == ASSET_TYPE_MODEL)
      cgltf_free((cgltf_data *)e->parsed);
    else if (e->type == ASSET_TYPE_AUDIO) {
      ma_decoder_uninit((ma_decoder *)e->parsed);
      free(e->parsed);
    }
    e->parsed = NULL;
  }

#ifdef DEV
  // In dev mode, raw.data was malloc'd from disk — free it so the next
  // load re-reads the file. path is left intact (it's a string literal).
  if (e->raw.path) {
    free((void *)e->raw.data);
    e->raw.data = NULL;
    e->raw.size = 0;
  }
#endif
}

// ── Bundles ──────────────────────────────────────────────────────────────────

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
