/* =============================================================================
 * Single-File C99 Software Renderer
 * =============================================================================
 *
 *   A minimal, dependency-free software 3D renderer written in standard C99.
 *
 *   This program can load and render arbitrary Wavefront (.obj) files using a
 *   simple CPU-based rasterizer. The goal of this project was to keep things
 *   minimal, so only standard C libraries are used; there are no third-party
 *   dependencies.
 *
 *   If no .obj file is provided, the program renders a low-resolution version
 *   of the Utah Teapot using embedded .obj data included at the end of this
 *   file. (https://graphics.cs.utah.edu/teapot/)
 *
 *
 * BUILDING:
 * -----------------------------------------------------------------------------
 *
 *   Any C compiler with C99 support should work.
 *
 *   Linux / macOS:
 *     clang -std=c99 main.c -lm
 *     gcc -std=c99 main.c -lm
 *
 *   Windows:
 *     cl main.c
 *     clang -std=c99 main.c
 *
 *
 * USAGE:
 * -----------------------------------------------------------------------------
 *
 *   Render embedded teapot:
 *     ./a.out
 *
 *   Render external .obj:
 *     ./a.out path/to/model.obj
 *
 *   To change output width/height, field of view, and output file name, see the
 *   CONSTANTS section of this file.
 *
 *   To change the camera position and sun direction see lines 673 and 694 of
 *   this file.
 *
 *
 * NOTE ON DESIGN TRADEOFFS
 * -----------------------------------------------------------------------------
 *
 *   This project was intentionally constrained to a single source file with
 *   no dependencies other the the C standard library. Due to these
 *   restrictions, certain design decision prioritize minimalism over
 *   robustness and extensibility.
 *
 *   For example:
 *     - Error handling is intentionally minimal
 *     - Abstractions are introduced only when reused or logic is complex
 *     - Only the operations required by the renderer are implemented
 *
 *   In a production environment, this code would likely be refactored into
 *   multiple translation units with stronger validation, clearer module
 *   boundaries, and more complete math utilities.
 *
 *   These trade-offs are deliberate and aligned with the project's goals.
 * -----------------------------------------------------------------------------
 */

/* INCLUDES ----------------------------------------------------------------- */
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* MACROS ------------------------------------------------------------------- */
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

// helpers for fixed point math
#define FIX_SHIFT 8
#define FIX_HALF ((1 << FIX_SHIFT) >> 1)
#define FIX_MASK ((1 << FIX_SHIFT) - 1)

// helpers for dynamic arrays
#define DARRAY_DEFINE(type, name)                                              \
  typedef struct {                                                             \
    size_t capacity;                                                           \
    size_t size;                                                               \
    type *items;                                                               \
  } name;

#define DARRAY_APPEND(array, item)                                             \
  do {                                                                         \
    if (array.size >= array.capacity) {                                        \
      if (array.capacity == 0) array.capacity = 256;                           \
      else                                                                     \
        array.capacity *= 2;                                                   \
      array.items =                                                            \
        xrealloc(array.items, array.capacity * sizeof(*array.items));          \
    }                                                                          \
    array.items[array.size++] = item;                                          \
  } while (0)

#define DARRAY_FREE(array)                                                     \
  do {                                                                         \
    array.capacity = 0;                                                        \
    array.size = 0;                                                            \
    free(array.items);                                                         \
  } while (0);

/* CONSTANTS ---------------------------------------------------------------- */
const char *TEAPOT_OBJ; // See the end of this file for teapot model data

static const uint32_t WIDTH = 1920;
static const uint32_t HEIGHT = 1080;
static const float FOV_Y = 3.1415f / 4;

static const char *OUPUT_FILE_NAME = "out.bmp";

/* TYPEDEFS ----------------------------------------------------------------- */
// clang-format off
typedef int32_t fix32_t;

typedef struct { float x, y; } vec2;
typedef struct { fix32_t x, y; } fix2;
typedef struct { float x, y, z; } vec3;
typedef struct { float x, y, z, w; } vec4;
typedef struct { vec4 col0, col1, col2, col3; } mat4;
// clang-format on

typedef struct {
  vec3 position;
  float _pad0;
  vec3 normal;
  float _pad1;
  vec4 color;
  vec2 uv;
} Vertex;

typedef struct {
  mat4 model_mat;
  mat4 view_mat;
  mat4 projection_mat;
  vec3 sun_dir;
  float fov_y;
} RenderParams;

DARRAY_DEFINE(vec2, vec2_da);
DARRAY_DEFINE(vec3, vec3_da);
DARRAY_DEFINE(Vertex, Vertex_da);

