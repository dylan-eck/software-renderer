#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

const uint32_t WIDTH = 1920;
const uint32_t HEIGHT = 1080;

typedef struct {
  float x;
  float y;
} float2;

typedef struct {
  float x;
  float y;
  float z;
} float3;

typedef struct {
  float x;
  float y;
  float z;
  float w;
} float4;

typedef struct {
  float4 col0;
  float4 col1;
  float4 col2;
  float4 col3;
} float4x4;

typedef struct {
  float3 position;
  float4 color;
  float2 uv;
} Vertex;

float2 float2_sub(float2 v1, float2 v2) {
  return (float2){v1.x - v2.x, v1.y - v2.y};
}

float3 float3_sub(float3 v1, float3 v2) {
  return (float3){v1.x - v2.x, v1.y - v2.y, v1.z - v2.z};
}

float float2_cross(float2 v1, float2 v2) { return v1.x * v2.y - v1.y * v2.x; }

float3 float3_cross(float3 v1, float3 v2) {
  return (float3){
      v1.y * v2.z - v1.z * v2.y,
      v1.z * v2.x - v1.x * v2.z,
      v1.x * v2.y - v1.y * v2.x,
  };
}

float float3_dot(float3 v1, float3 v2) {
  return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

float float4_dot(float4 v1, float4 v2) {
  return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z + v1.w * v2.w;
}

float float3_mag(float3 v) { return sqrt(float3_dot(v, v)); }

float3 float3_div(float3 v, float s) {
  return (float3){v.x / s, v.y / s, v.z / s};
}

float3 float3_norm(float3 v) { return float3_div(v, float3_mag(v)); }

float4x4 float4x4_mat_mult(float4x4 m1, float4x4 m2) {
  float4 row0 = {m1.col0.x, m1.col1.x, m1.col2.x, m1.col3.x};
  float4 row1 = {m1.col0.y, m1.col1.y, m1.col2.y, m1.col3.y};
  float4 row2 = {m1.col0.z, m1.col1.z, m1.col2.z, m1.col3.z};
  float4 row3 = {m1.col0.w, m1.col1.w, m1.col2.w, m1.col3.w};

  return (float4x4){
      {
          float4_dot(row0, m2.col0),
          float4_dot(row1, m2.col0),
          float4_dot(row2, m2.col0),
          float4_dot(row3, m2.col0),
      },
      {
          float4_dot(row0, m2.col1),
          float4_dot(row1, m2.col1),
          float4_dot(row2, m2.col1),
          float4_dot(row3, m2.col1),
      },
      {
          float4_dot(row0, m2.col2),
          float4_dot(row1, m2.col2),
          float4_dot(row2, m2.col2),
          float4_dot(row3, m2.col2),
      },
      {
          float4_dot(row0, m2.col3),
          float4_dot(row1, m2.col3),
          float4_dot(row2, m2.col3),
          float4_dot(row3, m2.col3),
      },
  };
}

float4 float4x4_vec_mult(float4x4 m, float4 v) {
  float4 row0 = {m.col0.x, m.col1.x, m.col2.x, m.col3.x};
  float4 row1 = {m.col0.y, m.col1.y, m.col2.y, m.col3.y};
  float4 row2 = {m.col0.z, m.col1.z, m.col2.z, m.col3.z};
  float4 row3 = {m.col0.w, m.col1.w, m.col2.w, m.col3.w};

  return (float4){float4_dot(row0, v), float4_dot(row1, v), float4_dot(row2, v),
                  float4_dot(row3, v)};
}

float4x4 float4x4_identity() {
  return (float4x4){
      {1, 0, 0, 0},
      {0, 1, 0, 0},
      {0, 0, 1, 0},
      {0, 0, 0, 1},
  };
}

float4x4 float4x4_look_at(float3 pos, float3 target, float3 up) {
  float3 view_forward = float3_norm(float3_sub(target, pos));
  float3 view_right = float3_norm(float3_cross(view_forward, up));
  float3 view_up = float3_cross(view_forward, view_right);

  float d1 = float3_dot(view_right, pos);
  float d2 = float3_dot(view_up, pos);
  float d3 = float3_dot(view_forward, pos);

  return (float4x4){
      {view_right.x, view_right.y, view_right.z, 0},
      {view_up.x, view_up.y, view_up.z, 0},
      {view_forward.x, view_forward.y, view_forward.z, 0},
      {-d1, -d2, -d3, 1},
  };
}

float4x4 float4x4_perspective(float aspect, float fov, float near, float far) {
  return (float4x4){
      {1 / (aspect * tan(fov / 2)), 0, 0, 0},
      {0, 1 / tan(fov / 2), 0, 0},
      {0, 0, far / (far - near), 1},
      {0, 0, -far * near / (far - near), 0},
  };
}

float3 barycentric_weights(float2 p, float2 tri[3]) {
  float2 a = tri[0];
  float2 b = tri[1];
  float2 c = tri[2];

  float xd = float2_cross(a, b) + float2_cross(b, c) + float2_cross(c, a);

  if (fabsf(xd) < 1e-6f) {
    return (float3){0, 0, 0};
  }

  float xa = float2_cross(b, c) + float2_cross(p, float2_sub(b, c));
  float xb = float2_cross(c, a) + float2_cross(p, float2_sub(c, a));
  float xc = float2_cross(a, b) + float2_cross(p, float2_sub(a, b));

  return (float3){xa / xd, xb / xd, xc / xd};
}

int is_inside_triangle(float2 p, float2 tri[3]) {
  float3 w = barycentric_weights(p, tri);
  const float eps = 1e-6f;
  return w.x >= -eps && w.y >= -eps && w.z >= -eps && w.x <= 1 + eps &&
         w.y <= 1 + eps && w.z <= 1 + eps;
}

int write_bmp_image(uint32_t width, uint32_t height, const uint32_t *pixels) {
  static uint32_t header_size = 54;
  uint32_t file_size = header_size + width * height * sizeof(uint32_t);
  uint32_t pixel_data_size = width * height * sizeof(uint32_t);

  char header[header_size];
  memset(header, 0, header_size);

  // BMP identifier
  header[0] = 0x42;
  header[1] = 0x4d;

  header[2] = file_size & 0x000000ff;
  header[3] = (file_size & 0x0000ff00) >> 8;
  header[4] = (file_size & 0x00ff0000) >> 16;
  header[6] = (file_size & 0xff000000) >> 24;

  header[10] = 0x36; // offset to start of pixel data
  header[14] = 0x28; // size of DIB header

  header[18] = width & 0x000000ff;
  header[19] = (width & 0x0000ff00) >> 8;
  header[20] = (width & 0x00ff0000) >> 16;
  header[21] = (width & 0xff000000) >> 24;

  header[22] = height & 0x000000ff;
  header[23] = (height & 0x0000ff00) >> 8;
  header[24] = (height & 0x00ff0000) >> 16;
  header[25] = (height & 0xff000000) >> 24;

  header[26] = 0x01; // number of color planes (always 1)
  header[28] = 0x20; // bits per pixel

  header[34] = pixel_data_size & 0x000000ff;
  header[35] = (pixel_data_size & 0x0000ff00) >> 8;
  header[36] = (pixel_data_size & 0x00ff0000) >> 16;
  header[37] = (pixel_data_size & 0xff000000) >> 24;

  FILE *f = fopen("out.bmp", "wb");
  if (!f)
    return 1;

  fwrite(header, sizeof(char), header_size, f);
  fwrite(pixels, sizeof(uint32_t), WIDTH * HEIGHT, f);

  fclose(f);

  return 0;
}

int main() {
  srand(time(NULL));

  float4x4 model_mat = float4x4_identity();
  float4x4 view_mat = float4x4_look_at((float3){5, -5, -5}, (float3){0, 0, 0},
                                       (float3){0, -1, 0});
  float4x4 projection_mat =
      float4x4_perspective((float)WIDTH / HEIGHT, 3.1415 / 4, 0.1f, 100.0f);

  float4x4 transform =
      float4x4_mat_mult(float4x4_mat_mult(projection_mat, view_mat), model_mat);

  uint32_t vertex_count = 36;
  float3 verts[] = {
      {-0.5, -0.5, 0.5},  {0.5, -0.5, 0.5},   {0.5, 0.5, 0.5},
      {-0.5, -0.5, 0.5},  {0.5, 0.5, 0.5},    {-0.5, 0.5, 0.5},
      {0.5, -0.5, -0.5},  {-0.5, -0.5, -0.5}, {-0.5, 0.5, -0.5},
      {0.5, -0.5, -0.5},  {-0.5, 0.5, -0.5},  {0.5, 0.5, -0.5},
      {0.5, -0.5, 0.5},   {0.5, -0.5, -0.5},  {0.5, 0.5, -0.5},
      {0.5, -0.5, 0.5},   {0.5, 0.5, -0.5},   {0.5, 0.5, 0.5},
      {-0.5, -0.5, -0.5}, {-0.5, -0.5, 0.5},  {-0.5, 0.5, 0.5},
      {-0.5, -0.5, -0.5}, {-0.5, 0.5, 0.5},   {-0.5, 0.5, -0.5},
      {-0.5, 0.5, 0.5},   {0.5, 0.5, 0.5},    {0.5, 0.5, -0.5},
      {-0.5, 0.5, 0.5},   {0.5, 0.5, -0.5},   {-0.5, 0.5, -0.5},
      {-0.5, -0.5, -0.5}, {0.5, -0.5, -0.5},  {0.5, -0.5, 0.5},
      {-0.5, -0.5, -0.5}, {0.5, -0.5, 0.5},   {-0.5, -0.5, 0.5},
  };

  // uint32_t vertex_count = 3;
  // float3 verts[] = {{0.0, -0.5, 0.0}, {0.5, 0.5, 0.0}, {-0.5, 0.5, 0.0}};

  uint32_t *pixels = malloc(WIDTH * HEIGHT * sizeof(*pixels));

  for (int i = 0; i < WIDTH * HEIGHT; i++) {
    int rowIndex = i / WIDTH;
    int colIndex = i % WIDTH;

    float2 screen_uv = {
        (float)colIndex / WIDTH * 2 - 1,
        (1 - (float)rowIndex / HEIGHT) * 2 - 1,
    };

    pixels[i] = 0;
    for (uint32_t j = 0; j < vertex_count; j += 3) {
      float4 clip_positions[] = {
          float4x4_vec_mult(transform,
                            (float4){
                                verts[j].x,
                                verts[j].y,
                                verts[j].z,
                                1.0f,
                            }),
          float4x4_vec_mult(transform,
                            (float4){
                                verts[j + 1].x,
                                verts[j + 1].y,
                                verts[j + 1].z,
                                1.0f,
                            }),
          float4x4_vec_mult(transform,
                            (float4){
                                verts[j + 2].x,
                                verts[j + 2].y,
                                verts[j + 2].z,
                                1.0f,
                            }),
      };

      float2 screen_coords[] = {
          {
              clip_positions[0].x / clip_positions[0].w,
              clip_positions[0].y / clip_positions[0].w,
          },
          {
              clip_positions[1].x / clip_positions[1].w,
              clip_positions[1].y / clip_positions[1].w,
          },
          {
              clip_positions[2].x / clip_positions[2].w,
              clip_positions[2].y / clip_positions[2].w,
          },
      };

      if (is_inside_triangle(screen_uv, screen_coords)) {
        pixels[i] = 0x00ffffff;
        break;
      }
    }
  }

  write_bmp_image(WIDTH, HEIGHT, pixels);

  free(pixels);

  return 0;
}