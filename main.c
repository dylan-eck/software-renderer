/*
 * Single File Software Renderer
 *
 * This is a very basic software renderer that can load and render obj files.
 *
 * Things to note:
 * This is a single threaded program, so large images can take a long time to
 * render
 */

#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* MACROS =================================================================== */
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define FX_SHAMT 4
#define FX_MULT (1 << FX_SHAMT)
#define FX_HALF (FX_MULT >> 1)

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
          realloc(array.items, array.capacity * sizeof(*array.items));         \
    }                                                                          \
    array.items[array.size++] = item;                                          \
  } while (0)

#define DARRAY_FREE(array)                                                     \
  do {                                                                         \
    array.capacity = 0;                                                        \
    array.size = 0;                                                            \
    free(array.items);                                                         \
  } while (0);

/* GLOBALS ================================================================== */
// See the end of this file for teapot model data
enum { TEAPOT_VERTEX_COUNT = 255, TEAPOT_INDEX_COUNT = 351 };
extern const float TEAPOT_VERTICES[];
extern const uint8_t TEAPOT_INDICES[];

static const uint32_t WIDTH = 64;
static const uint32_t HEIGHT = 64;

static const bool WIREFRAME_ENABLED = false;

static const char *OUPUT_FILE_NAME = "out.bmp";

/* TYPE DEFINITIONS ========================================================= */
typedef struct {
  float x, y;
} vec2;

typedef struct {
  int x, y;
} ivec2;

typedef struct {
  int32_t x;
  int32_t y;
} fxvec2;

typedef struct {
  float x, y, z;
} vec3;
typedef struct {
  float x, y, z, w;
} vec4;
typedef struct {
  vec4 col0, col1, col2, col3;
} mat4;

typedef struct {
  vec3 position;
  float p0;
  vec3 normal;
  float p1;
  vec4 color;
  vec2 uv;
} Vertex;

DARRAY_DEFINE(vec2, vec2_da);
DARRAY_DEFINE(ivec2, ivec2_da)
DARRAY_DEFINE(vec3, vec3_da);
DARRAY_DEFINE(Vertex, Vertex_da);

/* FUNCTION DECLARATIONS ==================================================== */
vec3 vec3_add(vec3 v1, vec3 v2);
vec2 vec2_sub(vec2 v1, vec2 v2);
vec3 vec3_sub(vec3 v1, vec3 v2);
vec3 vec3_div(vec3 v, float s);
vec4 vec4_div(vec4 v, float d);

float vec3_dot(vec3 v1, vec3 v2);
float vec4_dot(vec4 v1, vec4 v2);
float vec2_cross(vec2 v1, vec2 v2);
vec3 vec3_cross(vec3 v1, vec3 v2);
float vec3_mag(vec3 v);
float vec4_mag(vec4 v);
vec3 vec3_norm(vec3 v);
vec4 vec4_norm(vec4 v);

mat4 mat4_mult(mat4 m1, mat4 m2);
vec4 mat4_vec4_mult(mat4 m, vec4 v);

mat4 mat4_scale(float factor);
mat4 mat4_look_at(vec3 eye, vec3 target, vec3 up);
mat4 mat4_perspective(float aspect, float fov, float near, float far);

void calculate_normals(size_t vertex_count, Vertex *vertices);
int load_teapot(size_t *vertex_count, Vertex **vertices);
char *obj_parse_indices(char *p, int32_t *v_idx, int32_t *vt_idx,
                        int32_t *vn_idx);
int load_obj(const char *file_path, size_t *vertex_count, Vertex **vertices);

void set_pixel(uint32_t *pixels, size_t index, float r, float g, float b,
               float a);
void render(const uint32_t vertex_count, const Vertex *vertices,
            uint32_t *color_buffer, float *depth_buffer, const uint32_t width,
            const uint32_t height);

void write_uint32_t_le(uint8_t *buffer, uint32_t data);
int write_bmp_image(const char *file_name, int32_t width, int32_t height,
                    const uint32_t *pixels);