/* UTILITY FUNCTIONS -------------------------------------------------------- */
void error_and_exit(const char *message) {
  fprintf(stderr, "Error: %s\n", message);
  exit(EXIT_FAILURE);
}

void *xmalloc(size_t size) {
  void *tmp = malloc(size);
  if (tmp == NULL) error_and_exit("Memory allocation failed");
  return tmp;
}

void *xrealloc(void *ptr, size_t size) {
  void *tmp = realloc(ptr, size);
  if (tmp == NULL) error_and_exit("Memory allocation failed");
  return tmp;
}

void print_progress_bar(float progress, int length, const char *prefix) {
  if (progress >= 1) {
    printf("\r%sdone%*s\n", prefix, length, "");
    return;
  }

  printf("\r%s[", prefix);
  for (int i = 0; i < length; i++) {
    putchar(i < (int)(progress * length) ? '#' : ' ');
  }
  printf("]");
  fflush(stdout);
}

/* VECTOR/MATRIX MATH FUNCTIONS --------------------------------------------- */
fix2 fix2_create(float x, float y) {
  return (fix2){(fix32_t)roundf(x * (1 << FIX_SHIFT)),
                (fix32_t)roundf(y * (1 << FIX_SHIFT))};
}

fix32_t fix_signed_area(fix2 a, fix2 b, fix2 c) {
  int64_t cross =
    (int64_t)(c.x - a.x) * (b.y - a.y) - (int64_t)(b.x - a.x) * (c.y - a.y);

  return (fix32_t)(cross >> FIX_SHIFT);
}

vec3 vec3_sub(vec3 v1, vec3 v2) {
  return (vec3){v1.x - v2.x, v1.y - v2.y, v1.z - v2.z};
}

