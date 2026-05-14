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

void dev_poll_asset_changes(void) {
  for (int i = 0; i < ASSET_COUNT_VALUE; i++) {
    asset_entry *e = &ASSET_TABLE[i];

    time_t mtime = 0;

    switch (e->type) {
    case ASSET_TYPE_SHADER:
      if (!e->shader.slang_path)
        continue;
      else
        mtime = _get_mtime(e->shader.slang_path);
      break;
    case ASSET_TYPE_TEXTURE:
    case ASSET_TYPE_MODEL:
    case ASSET_TYPE_AUDIO:
    case ASSET_TYPE_BIN:
      if (!e->raw.path)
        continue;
      else
        mtime = _get_mtime(e->raw.path);
      break;
    }

    // First poll — just record the timestamp, don't reload
    if (_asset_mtimes[i] == 0) {
      _asset_mtimes[i] = mtime;
      continue;
    }

    if (mtime != _asset_mtimes[i]) {
      _asset_mtimes[i] = mtime;

      switch (e->type) {
      case ASSET_TYPE_SHADER:
        printf("[asset] changed: %s\n", e->shader.slang_path);
        break;
      case ASSET_TYPE_TEXTURE:
      case ASSET_TYPE_MODEL:
      case ASSET_TYPE_AUDIO:
      case ASSET_TYPE_BIN:
        printf("[asset] changed: %s\n", e->raw.path);
        break;
      }

      dev_on_asset_changed(e->type, i);
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
  if (e->raw.parsed)
    return (unsigned char *)e->raw.parsed;
  _ensure_raw(e);
  e->raw.parsed =
      stbi_load_from_memory(e->raw.data, (int)e->raw.size, w, h, ch, 4);
  return (unsigned char *)e->raw.parsed;
}

cgltf_data *asset_load_model(asset_id id) {
  asset_entry *e = &ASSET_TABLE[id];
  if (e->raw.parsed)
    return (cgltf_data *)e->raw.parsed;
  _ensure_raw(e);
  cgltf_options opts = {0};
  cgltf_parse(&opts, e->raw.data, e->raw.size, (cgltf_data **)&e->raw.parsed);
  return (cgltf_data *)e->raw.parsed;
}

ma_decoder *asset_load_audio(asset_id id) {
  asset_entry *e = &ASSET_TABLE[id];
  if (e->raw.parsed)
    return (ma_decoder *)e->raw.parsed;
  _ensure_raw(e);
  e->raw.parsed = malloc(sizeof(ma_decoder));
  ma_decoder_init_memory(e->raw.data, e->raw.size, NULL,
                         (ma_decoder *)e->raw.parsed);
  return (ma_decoder *)e->raw.parsed;
}

static shader_t *load_shader_from_files(const char *blob_path,
                                        const char *json_path) {
  // --- Read the blob ---
  FILE *blob_file = fopen(blob_path, "rb");
  if (!blob_file)
    return NULL;

  fseek(blob_file, 0, SEEK_END);
  size_t blob_size = ftell(blob_file);
  rewind(blob_file);

  void *blob = malloc(blob_size);
  fread(blob, 1, blob_size, blob_file);
  fclose(blob_file);

  // --- Read the reflect ---
  FILE *json_file = fopen(json_path, "rb");
  if (!json_file) {
    free(blob);
    return NULL;
  }

  fseek(json_file, 0, SEEK_END);
  size_t json_size = ftell(json_file);
  rewind(json_file);

  char *reflect = malloc(json_size + 1); // +1 for null terminator
  fread(reflect, 1, json_size, json_file);
  reflect[json_size] = '\0';
  fclose(json_file);

  // --- Pack into shader_t ---
  shader_t *sh = malloc(sizeof(shader_t));
  sh->blob = blob;
  sh->blob_size = blob_size;
  sh->reflect = reflect;
  return sh;
}

#ifdef _WIN32
#include <windows.h>

static int run_slangc(const char *cmd) {
  STARTUPINFOA si = {.cb = sizeof(si)};
  PROCESS_INFORMATION pi = {0};

  // CreateProcess needs a mutable buffer
  char cmd_buf[2048];
  snprintf(cmd_buf, sizeof(cmd_buf), "%s", cmd);

  if (!CreateProcessA(NULL, cmd_buf, NULL, NULL, FALSE, 0, NULL, NULL, &si,
                      &pi))
    return -1;

  WaitForSingleObject(pi.hProcess, INFINITE);

  DWORD exit_code;
  GetExitCodeProcess(pi.hProcess, &exit_code);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  return (int)exit_code;
}
#else
static int run_slangc(const char *cmd) { return system(cmd); }
#endif

shader_t *asset_load_shader(asset_id id) {
  asset_entry *e = &ASSET_TABLE[id];
  assert(e->type == ASSET_TYPE_SHADER);

#ifdef DEV
  // If already compiled, return cached — hot-reload path calls asset_free first
  // if (e->shader.data.blob)
  //   return &e->shader.data;
  // Retry loop — editor atomic saves can cause a brief window where
  // the file doesn't exist between delete and write
  FILE *test = NULL;
  for (int attempt = 0; attempt < 10; attempt++) {
    test = fopen(e->shader.slang_path, "rb");
    if (test)
      break;
#ifdef _WIN32
    Sleep(50); // 50ms
#else
    struct timespec ts = {.tv_nsec = 50000000};
    nanosleep(&ts, NULL);
#endif
  }

  if (!test) {
    fprintf(stderr, "[shader] source file not accessible after retries: %s\n",
            e->shader.slang_path);
    return NULL;
  }
  fclose(test);

  char blob_tmp[512], json_tmp[512];
  snprintf(blob_tmp, sizeof(blob_tmp), "%s.tmp.blob", e->shader.slang_path);
  snprintf(json_tmp, sizeof(json_tmp), "%s.tmp.json", e->shader.slang_path);

  char cmd[2048];
  snprintf(cmd, sizeof(cmd),
           "\"%s\" \"%s\" -target %s -o \"%s\" -reflection-json \"%s\"",
           SLANGC_PATH, e->shader.slang_path, SLANGC_TARGET, blob_tmp,
           json_tmp);

  printf("[shader] slang_path: %s\n", e->shader.slang_path);
  printf("[shader] cmd: %s\n", cmd);

  int rc = run_slangc(cmd);

  if (rc != 0)
    return NULL; // compile error — keep old shader alive

  shader_t *sh = load_shader_from_files(blob_tmp, json_tmp);
  remove(blob_tmp);
  remove(json_tmp);
  if (!sh)
    return NULL;

  e->shader.data = *sh; // cache into entry
  free(sh);
  return &e->shader.data;
#else
  assert(e->shader.data.blob);
  assert(e->shader.data.reflect);
  assert(e->shader.data.blob_size > 0);
  return &e->shader.data;
#endif
}

void asset_free(asset_id id) {
  asset_entry *e = &ASSET_TABLE[id];
  if (!e)
    return;

  // 1. Free Shaders
#ifdef DEV
  if (e->type == ASSET_TYPE_SHADER) {
    if (e->shader.data.blob)
      free((void *)e->shader.data.blob);
    if (e->shader.data.reflect)
      free((void *)e->shader.data.reflect);
    e->shader.data.blob = NULL;
    e->shader.data.reflect = NULL;
    return;
  }
#endif

  // 2. Free Parsed Data (Texture, Model, Audio)
  if (e->raw.parsed) {
    if (e->type == ASSET_TYPE_TEXTURE)
      stbi_image_free(e->raw.parsed);
    else if (e->type == ASSET_TYPE_MODEL)
      cgltf_free((cgltf_data *)e->raw.parsed);
    else if (e->type == ASSET_TYPE_AUDIO) {
      ma_decoder_uninit((ma_decoder *)e->raw.parsed);
      free(e->raw.parsed);
    }
    e->raw.parsed = NULL;
  }

  // 3. Free Disk Buffer (DEV mode)
#ifdef DEV
  if (e->raw.data) {
    free((void *)e->raw.data);
    e->raw.data = NULL; // CRITICAL: Prevent double free
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