/* MAIN ===================================================================== */
int main(int argc, char **argv) {
  srand(time(NULL));

  size_t vertex_count;
  Vertex *vertices;

  if (argc == 1) {
    if (load_teapot(&vertex_count, &vertices) != 0) {
      perror("Error loading teapot model: ");
      return EXIT_FAILURE;
    }
  } else if (argc == 2) {
    if (load_obj(argv[1], &vertex_count, &vertices) != 0) {
      fprintf(stderr, "Error loading file %s: %s\n", argv[1], strerror(errno));
      return EXIT_FAILURE;
    }
  } else {
    fprintf(stderr, "Error: Invalid command line arguments");
    return EXIT_FAILURE;
  }

  // center model at origin
  vec3 avg_position = {0};
  for (size_t i = 0; i < vertex_count; i++) {
    avg_position = vec3_add(avg_position, vertices[i].position);
  }
  avg_position = vec3_div(avg_position, (float)vertex_count);

  for (size_t i = 0; i < vertex_count; i++) {
    vertices[i].position = vec3_sub(vertices[i].position, avg_position);
  }

  uint32_t *color_buffer = calloc(WIDTH * HEIGHT, sizeof(*color_buffer));
  if (color_buffer == NULL) {
    free(vertices);
    perror("Error allocating color buffer: ");
    return EXIT_FAILURE;
  }

  float *depth_buffer = malloc(WIDTH * HEIGHT * sizeof(*depth_buffer));
  if (depth_buffer == NULL) {
    free(vertices);
    free(color_buffer);
    perror("Error allocating depth buffer: ");
    return EXIT_FAILURE;
  }

  Vertex test_verts[] = {
      (Vertex){.position = (vec3){0, -2, 0}},
      (Vertex){.position = (vec3){2, 0, 0}},
      (Vertex){.position = (vec3){2, -2, 0}},

      (Vertex){.position = (vec3){0, -2, 0}},
      (Vertex){.position = (vec3){-1, 1, 0}},
      (Vertex){.position = (vec3){2, 0, 0}},
  };

  // render(vertex_count, vertices, color_buffer, depth_buffer, WIDTH, HEIGHT);
  render(6, test_verts, color_buffer, depth_buffer, WIDTH, HEIGHT);

  if (write_bmp_image(OUPUT_FILE_NAME, WIDTH, HEIGHT, color_buffer) != 0) {
    perror("Error writing output image: ");
    free(vertices);
    free(color_buffer);
    free(depth_buffer);
    return EXIT_FAILURE;
  }

  free(vertices);
  free(color_buffer);
  free(depth_buffer);

  return EXIT_SUCCESS;
}

/* FUNCTION IMPLEMENTATIONS ================================================= */
vec3 vec3_add(vec3 v1, vec3 v2) {
  return (vec3){v1.x + v2.x, v1.y + v2.y, v1.z + v2.z};
}

vec2 vec2_sub(vec2 v1, vec2 v2) { return (vec2){v1.x - v2.x, v1.y - v2.y}; }

vec3 vec3_sub(vec3 v1, vec3 v2) {
  return (vec3){v1.x - v2.x, v1.y - v2.y, v1.z - v2.z};
}

vec3 vec3_div(vec3 v, float d) { return (vec3){v.x / d, v.y / d, v.z / d}; }

vec4 vec4_div(vec4 v, float d) {
  return (vec4){v.x / d, v.y / d, v.z / d, v.w / d};
}