float vec3_dot(vec3 v1, vec3 v2) {
  return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

vec3 vec3_cross(vec3 v1, vec3 v2) {
  return (vec3){
    v1.y * v2.z - v1.z * v2.y,
    v1.z * v2.x - v1.x * v2.z,
    v1.x * v2.y - v1.y * v2.x,
  };
}

vec3 vec3_norm(vec3 v) {
  float mag = sqrtf(vec3_dot(v, v));
  return (vec3){
    v.x / mag,
    v.y / mag,
    v.z / mag,
  };
}

float vec4_dot(vec4 v1, vec4 v2) {
  return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z + v1.w * v2.w;
}

mat4 mat4_mult(mat4 m1, mat4 m2) {
  vec4 row0 = {m1.col0.x, m1.col1.x, m1.col2.x, m1.col3.x};
  vec4 row1 = {m1.col0.y, m1.col1.y, m1.col2.y, m1.col3.y};
  vec4 row2 = {m1.col0.z, m1.col1.z, m1.col2.z, m1.col3.z};
  vec4 row3 = {m1.col0.w, m1.col1.w, m1.col2.w, m1.col3.w};

  mat4 m;
  m.col0 = (vec4){vec4_dot(row0, m2.col0), vec4_dot(row1, m2.col0),
                  vec4_dot(row2, m2.col0), vec4_dot(row3, m2.col0)};
  m.col1 = (vec4){vec4_dot(row0, m2.col1), vec4_dot(row1, m2.col1),
                  vec4_dot(row2, m2.col1), vec4_dot(row3, m2.col1)};
  m.col2 = (vec4){vec4_dot(row0, m2.col2), vec4_dot(row1, m2.col2),
                  vec4_dot(row2, m2.col2), vec4_dot(row3, m2.col2)};
  m.col3 = (vec4){vec4_dot(row0, m2.col3), vec4_dot(row1, m2.col3),
                  vec4_dot(row2, m2.col3), vec4_dot(row3, m2.col3)};
  return m;
}

vec4 mat4_vec4_mult(mat4 m, vec4 v) {
  vec4 row0 = {m.col0.x, m.col1.x, m.col2.x, m.col3.x};
  vec4 row1 = {m.col0.y, m.col1.y, m.col2.y, m.col3.y};
  vec4 row2 = {m.col0.z, m.col1.z, m.col2.z, m.col3.z};
  vec4 row3 = {m.col0.w, m.col1.w, m.col2.w, m.col3.w};

  return (vec4){
    vec4_dot(row0, v),
    vec4_dot(row1, v),
    vec4_dot(row2, v),
    vec4_dot(row3, v),
  };
}

mat4 mat4_look_at(vec3 pos, vec3 target, vec3 up) {
  vec3 f = vec3_norm(vec3_sub(target, pos));
  vec3 r = vec3_norm(vec3_cross(f, up));
  vec3 u = vec3_cross(f, r);

  mat4 m;
  m.col0 = (vec4){r.x, u.x, f.x, 0.0f};
  m.col1 = (vec4){r.y, u.y, f.y, 0.0f};
  m.col2 = (vec4){r.z, u.z, f.z, 0.0f};
  m.col3 =
    (vec4){-vec3_dot(r, pos), -vec3_dot(u, pos), -vec3_dot(f, pos), 1.0f};

  return m;
}

mat4 mat4_perspective(float aspect, float fov, float near, float far) {
  return (mat4){
    {1 / (aspect * tanf(fov / 2)), 0, 0, 0},
    {0, 1 / tanf(fov / 2), 0, 0},
    {0, 0, far / (far - near), 1},
    {0, 0, -far * near / (far - near), 0},
  };
}

/* FILE LOADING AND PARSING FUNCTIONS --------------------------------------- */
int load_file(const char *file_path, long *file_size, char **file) {
  FILE *fp = fopen(file_path, "rb");
  if (fp == NULL) return -1;

  fseek(fp, 0L, SEEK_END);
  *file_size = ftell(fp);
  rewind(fp);

  *file = xmalloc(*file_size + 1);

  size_t n = fread(*file, sizeof((*file)[0]), *file_size, fp);
  (*file)[n] = '\0';
  fclose(fp);

  return 0;
}

char *obj_parse_indices(char *p, int32_t *v_idx, int32_t *vt_idx,
                        int32_t *vn_idx) {
  int32_t idxs[3] = {0};

  int i = 0;
  do {
    idxs[i++] = strtol(p, &p, 10);
    if (*p == '/') p++;
  } while (*p && !isspace((unsigned char)*p));

  *v_idx = idxs[0];
  *vt_idx = idxs[1];
  *vn_idx = idxs[2];

  return p;
}

void parse_obj_str(long file_size, char *file, size_t *vertex_count,
                   Vertex **vertices) {
  vec3_da vs = {0};
  vec2_da vts = {0};
  vec3_da vns = {0};
  Vertex_da face_verts = {0};
  Vertex_da mesh_verts = {0};

  char *p = file;
  char *end = file + file_size;
  while (p < end) {
    size_t i = p - file;
    if ((i * 3) % 10000 == 0) {
      print_progress_bar((float)i / file_size, 30, "loading model: ");
    }

    while (p != file && *(p - 1) != '\n') {
      p++;
    };

    if (*p == 'f') {
      p++;

      face_verts.size = 0;

      while (*p && !isalpha(*p) && *p != '#') {
        int32_t v, vt, vn;
        p = obj_parse_indices(p, &v, &vt, &vn);
        while (*p && isspace(*p)) p++;

        Vertex vert;
        vert.position = vs.items[v - 1];
        if (vt != 0) vert.uv = vts.items[vt - 1];
        if (vn != 0) vert.normal = vns.items[vn - 1];

        DARRAY_APPEND(face_verts, vert);
      }
      p--;

      for (size_t j = 2; j < face_verts.size; j++) {
        DARRAY_APPEND(mesh_verts, face_verts.items[0]);
        DARRAY_APPEND(mesh_verts, face_verts.items[j - 1]);
        DARRAY_APPEND(mesh_verts, face_verts.items[j]);
      }
    } else if (*p == 'v') {
      float x, y, z;

      char *tmp = p + 2;
      x = strtof(tmp, &tmp);
      y = strtof(tmp, &tmp);
      z = strtof(tmp, &tmp);

      if (*(p + 1) == ' ') {
        DARRAY_APPEND(vs, ((vec3){x, y, z}));
      } else if (*(p + 1) == 't') {
        DARRAY_APPEND(vts, ((vec2){x, y}));
      } else if (*(p + 1) == 'n') {
        DARRAY_APPEND(vns, ((vec3){x, y, z}));
      }
    }

    p++;
  }
  print_progress_bar(1, 30, "loading model: ");

  // calculate normals if the model doesn't include them already
  if (vns.size == 0) {
    for (size_t i = 0; i < mesh_verts.size; i += 3) {

      vec3 p0 = mesh_verts.items[i].position;
      vec3 p1 = mesh_verts.items[i + 1].position;
      vec3 p2 = mesh_verts.items[i + 2].position;

      vec3 v1 = vec3_sub(p1, p0);
      vec3 v2 = vec3_sub(p2, p0);

      vec3 normal = vec3_norm(vec3_cross(v1, v2));

      mesh_verts.items[i].normal = normal;
      mesh_verts.items[i + 1].normal = normal;
      mesh_verts.items[i + 2].normal = normal;
    }
  }

  DARRAY_FREE(vs);
  DARRAY_FREE(vts);
  DARRAY_FREE(vns);

  *vertices = mesh_verts.items;
  *vertex_count = mesh_verts.size;

  DARRAY_FREE(face_verts);
}

/* RENDERING FUNCTIONS ------------------------------------------------------ */
int fix_fill_rule_bias(fix2 p0, fix2 p1) {
  fix2 delta = {p1.x - p0.x, p1.y - p0.y};

  bool is_top = (delta.y == 0) && (delta.x < 0);
  bool is_left = delta.y > 0;

  return (is_top || is_left) ? 0 : -1;
}

int render(const uint32_t vertex_count, const Vertex *vertices,
           uint32_t *color_buffer, float *depth_buffer, const int32_t width,
           const int32_t height, RenderParams params) {
  if (width <= 0 || height <= 0) return -1;

  // clear buffers
  for (size_t i = 0; i < width * height; i++) {
    color_buffer[i] = 0xFF000000;
    depth_buffer[i] = 1.0f;
  }

  mat4 transform = mat4_mult(mat4_mult(params.projection_mat, params.view_mat),
                             params.model_mat);

  for (size_t i = 0; i < vertex_count; i += 3) {
    if ((i * 3) % 10000 == 0) {
      print_progress_bar((float)i / vertex_count, 30, "    rendering: ");
    }

    vec3 scr_pos[3];
    vec3 normals[3];
    int clip_count = 0;

    for (int j = 0; j < 3; j++) {
      vec3 pos = vertices[i + j].position;
      vec4 clip_pos = mat4_vec4_mult(transform, (vec4){pos.x, pos.y, pos.z, 1});

      // check if vertex is out of view
      // if all three are out of view, we do not need to draw the triangle
      bool in_view_x = clip_pos.x >= -clip_pos.w && clip_pos.x <= clip_pos.w;
      bool in_view_y = clip_pos.y >= -clip_pos.w && clip_pos.y <= clip_pos.w;
      bool in_view_z = clip_pos.z >= -clip_pos.w && clip_pos.z <= clip_pos.w;
      if (!in_view_x || !in_view_y || !in_view_z) clip_count++;

      vec3 n = vertices[i + j].normal;
      // note that this normal transformation is only correct under uniform
      // scalling
      vec4 wn = mat4_vec4_mult(params.model_mat, (vec4){n.x, n.y, n.z, 0});
      normals[j] = vec3_norm((vec3){wn.x, wn.y, wn.z});

      scr_pos[j] = (vec3){
        clip_pos.x / clip_pos.w * 0.5f + 0.5f,
        clip_pos.y / clip_pos.w * 0.5f + 0.5f,
        clip_pos.z / clip_pos.w,
      };
    }
    if (clip_count == 3) continue; // triangle completely out of view

    fix2 p0 = fix2_create(scr_pos[0].x * width, scr_pos[0].y * height);
    fix2 p1 = fix2_create(scr_pos[1].x * width, scr_pos[1].y * height);
    fix2 p2 = fix2_create(scr_pos[2].x * width, scr_pos[2].y * height);

    if (fix_signed_area(p0, p1, p2) < 0) continue;

    int xmin = MAX(0, MIN(MIN(p0.x, p1.x), p2.x) >> FIX_SHIFT);
    int ymin = MAX(0, MIN(MIN(p0.y, p1.y), p2.y) >> FIX_SHIFT);
    int xmax = MIN(width - 1, MAX(MAX(p0.x, p1.x), p2.x) >> FIX_SHIFT);
    int ymax = MIN(height - 1, MAX(MAX(p0.y, p1.y), p2.y) >> FIX_SHIFT);

    for (int y = ymin; y <= ymax; y++) {
      for (int x = xmin; x <= xmax; x++) {
        fix2 p = {(x << FIX_SHIFT) + FIX_HALF, (y << FIX_SHIFT) + FIX_HALF};

        // note we don't really need to do the full signed area calculation
        // for each pixel, but the code is simpler this way
        // (and this is efficient enough for this project)
        fix32_t a0 = fix_signed_area(p1, p2, p);
        fix32_t a1 = fix_signed_area(p2, p0, p);
        fix32_t a2 = fix_signed_area(p0, p1, p);

        if (a0 < 0 || a1 < 0 || a2 < 0) continue;

        // normalized barycentric weights
        vec3 w = {
          (a0 >> FIX_SHIFT) + (float)(a0 & FIX_MASK) / (1 << FIX_SHIFT),
          (a1 >> FIX_SHIFT) + (float)(a1 & FIX_MASK) / (1 << FIX_SHIFT),
          (a2 >> FIX_SHIFT) + (float)(a2 & FIX_MASK) / (1 << FIX_SHIFT),
        };
        float w_sum = w.x + w.y + w.z;

        // depth test
        float depth =
          (scr_pos[0].z * w.x + scr_pos[1].z * w.y + scr_pos[2].z * w.z) /
          w_sum;

        int idx = x + y * width;
        if (depth >= depth_buffer[idx]) continue;
        depth_buffer[idx] = depth;

        // interpolate vertex normals for lighting calculation
        vec3 nx = (vec3){normals[0].x, normals[1].x, normals[2].x};
        vec3 ny = (vec3){normals[0].y, normals[1].y, normals[2].y};
        vec3 nz = (vec3){normals[0].z, normals[1].z, normals[2].z};

        vec3 normal = {
          (nx.x * w.x + nx.y * w.y + nx.z * w.z) / w_sum,
          (ny.x * w.x + ny.y * w.y + ny.z * w.z) / w_sum,
          (nz.x * w.x + nz.y * w.y + nz.z * w.z) / w_sum,
        };

        // diffuse lighting
        float ambient = 0.1f;
        float light = MAX(vec3_dot(normal, params.sun_dir), 0) + ambient;

        uint8_t r = (uint8_t)(fminf(fmaxf(light, 0.0f), 1.0f) * 255);
        uint8_t g = (uint8_t)(fminf(fmaxf(light, 0.0f), 1.0f) * 255);
        uint8_t b = (uint8_t)(fminf(fmaxf(light, 0.0f), 1.0f) * 255);

        color_buffer[idx] = (0xFF << 24) | (r << 16) | (g << 8) | b;
      }
    }
  }
  print_progress_bar(1, 30, "    rendering: ");
  return 0;
}

/* FILE WRITING FUNCTIONS --------------------------------------------------- */
void write_uint32_t_le(uint8_t *buffer, uint32_t data) {
  buffer[0] = data & 0xff;
  buffer[1] = (data >> 8) & 0xff;
  buffer[2] = (data >> 16) & 0xff;
  buffer[3] = (data >> 24) & 0xff;
}

int write_bmp_image(const char *file_name, int32_t width, int32_t height,
                    const uint32_t *pixels) {
  if (width <= 0 || height == 0) return -1;

  enum { HEADER_SIZE = 54 };
  uint32_t file_size = HEADER_SIZE + width * height * sizeof(uint32_t);
  uint32_t pixel_data_size = width * height * sizeof(uint32_t);

  uint8_t header[HEADER_SIZE];
  memset(header, 0, HEADER_SIZE);

  // BMP file identifier
  header[0] = 0x42;
  header[1] = 0x4d;

  write_uint32_t_le(&header[2], file_size);
  header[10] = 0x36; // offset to start of pixel data
  header[14] = 0x28; // size of DIB header

  // DIB header
  write_uint32_t_le(&header[18], width);

  // by default, BMP assumes rows are in bottom to top order
  uint32_t inverse_height = (uint32_t)(-(int32_t)(height));
  write_uint32_t_le(&header[22], inverse_height);
  header[26] = 0x01; // number of color planes (always 1)
  header[28] = 0x20; // bits per pixel
  write_uint32_t_le(&header[34], pixel_data_size);

  FILE *fp = fopen(file_name, "wb");
  if (fp == NULL) return -1;

  fwrite(header, sizeof(uint8_t), HEADER_SIZE, fp);
  fwrite(pixels, sizeof(uint32_t), width * height, fp);

  fclose(fp);

  return 0;
}

/* MAIN --------------------------------------------------------------------- */
int main(int argc, char **argv) {
  if (argc > 2) error_and_exit("Invalid command line arguments");

  size_t vertex_count;
  Vertex *vertices;
  int res;

  if (argc == 1) {
    printf("no model specified, rendering embedded teapot model\n");
    parse_obj_str(strlen(TEAPOT_OBJ), (char *)TEAPOT_OBJ, &vertex_count,
                  &vertices);

    // embedded teapot data only contains half of the teapot, so we need to
    // mirror all of the vertices and normals
    vertices = xrealloc(vertices, 2 * vertex_count * sizeof(*vertices));
    for (size_t i = 0; i < vertex_count; i += 3) {
      Vertex v0 = vertices[i];
      Vertex v1 = vertices[i + 1];
      Vertex v2 = vertices[i + 2];

      v0.position.z *= -1;
      v1.position.z *= -1;
      v2.position.z *= -1;

      v0.normal.z *= -1;
      v1.normal.z *= -1;
      v2.normal.z *= -1;

      // mirroring flips winding order, so we need to swap two vertices
      // to negate that
      vertices[vertex_count + i] = v0;
      vertices[vertex_count + i + 1] = v2;
      vertices[vertex_count + i + 2] = v1;
    }
    vertex_count = 2 * vertex_count;

  } else {
    printf("rendering model %s\n", argv[1]);
    char *fp;
    long flen;
    res = load_file(argv[1], &flen, &fp);
    if (res != 0) error_and_exit("Failed to load .obj file");
    parse_obj_str(flen, fp, &vertex_count, &vertices);
    free(fp);
  }

  // printf("vertex count: %lu\n", vertex_count);
  // for (size_t i = 0; i < vertex_count; i++) {
  //   vec3 p = vertices[i].position;
  //   printf("% 5.3f % 5.3f % 5.3f\n", p.x, p.y, p.z);
  // }

  // center model at origin
  vec3 avg_position = {0};
  for (size_t i = 0; i < vertex_count; i++) {
    avg_position.x += vertices[i].position.x;
    avg_position.y += vertices[i].position.y;
    avg_position.z += vertices[i].position.z;
  }
  avg_position.x /= (float)vertex_count;
  avg_position.y /= (float)vertex_count;
  avg_position.z /= (float)vertex_count;

  for (size_t i = 0; i < vertex_count; i++) {
    vertices[i].position = vec3_sub(vertices[i].position, avg_position);
  }

  // position camera so that model is fully visible
  float max_dist = 0;
  for (size_t i = 0; i < vertex_count; i++) {
    vec3 p = vertices[i].position;
    float dist_sq = vec3_dot(p, p);
    max_dist = (dist_sq > max_dist) ? dist_sq : max_dist;
  }
  max_dist = sqrtf(max_dist) * 1.1f;
  float min_fov = (WIDTH >= HEIGHT) ? FOV_Y : (FOV_Y * (float)WIDTH / HEIGHT);
  float camera_dist = fabsf(max_dist / sinf(min_fov * 0.5f));

  vec3 camera_pos = vec3_norm((vec3){-0.2, 0.4, 1});
  camera_pos.x *= camera_dist;
  camera_pos.y *= camera_dist;
  camera_pos.z *= camera_dist;

  // create transformation matrices
  mat4 model_mat = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};

  mat4 view_mat = mat4_look_at(camera_pos, (vec3){0, 0, 0}, (vec3){0, 1, 0});

  mat4 projection_mat =
    mat4_perspective((float)WIDTH / HEIGHT, FOV_Y, 0.1f, 2.0f * camera_dist);

  // render and write image
  uint32_t *color_buffer = xmalloc(WIDTH * HEIGHT * sizeof(*color_buffer));
  float *depth_buffer = xmalloc(WIDTH * HEIGHT * sizeof(*depth_buffer));

  RenderParams params = {
    .model_mat = model_mat,
    .view_mat = view_mat,
    .projection_mat = projection_mat,
    .sun_dir = vec3_norm((vec3){.x = 0, .y = 1, .z = 2}),
  };

  res = render(vertex_count, vertices, color_buffer, depth_buffer, WIDTH,
               HEIGHT, params);
  if (res != 0) error_and_exit("Rendering failed");

  res = write_bmp_image(OUPUT_FILE_NAME, WIDTH, HEIGHT, color_buffer);
  if (res != 0) error_and_exit("Failed to write output image");
  printf("wrote output image %s\n", OUPUT_FILE_NAME);

  free(vertices);
  free(color_buffer);
  free(depth_buffer);

  return EXIT_SUCCESS;
}

