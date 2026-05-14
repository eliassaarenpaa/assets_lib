#ifndef ASSETS_H
#define ASSETS_H

#include <stddef.h>
#include <time.h>
// Forward declarations
typedef struct cgltf_data cgltf_data;
typedef struct ma_decoder ma_decoder;

typedef enum {
  ASSET_TYPE_SHADER,
  ASSET_TYPE_TEXTURE,
  ASSET_TYPE_MODEL,
  ASSET_TYPE_AUDIO,
  ASSET_TYPE_BIN,
} asset_type;

typedef struct {
  const unsigned char *data;
  unsigned int size;
#ifdef DEV
  const char *path; // Disk path for dev mode
#endif
  void *parsed;
} asset_raw;

typedef struct {
  const void *blob;
  size_t blob_size;
  const char *reflect; // null-terminated JSON
} shader_t;

typedef struct {
#ifdef DEV
  const char *slang_path; // absolute path to .slang source
#endif
  shader_t data;
} asset_shader;

typedef int asset_id;

typedef struct {
  asset_type type;
  union {
    asset_raw raw;
    asset_shader shader;
  };
} asset_entry;

extern asset_entry ASSET_TABLE[];
extern const int ASSET_COUNT_VALUE;
extern time_t _asset_mtimes[];

// API
unsigned char *asset_load_texture(asset_id id, int *w, int *h, int *ch);
cgltf_data *asset_load_model(asset_id id);
ma_decoder *asset_load_audio(asset_id id);
shader_t *asset_load_shader(asset_id id);
void asset_free(asset_id id);

// Bundles
typedef struct {
  const asset_id *ids;
  int count;
} asset_bundle;
typedef struct {
  asset_bundle *bundle;
  int index;
  asset_type type;
  union {
    struct {
      unsigned char *pixels;
      int w, h, ch;
    } texture;
    cgltf_data *model;
    ma_decoder *audio;
  };
} bundle_iter;
#define ASSET_GROUP(group_name, ...)                                           \
  static const asset_id _##group_name##_ids[] = {__VA_ARGS__};                 \
  static asset_bundle group_name##_bundle = {                                  \
      _##group_name##_ids, sizeof(_##group_name##_ids) / sizeof(asset_id)}
bundle_iter asset_bundle_iter(asset_bundle *bundle);
int asset_bundle_iter_next(bundle_iter *it);
void bundle_free(asset_bundle *bundle);

#ifdef DEV
extern void dev_on_asset_changed(asset_type type, asset_id id);
void dev_poll_asset_changes(void);
#endif
#endif