float vec3_dot(vec3 v1, vec3 v2) {
  return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

float vec4_dot(vec4 v1, vec4 v2) {
  return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z + v1.w * v2.w;
}

float vec2_cross(vec2 v1, vec2 v2) { return v1.x * v2.y - v1.y * v2.x; }

vec3 vec3_cross(vec3 v1, vec3 v2) {
  return (vec3){
      v1.y * v2.z - v1.z * v2.y,
      v1.z * v2.x - v1.x * v2.z,
      v1.x * v2.y - v1.y * v2.x,
  };
}

float vec3_mag(vec3 v) { return sqrtf(vec3_dot(v, v)); }

float vec4_mag(vec4 v) { return sqrtf(vec4_dot(v, v)); }

vec3 vec3_norm(vec3 v) { return vec3_div(v, vec3_mag(v)); }

vec4 vec4_norm(vec4 v) { return vec4_div(v, vec4_mag(v)); }

mat4 mat4_mult(mat4 m1, mat4 m2) {
  vec4 row0 = {m1.col0.x, m1.col1.x, m1.col2.x, m1.col3.x};
  vec4 row1 = {m1.col0.y, m1.col1.y, m1.col2.y, m1.col3.y};
  vec4 row2 = {m1.col0.z, m1.col1.z, m1.col2.z, m1.col3.z};
  vec4 row3 = {m1.col0.w, m1.col1.w, m1.col2.w, m1.col3.w};

  return (mat4){
      {vec4_dot(row0, m2.col0), vec4_dot(row1, m2.col0),
       vec4_dot(row2, m2.col0), vec4_dot(row3, m2.col0)},
      {vec4_dot(row0, m2.col1), vec4_dot(row1, m2.col1),
       vec4_dot(row2, m2.col1), vec4_dot(row3, m2.col1)},
      {vec4_dot(row0, m2.col2), vec4_dot(row1, m2.col2),
       vec4_dot(row2, m2.col2), vec4_dot(row3, m2.col2)},
      {vec4_dot(row0, m2.col3), vec4_dot(row1, m2.col3),
       vec4_dot(row2, m2.col3), vec4_dot(row3, m2.col3)},
  };
}

vec4 mat4_vec4_mult(mat4 m, vec4 v) {
  vec4 row0 = {m.col0.x, m.col1.x, m.col2.x, m.col3.x};
  vec4 row1 = {m.col0.y, m.col1.y, m.col2.y, m.col3.y};
  vec4 row2 = {m.col0.z, m.col1.z, m.col2.z, m.col3.z};
  vec4 row3 = {m.col0.w, m.col1.w, m.col2.w, m.col3.w};

  return (vec4){vec4_dot(row0, v), vec4_dot(row1, v), vec4_dot(row2, v),
                vec4_dot(row3, v)};
}

mat4 mat4_scale(float factor) {
  return (mat4){
      {factor, 0, 0, 0},
      {0, factor, 0, 0},
      {0, 0, factor, 0},
      {0, 0, 0, 1},
  };
}

mat4 mat4_look_at(vec3 eye, vec3 target, vec3 up) {
  vec3 f = vec3_norm(vec3_sub(target, eye));
  vec3 s = vec3_norm(vec3_cross(up, f));
  vec3 u = vec3_cross(f, s);

  mat4 m;
  m.col0 = (vec4){s.x, s.y, s.z, 0};
  m.col1 = (vec4){u.x, u.y, u.z, 0};
  m.col2 = (vec4){-f.x, -f.y, -f.z, 0};
  m.col3 = (vec4){-vec3_dot(s, eye), -vec3_dot(u, eye), vec3_dot(f, eye), 1};
  return m;
}

mat4 mat4_perspective(float aspect, float fov, float near, float far) {
  return (mat4){
      {1 / (aspect * tanf(fov / 2)), 0, 0, 0},
      {0, -1 / tanf(fov / 2), 0, 0},
      {0, 0, far / (near - far), -1},
      {0, 0, -far * near / (far - near), 0},
  };
}

void calculate_normals(size_t vertex_count, Vertex *vertices) {
  for (size_t i = 0; i < vertex_count; i += 3) {

    vec3 p0 = vertices[i].position;
    vec3 p1 = vertices[i + 1].position;
    vec3 p2 = vertices[i + 2].position;

    vec3 v1 = vec3_sub(p1, p0);
    vec3 v2 = vec3_sub(p2, p0);

    vec3 normal = vec3_norm(vec3_cross(v1, v2));

    vertices[i].normal = normal;
    vertices[i + 1].normal = normal;
    vertices[i + 2].normal = normal;
  }
}

int load_teapot(size_t *vertex_count, Vertex **vertices) {
  *vertices = NULL;
  *vertex_count = 0;

  Vertex *verts = malloc(2 * TEAPOT_INDEX_COUNT * sizeof(*verts));
  if (verts == NULL) return -1;

  for (int i = 0; i < TEAPOT_INDEX_COUNT; i += 3) {
    for (int j = 0; j < 3; j++) {
      int vidx = TEAPOT_INDICES[i + j] * 3;

      Vertex v;
      v.position.x = TEAPOT_VERTICES[vidx + 0];
      v.position.y = TEAPOT_VERTICES[vidx + 1];
      v.position.z = TEAPOT_VERTICES[vidx + 2];

      verts[i + j] = v;

      // The teapot model data only contains half of the teapot, so we
      // have to mirror all of the vertices
      v.position.z *= -1;
      // reverse order of mirrored verts to preserve counter-clockwise
      // winding order
      verts[i + 2 - j + TEAPOT_INDEX_COUNT] = v;
    }
  }

  calculate_normals(2 * TEAPOT_INDEX_COUNT, verts);

  *vertex_count = 2 * TEAPOT_INDEX_COUNT;
  *vertices = verts;

  return 0;
}

char *obj_parse_indices(char *p, int32_t *v_idx, int32_t *vt_idx,
                        int32_t *vn_idx) {
  int32_t idxs[3] = {0};

  int i = 0;
  do {
    idxs[i++] = strtol(p, &p, 10);
    if (*p == '/') p++;
  } while (*p != ' ' && *p != '\r' && *p != '\n' && *p != '\0');

  *v_idx = idxs[0];
  *vt_idx = idxs[1];
  *vn_idx = idxs[2];

  return p;
}

int load_obj(const char *file_path, size_t *vertex_count, Vertex **vertices) {
  *vertices = NULL;
  *vertex_count = 0;

  FILE *fp = fopen(file_path, "rb");
  if (fp == NULL) return -1;

  fseek(fp, 0L, SEEK_END);
  long flen = ftell(fp);
  rewind(fp);

  char *start = malloc(flen + 1);
  if (start == NULL) return -1;

  size_t n = fread(start, sizeof(start[0]), flen, fp);
  start[n] = '\0';
  fclose(fp);

  vec3_da vs = {0};
  vec2_da vts = {0};
  vec3_da vns = {0};
  Vertex_da face_verts = {0};
  Vertex_da mesh_verts = {0};

  char *p = start;
  char *end = start + flen;
  size_t last_print = 0;
  while (p < end) {
    size_t index = p - start;
    if (index - last_print >= 10000) {
      printf("\r%zu/%ld (%.0f%%)", index, flen, 100.0 * index / flen);
      fflush(stdout);
      last_print = index;
    }

    if (p != start && *(p - 1) != '\n') {
      p++;
      continue;
    };

    if (*p == 'f') {
      p++;

      face_verts.size = 0;

      while (*p != '\r' && *p != '\n' && *p != '\0') {
        int32_t v, vt, vn;
        p = obj_parse_indices(p, &v, &vt, &vn);

        // printf("%d/%d/%d\n", v, vt, vn);

        Vertex vert;
        vert.position = vs.items[v - 1];
        if (vt != 0) vert.uv = vts.items[vt - 1];
        if (vn != 0) vert.normal = vns.items[vn - 1];

        DARRAY_APPEND(face_verts, vert);
      }

      for (size_t i = 2; i < face_verts.size; i++) {
        DARRAY_APPEND(mesh_verts, face_verts.items[0]);
        DARRAY_APPEND(mesh_verts, face_verts.items[i - 1]);
        DARRAY_APPEND(mesh_verts, face_verts.items[i]);
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
  printf("\n");

  free(start);
  DARRAY_FREE(vs);
  DARRAY_FREE(vts);
  DARRAY_FREE(vns);

  if (vns.size == 0) {
    calculate_normals(mesh_verts.size, mesh_verts.items);
  }

  *vertices = mesh_verts.items;
  *vertex_count = mesh_verts.size;

  return 0;
}

void set_pixel(uint32_t *pixels, size_t index, float r, float g, float b,
               float a) {
  pixels[index] = ((uint8_t)(a * 255) << 24) | ((uint8_t)(r * 255) << 16) |
                  ((uint8_t)(g * 255) << 8) | (uint8_t)(b * 255);
}

int ivec2_cross(ivec2 a, ivec2 b) { return a.x * b.y - b.x * a.y; }
ivec2 ivec2_sub(ivec2 a, ivec2 b) { return (ivec2){a.x - b.x, a.y - b.y}; }

bool is_top_left(vec2 p0, vec2 p1) {
  vec2 delta = vec2_sub(p1, p0);

  bool is_top = delta.y == 0 && delta.x < 0;
  bool is_left = delta.y > 0;

  return is_top || is_left;
}

mat4 rotate_z(float theta) {
  return (mat4){
      (vec4){cosf(theta), sinf(theta), 0, 0},
      (vec4){-sinf(theta), cosf(theta), 0, 0},
      (vec4){0, 0, 1, 0},
      (vec4){0, 0, 0, 1},
  };
}

float signed_area(vec2 a, vec2 b, vec2 c) {
  return vec2_cross(vec2_sub(c, a), vec2_sub(b, a));
}

void render(const uint32_t vertex_count, const Vertex *vertices,
            uint32_t *color_buffer, float *depth_buffer, const uint32_t width,
            const uint32_t height) {

  for (size_t i = 0; i < width * height; i++) {
    // set_pixel(color_buffer, i, 1, 0, 1, 1);
    set_pixel(color_buffer, i, 0, 0, 0, 1);
    depth_buffer[i] = 1.0f;
  }

  // mat4 model_mat = mat4_scale(1.0f);
  mat4 model_mat = rotate_z(0.2f);

  mat4 view_mat =
      mat4_look_at((vec3){0, 0, 10}, (vec3){0, 0, 0}, (vec3){0, -1, 0});

  mat4 projection_mat =
      mat4_perspective((float)width / height, 3.1415f / 4, 0.1f, 500.0f);

  mat4 transform = mat4_mult(mat4_mult(projection_mat, view_mat), model_mat);

  vec3 sun_direction = {.x = 0, .y = 1, .z = 0};
  sun_direction = vec3_norm(sun_direction);

  for (size_t i = 0; i < vertex_count; i += 3) {
    // if ((i * 3) % 10000 == 0) {
    //   printf("\r%lu/%u (%.0f%%)", i, vertex_count,
    //          100.0f * (float)i / vertex_count);
    //   fflush(stdout);
    // }

    vec2 scr_pos[3];
    float depths[3];
    int shouldClip = 0;

    for (int j = 0; j < 3; j++) {
      vec3 pos = vertices[i + j].position;

      vec4 clip_pos = mat4_vec4_mult(transform, (vec4){pos.x, pos.y, pos.z, 1});

      // printf("% 5.3f % 5.3f % 5.3f % 5.3f\n", clip_pos.x, clip_pos.y,
      //        clip_pos.z, clip_pos.w);

      if (clip_pos.x < -clip_pos.w || clip_pos.x > clip_pos.w ||
          clip_pos.y < -clip_pos.w || clip_pos.y > clip_pos.w ||
          clip_pos.z < 0 || clip_pos.z > clip_pos.w) {

        shouldClip = 1;
        break;
      }

      scr_pos[j] = (vec2){clip_pos.x / clip_pos.w * 0.5 + 0.5,
                          clip_pos.y / clip_pos.w * 0.5 + 0.5};
      depths[j] = clip_pos.z / clip_pos.w;
    }

    // TODO: clip triangles that are partially inside the viewing volume
    if (shouldClip == 1) continue;

    vec2 p0 = (vec2){scr_pos[0].x * width, scr_pos[0].y * height};
    vec2 p1 = (vec2){scr_pos[1].x * width, scr_pos[1].y * height};
    vec2 p2 = (vec2){scr_pos[2].x * width, scr_pos[2].y * height};

    // printf("%d %d | %d %d | %d %d\n", p0.x, p0.y, p1.x, p1.y, p2.x, p2.y);
    printf("%f %f | %f %f | %f %f\n", p0.x, p0.y, p1.x, p1.y, p2.x, p2.y);

    // float area = vec2_cross(vec2_sub(p2, p0), vec2_sub(p1, p0));.
    float area = signed_area(p0, p1, p2);

    if (area <= 0) continue;

    printf("%f\n", area);

    int xmin = floorf(MIN(MIN(p0.x, p1.x), p2.x));
    int ymin = floorf(MIN(MIN(p0.y, p1.y), p2.y));
    int xmax = ceilf(MAX(MAX(p0.x, p1.x), p2.x));
    int ymax = ceilf(MAX(MAX(p0.y, p1.y), p2.y));

    printf("triangle bbox:\n");
    printf("  x: %d %d\n", xmin, xmax);
    printf("  y: %d %d\n", ymin, ymax);

    float b0 = is_top_left(p1, p2) ? 0 : -1;
    float b1 = is_top_left(p2, p0) ? 0 : -1;
    float b2 = is_top_left(p0, p1) ? 0 : -1;

    printf("biases:\n");
    printf("  %f %f %f\n", b0, b1, b2);

    vec3 rand_color = {
        (float)rand() / RAND_MAX,
        (float)rand() / RAND_MAX,
        (float)rand() / RAND_MAX,
    };

    for (int y = ymin; y <= ymax; y++) {
      for (int x = xmin; x <= xmax; x++) {
        vec2 p = {x + 0.5, y + 0.5};
        float w0 = signed_area(p0, p1, p) + b0;
        float w1 = signed_area(p1, p2, p) + b1;
        float w2 = signed_area(p2, p0, p) + b2;

        if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
          // set_pixel(color_buffer, x + y * width, rand_color.x, rand_color.y,
          //           rand_color.z, 1);
          set_pixel(color_buffer, x + y * width, w0 / area, w1 / area,
                    w2 / area, 1);
          // set_pixel(color_buffer, x + y * width, 1, 0, 0, 1);
        }
      }
    }
  }
  printf("\n");
}

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

/* TEAPOT MODEL DATA ========================================================
 */
// Original model data from https://graphics.cs.utah.edu/teapot/
const float TEAPOT_VERTICES[TEAPOT_VERTEX_COUNT] = {
    0.48f, 1.95f, 0.23f, 0.38f, 2.04f, 0.00f, 0.00f, 1.65f, 0.00f, 0.15f, 1.65f,
    0.23f, 0.59f, 1.85f, 0.00f, 0.30f, 1.65f, 0.00f, 1.43f, 2.10f, 0.00f, 1.48f,
    1.99f, 0.23f, 1.33f, 1.87f, 0.00f, 0.27f, 1.01f, 0.00f, 0.37f, 1.10f, 0.23f,
    0.46f, 1.20f, 0.00f, 1.00f, 0.75f, 0.00f, 1.23f, 0.47f, 0.23f, 1.21f, 0.32f,
    0.00f, 6.00f, 2.25f, 0.11f, 5.80f, 2.25f, 0.00f, 5.83f, 2.31f, 0.00f, 6.13f,
    2.32f, 0.15f, 6.20f, 2.25f, 0.00f, 6.43f, 2.33f, 0.00f, 5.70f, 2.25f, 0.00f,
    6.00f, 2.25f, 0.19f, 6.30f, 2.25f, 0.00f, 5.39f, 1.65f, 0.00f, 5.54f, 1.47f,
    0.34f, 5.69f, 1.29f, 0.00f, 4.87f, 1.37f, 0.00f, 4.76f, 0.91f, 0.48f, 4.96f,
    0.75f, 0.11f, 4.97f, 0.68f, 0.00f, 3.00f, 3.00f, 0.00f, 2.67f, 2.83f, 0.00f,
    2.77f, 2.83f, 0.23f, 3.00f, 2.83f, 0.33f, 2.80f, 2.55f, 0.00f, 2.86f, 2.55f,
    0.14f, 3.00f, 2.55f, 0.20f, 3.23f, 2.83f, 0.23f, 3.33f, 2.83f, 0.00f, 3.14f,
    2.55f, 0.14f, 3.20f, 2.55f, 0.00f, 2.17f, 2.40f, 0.00f, 2.42f, 2.40f, 0.58f,
    3.00f, 2.40f, 0.82f, 1.70f, 2.25f, 0.00f, 2.08f, 2.25f, 0.92f, 3.00f, 2.25f,
    1.30f, 3.58f, 2.40f, 0.58f, 3.83f, 2.40f, 0.00f, 3.92f, 2.25f, 0.92f, 4.30f,
    2.25f, 0.00f, 2.01f, 2.25f, 0.99f, 1.60f, 2.25f, 0.00f, 1.60f, 2.35f, 0.00f,
    2.01f, 2.35f, 0.99f, 3.00f, 2.25f, 1.40f, 3.00f, 2.35f, 1.40f, 1.50f, 2.25f,
    0.00f, 1.94f, 2.25f, 1.06f, 3.00f, 2.25f, 1.50f, 3.99f, 2.25f, 0.99f, 3.99f,
    2.35f, 0.99f, 4.40f, 2.25f, 0.00f, 4.40f, 2.35f, 0.00f, 4.06f, 2.25f, 1.06f,
    4.50f, 2.25f, 0.00f, 1.70f, 1.47f, 1.30f, 3.00f, 1.47f, 1.84f, 1.16f, 1.47f,
    0.00f, 1.59f, 0.75f, 1.41f, 3.00f, 0.75f, 2.00f, 4.30f, 1.47f, 1.30f, 4.84f,
    1.47f, 0.00f, 4.41f, 0.75f, 1.41f, 1.76f, 0.23f, 1.24f, 3.00f, 0.23f, 1.75f,
    1.25f, 0.23f, 0.00f, 1.50f, 0.00f, 0.00f, 1.94f, 0.00f, 1.06f, 3.00f, 0.00f,
    1.50f, 4.24f, 0.23f, 1.24f, 4.06f, 0.00f, 1.06f, 4.75f, 0.23f, 0.00f, 4.50f,
    0.00f, 0.00f,
};

const uint8_t TEAPOT_INDICES[TEAPOT_INDEX_COUNT] = {
    0,  1,  2,  0,  2,  3,  4,  0,  3,  4,  3,  5,  6,  0,  7,  6,  1,  0,  8,
    7,  0,  8,  0,  4,  3,  2,  9,  3,  9,  10, 5,  3,  10, 5,  10, 11, 12, 10,
    13, 12, 11, 10, 13, 9,  14, 13, 10, 9,  15, 16, 17, 15, 17, 18, 19, 15, 18,
    19, 18, 20, 18, 17, 21, 18, 21, 22, 20, 18, 22, 20, 22, 23, 22, 21, 24, 22,
    24, 25, 23, 22, 25, 23, 25, 26, 27, 28, 25, 27, 25, 24, 29, 25, 28, 29, 26,
    25, 29, 30, 26, 31, 32, 33, 31, 33, 34, 33, 32, 35, 33, 35, 36, 34, 33, 36,
    34, 36, 37, 31, 34, 38, 31, 38, 39, 38, 34, 37, 38, 37, 40, 39, 38, 40, 39,
    40, 41, 36, 35, 42, 36, 42, 43, 37, 36, 43, 37, 43, 44, 43, 42, 45, 43, 45,
    46, 44, 43, 46, 44, 46, 47, 40, 37, 44, 40, 44, 48, 41, 40, 48, 41, 48, 49,
    48, 44, 47, 48, 47, 50, 49, 48, 50, 49, 50, 51, 52, 53, 54, 52, 54, 55, 56,
    52, 55, 56, 55, 57, 55, 54, 58, 55, 58, 59, 57, 55, 59, 57, 59, 60, 61, 56,
    57, 61, 57, 62, 63, 61, 62, 63, 62, 64, 62, 57, 60, 62, 60, 65, 64, 62, 65,
    64, 65, 66, 60, 59, 67, 60, 67, 68, 67, 69, 12, 67, 12, 70, 68, 67, 70, 68,
    70, 71, 7,  58, 6,  7,  59, 58, 7,  67, 59, 7,  8,  69, 7,  69, 67, 65, 60,
    68, 65, 68, 72, 66, 65, 72, 66, 72, 73, 72, 68, 71, 72, 71, 74, 27, 74, 28,
    27, 72, 74, 27, 73, 72, 28, 74, 29, 71, 70, 75, 71, 75, 76, 75, 77, 78, 75,
    78, 79, 76, 75, 79, 76, 79, 80, 13, 70, 12, 13, 75, 70, 13, 14, 77, 13, 77,
    75, 74, 71, 76, 74, 76, 81, 81, 76, 80, 81, 80, 82, 83, 81, 82, 83, 82, 84,
    29, 83, 30, 29, 81, 83, 29, 74, 81,
};