/* TEAPOT MODEL DATA -------------------------------------------------------- */
// Original model data from https://graphics.cs.utah.edu/teapot/
const char *TEAPOT_OBJ =
  "v 2.6 2 0\nv 2.5 1.9 0.23\nv 2.4 1.8 0\nv 3 1.6 0\nv 2.9 1.6 0.23\nv 2.7 "
  "1.6 0\nv 1.6 2.1 0\nv 1.5 2 0.23\nv 1.7 1.9 0\nv 2.7 1 0\nv 2.6 1.1 0.23\nv "
  "2.5 1.2 0\nv 2 0.75 0\nv 1.8 0.47 0.23\nv 1.8 0.32 0\nv -2.8 2.2 0\nv -3 "
  "2.2 0.11\nv -3.2 2.2 0\nv -2.8 2.3 0\nv -3.1 2.3 0.15\nv -3.4 2.3 0\nv -2.7 "
  "2.2 0\nv -3 2.2 0.19\nv -3.3 2.2 0\nv -2.4 1.6 0\nv -2.5 1.5 0.34\nv -2.7 "
  "1.3 0\nv -1.9 1.4 0\nv -1.8 0.91 0.48\nv -2 0.75 0.11\nv -2 0.68 0\nv 0 3 "
  "0\nv 0.33 2.8 0\nv 0.23 2.8 0.23\nv 0 2.8 0.33\nv 0.2 2.5 0\nv 0.14 2.5 "
  "0.14\nv 0 2.5 0.2\nv -0.23 2.8 0.23\nv -0.33 2.8 0\nv -0.14 2.5 0.14\nv "
  "-0.2 2.5 0\nv 0.82 2.4 0\nv 0.58 2.4 0.58\nv 0 2.4 0.82\nv 1.3 2.2 0\nv "
  "0.92 2.2 0.92\nv 0 2.2 1.3\nv -0.58 2.4 0.58\nv -0.82 2.4 0\nv -0.92 2.2 "
  "0.92\nv -1.3 2.2 0\nv 1.4 2.2 0\nv 0.99 2.2 0.99\nv 0 2.2 1.4\nv 1.4 2.3 "
  "0\nv 0.99 2.3 0.99\nv 0 2.3 1.4\nv 1.5 2.2 0\nv 1.1 2.2 1.1\nv 0 2.2 1.5\nv "
  "-0.99 2.2 0.99\nv -1.4 2.2 0\nv -0.99 2.3 0.99\nv -1.4 2.3 0\nv -1.1 2.2 "
  "1.1\nv -1.5 2.2 0\nv 1.8 1.5 0\nv 1.3 1.5 1.3\nv 0 1.5 1.8\nv 1.4 0.75 "
  "1.4\nv 0 0.75 2\nv -1.3 1.5 1.3\nv -1.8 1.5 0\nv -1.4 0.75 1.4\nv 1.8 0.23 "
  "0\nv 1.2 0.23 1.2\nv 0 0.23 1.8\nv 1.5 0 0\nv 1.1 -0 1.1\nv 0 -0 1.5\nv "
  "-1.2 0.23 1.2\nv -1.8 0.23 0\nv -1.1 -0 1.1\nv -1.5 0 0\nvn 0.029 0.19 "
  "0.98\nvn 0.39 0.79 0.48\nvn 0.81 0.13 0.57\nvn 0.28 0.092 0.96\nvn -0.26 "
  "-0.85 0.45\nvn -0.78 -0.27 0.57\nvn 0.53 0.72 0.45\nvn 0.77 0.4 0.5\nvn "
  "0.66 -0.4 0.63\nvn 0.67 -0.53 0.52\nvn 0.11 -0.091 0.99\nvn -0.7 0.48 "
  "0.52\nvn 0.23 0.69 0.69\nvn 0.76 -0.46 0.46\nvn 0.73 -0.54 0.42\nvn -0.33 "
  "-0.013 -0.94\nvn -0.45 -0.29 -0.84\nvn -0.066 0.97 -0.22\nvn -0.32 0.71 "
  "0.63\nvn 0.28 0.82 -0.5\nvn -0.49 0.28 0.82\nvn 0.55 0.53 0.65\nvn -0.042 "
  "0.31 0.95\nvn -0.5 -0.49 0.72\nvn 0.52 0.58 0.62\nvn -0.15 0.019 0.99\nvn "
  "-0.6 -0.5 0.62\nvn -0.4 0.63 0.67\nvn -0.84 0.14 0.53\nvn -0.71 -0.52 "
  "0.47\nvn -0.75 -0.51 0.42\nvn -0 0.94 0.34\nvn 0.89 0.27 0.37\nvn 0.68 0.29 "
  "0.68\nvn -0 0.29 0.96\nvn 0.8 0.51 0.33\nvn 0.6 0.54 0.6\nvn -0 0.54 "
  "0.84\nvn -0.68 0.29 0.68\nvn -0.89 0.27 0.37\nvn -0.6 0.54 0.6\nvn -0.8 "
  "0.51 0.33\nvn 0.27 0.96 0.11\nvn 0.19 0.96 0.19\nvn -0 0.96 0.28\nvn 0.3 "
  "0.95 0.12\nvn 0.21 0.95 0.21\nvn -0 0.95 0.3\nvn -0.19 0.96 0.19\nvn -0.27 "
  "0.96 0.11\nvn -0.21 0.95 0.21\nvn -0.3 0.95 0.12\nvn -0.71 0.032 -0.71\nvn "
  "-0.92 0.029 -0.38\nvn -0.14 0.99 -0.057\nvn -0.098 0.99 -0.098\nvn -0 0.032 "
  "-1\nvn -0 0.99 -0.14\nvn 0.79 0.51 0.33\nvn 0.59 0.54 0.59\nvn -0 0.54 "
  "0.84\nvn 0.71 0.032 -0.71\nvn 0.098 0.99 -0.098\nvn 0.92 0.029 -0.38\nvn "
  "0.14 0.99 -0.057\nvn -0.59 0.54 0.59\nvn -0.79 0.51 0.33\nvn 0.67 0.3 "
  "0.67\nvn -0 0.3 0.95\nvn 0.89 0.28 0.37\nvn 0.7 -0.11 0.7\nvn -0 -0.11 "
  "0.99\nvn -0.67 0.3 0.67\nvn -0.89 0.28 0.37\nvn -0.7 -0.11 0.7\nvn 0.58 "
  "-0.57 0.58\nvn -0 -0.57 0.82\nvn 0.78 -0.54 0.32\nvn 0.66 -0.7 0.27\nvn "
  "0.48 -0.73 0.48\nvn -0 -0.73 0.68\nvn -0.58 -0.57 0.58\nvn -0.48 -0.73 "
  "0.48\nvn -0.78 -0.54 0.32\nvn -0.66 -0.7 0.27\ns 1\nf 2//1 5//4 4//3 "
  "1//2\nf 3//5 6//6 5//4 2//1\nf 7//7 8//8 2//1\nf 7//7 2//1 1//2\nf 9//9 "
  "2//1 8//8\nf 9//9 3//5 2//1\nf 5//4 11//11 10//10 4//3\nf 6//6 12//12 "
  "11//11 5//4\nf 13//13 14//14 11//11\nf 13//13 11//11 12//12\nf 14//14 "
  "15//15 10//10\nf 14//14 10//10 11//11\nf 17//16 20//19 19//18 16//17\nf "
  "18//20 21//21 20//19 17//16\nf 20//19 23//23 22//22 19//18\nf 21//21 24//24 "
  "23//23 20//19\nf 23//23 26//26 25//25 22//22\nf 24//24 27//27 26//26 "
  "23//23\nf 28//28 26//26 29//29\nf 28//28 25//25 26//26\nf 30//30 29//29 "
  "26//26\nf 30//30 26//26 27//27\nf 30//30 27//27 31//31\nf 32//32 34//34 "
  "33//33\nf 32//32 35//35 34//34\nf 34//34 37//37 36//36 33//33\nf 35//35 "
  "38//38 37//37 34//34\nf 32//32 39//39 35//35\nf 32//32 40//40 39//39\nf "
  "39//39 41//41 38//38 35//35\nf 40//40 42//42 41//41 39//39\nf 37//37 44//44 "
  "43//43 36//36\nf 38//38 45//45 44//44 37//37\nf 44//44 47//47 46//46 "
  "43//43\nf 45//45 48//48 47//47 44//44\nf 41//41 49//49 45//45 38//38\nf "
  "42//42 50//50 49//49 41//41\nf 49//49 51//51 48//48 45//45\nf 50//50 52//52 "
  "51//51 49//49\nf 54//53 57//56 56//55 53//54\nf 55//57 58//58 57//56 "
  "54//53\nf 57//56 60//60 59//59 56//55\nf 58//58 61//61 60//60 57//56\nf "
  "62//62 64//63 58//58 55//57\nf 63//64 65//65 64//63 62//62\nf 64//63 66//66 "
  "61//61 58//58\nf 65//65 67//67 66//66 64//63\nf 61//61 70//69 69//68 "
  "60//60\nf 69//68 71//71 13//13 68//70\nf 70//69 72//72 71//71 69//68\nf "
  "8//8 7//7 59//59\nf 8//8 59//59 60//60\nf 8//8 60//60 69//68\nf 8//8 68//70 "
  "9//9\nf 8//8 69//68 68//70\nf 66//66 73//73 70//69 61//61\nf 67//67 74//74 "
  "73//73 66//66\nf 73//73 75//75 72//72 70//69\nf 28//28 29//29 75//75\nf "
  "28//28 75//75 73//73\nf 28//28 73//73 74//74\nf 29//29 30//30 75//75\nf "
  "72//72 78//77 77//76 71//71\nf 77//76 80//80 79//79 76//78\nf 78//77 81//81 "
  "80//80 77//76\nf 14//14 13//13 71//71\nf 14//14 71//71 77//76\nf 14//14 "
  "76//78 15//15\nf 14//14 77//76 76//78\nf 75//75 82//82 78//77 72//72\nf "
  "82//82 84//83 81//81 78//77\nf 83//84 85//85 84//83 82//82\nf 30//30 31//31 "
  "83//84\nf 30//30 83//84 82//82\nf 30//30 82//82 75//75\n";