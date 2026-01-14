#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

const uint32_t WIDTH = 1920;
const uint32_t HEIGHT = 1080;

typedef struct {
  float x, y;
} float2;

typedef struct {
  float x, y, z;
} float3;

typedef struct {
  float x, y, z, w;
} float4;

typedef struct {
  float4 col0, col1, col2, col3;
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

float4x4 float4x4_look_at(float3 eye, float3 center, float3 up) {
  float3 f = float3_norm(float3_sub(center, eye));
  float3 s = float3_norm(float3_cross(f, up));
  float3 u = float3_cross(s, f);

  return (float4x4){
      {s.x, u.x, -f.x, 0},
      {s.y, u.y, -f.y, 0},
      {s.z, u.z, -f.z, 0},
      {-float3_dot(s, eye), -float3_dot(u, eye), float3_dot(f, eye), 1},
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

float3 calc_barycentric_weights(float2 p, float2 tri[3]) {
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

int is_inside_triangle(float2 point, float2 triangle[3],
                       float3 *barycentric_weights) {
  float3 w = calc_barycentric_weights(point, triangle);

  const float eps = 1e-6f;
  if (w.x >= -eps && w.y >= -eps && w.z >= -eps && w.x <= 1 + eps &&
      w.y <= 1 + eps && w.z <= 1 + eps) {
    barycentric_weights->x = w.x;
    barycentric_weights->y = w.y;
    barycentric_weights->z = w.z;
    return 1;
  } else {
    return 0;
  }
}

void write_uint32_t(uint8_t *buffer, uint32_t data) {
  buffer[0] = data & 0xff;
  buffer[1] = (data >> 8) & 0xff;
  buffer[2] = (data >> 16) & 0xff;
  buffer[3] = (data >> 24) & 0xff;
}

int write_bmp_image(const char *file_name, uint32_t width, uint32_t height,
                    const uint32_t *pixels) {
  static uint32_t header_size = 54;
  uint32_t file_size = header_size + width * height * sizeof(uint32_t);
  uint32_t pixel_data_size = width * height * sizeof(uint32_t);

  uint8_t header[header_size];
  memset(header, 0, header_size);

  // BMP file identifier
  header[0] = 0x42;
  header[1] = 0x4d;

  write_uint32_t(&header[2], file_size);

  header[10] = 0x36; // offset to start of pixel data
  header[14] = 0x28; // size of DIB header

  write_uint32_t(&header[18], width);
  write_uint32_t(&header[22], height);

  header[26] = 0x01; // number of color planes (always 1)
  header[28] = 0x20; // bits per pixel

  write_uint32_t(&header[34], pixel_data_size);

  FILE *f = fopen(file_name, "wb");
  if (!f)
    return 1;

  fwrite(header, sizeof(uint8_t), header_size, f);
  fwrite(pixels, sizeof(uint32_t), WIDTH * HEIGHT, f);

  fclose(f);

  return 0;
}

int main() {
  srand(time(NULL));

  float4x4 model_mat = float4x4_identity();
  float4x4 view_mat =
      float4x4_look_at((float3){4, 4, 4}, (float3){0, 0, 0}, (float3){0, 1, 0});
  float4x4 projection_mat =
      float4x4_perspective((float)WIDTH / HEIGHT, 3.1415 / 4, 0.1f, 100.0f);

  float4x4 transform =
      float4x4_mat_mult(float4x4_mat_mult(projection_mat, view_mat), model_mat);

  uint32_t vertex_count = 36;
  Vertex verts[36] = {
      // Front face
      {.position = {-0.5f, -0.5f, 0.5f}, .color = {1, 0, 0, 1}, .uv = {0, 0}},
      {.position = {0.5f, -0.5f, 0.5f}, .color = {1, 0, 0, 1}, .uv = {1, 0}},
      {.position = {0.5f, 0.5f, 0.5f}, .color = {1, 0, 0, 1}, .uv = {1, 1}},

      {.position = {-0.5f, -0.5f, 0.5f}, .color = {1, 0, 0, 1}, .uv = {0, 0}},
      {.position = {0.5f, 0.5f, 0.5f}, .color = {1, 0, 0, 1}, .uv = {1, 1}},
      {.position = {-0.5f, 0.5f, 0.5f}, .color = {1, 0, 0, 1}, .uv = {0, 1}},

      // Back face
      {.position = {0.5f, -0.5f, -0.5f}, .color = {0, 1, 0, 1}, .uv = {0, 0}},
      {.position = {-0.5f, -0.5f, -0.5f}, .color = {0, 1, 0, 1}, .uv = {1, 0}},
      {.position = {-0.5f, 0.5f, -0.5f}, .color = {0, 1, 0, 1}, .uv = {1, 1}},

      {.position = {0.5f, -0.5f, -0.5f}, .color = {0, 1, 0, 1}, .uv = {0, 0}},
      {.position = {-0.5f, 0.5f, -0.5f}, .color = {0, 1, 0, 1}, .uv = {1, 1}},
      {.position = {0.5f, 0.5f, -0.5f}, .color = {0, 1, 0, 1}, .uv = {0, 1}},

      // Right face
      {.position = {0.5f, -0.5f, 0.5f}, .color = {0, 0, 1, 1}, .uv = {0, 0}},
      {.position = {0.5f, -0.5f, -0.5f}, .color = {0, 0, 1, 1}, .uv = {1, 0}},
      {.position = {0.5f, 0.5f, -0.5f}, .color = {0, 0, 1, 1}, .uv = {1, 1}},

      {.position = {0.5f, -0.5f, 0.5f}, .color = {0, 0, 1, 1}, .uv = {0, 0}},
      {.position = {0.5f, 0.5f, -0.5f}, .color = {0, 0, 1, 1}, .uv = {1, 1}},
      {.position = {0.5f, 0.5f, 0.5f}, .color = {0, 0, 1, 1}, .uv = {0, 1}},

      // Left face
      {.position = {-0.5f, -0.5f, -0.5f}, .color = {1, 1, 0, 1}, .uv = {0, 0}},
      {.position = {-0.5f, -0.5f, 0.5f}, .color = {1, 1, 0, 1}, .uv = {1, 0}},
      {.position = {-0.5f, 0.5f, 0.5f}, .color = {1, 1, 0, 1}, .uv = {1, 1}},

      {.position = {-0.5f, -0.5f, -0.5f}, .color = {1, 1, 0, 1}, .uv = {0, 0}},
      {.position = {-0.5f, 0.5f, 0.5f}, .color = {1, 1, 0, 1}, .uv = {1, 1}},
      {.position = {-0.5f, 0.5f, -0.5f}, .color = {1, 1, 0, 1}, .uv = {0, 1}},

      // Top face
      {.position = {-0.5f, 0.5f, 0.5f}, .color = {1, 0, 1, 1}, .uv = {0, 0}},
      {.position = {0.5f, 0.5f, 0.5f}, .color = {1, 0, 1, 1}, .uv = {1, 0}},
      {.position = {0.5f, 0.5f, -0.5f}, .color = {1, 0, 1, 1}, .uv = {1, 1}},

      {.position = {-0.5f, 0.5f, 0.5f}, .color = {1, 0, 1, 1}, .uv = {0, 0}},
      {.position = {0.5f, 0.5f, -0.5f}, .color = {1, 0, 1, 1}, .uv = {1, 1}},
      {.position = {-0.5f, 0.5f, -0.5f}, .color = {1, 0, 1, 1}, .uv = {0, 1}},

      // Bottom face
      {.position = {-0.5f, -0.5f, -0.5f}, .color = {1, 1, 1, 1}, .uv = {0, 0}},
      {.position = {0.5f, -0.5f, -0.5f}, .color = {1, 1, 1, 1}, .uv = {1, 0}},
      {.position = {0.5f, -0.5f, 0.5f}, .color = {1, 1, 1, 1}, .uv = {1, 1}},

      {.position = {-0.5f, -0.5f, -0.5f}, .color = {1, 1, 1, 1}, .uv = {0, 0}},
      {.position = {0.5f, -0.5f, 0.5f}, .color = {1, 1, 1, 1}, .uv = {1, 1}},
      {.position = {-0.5f, -0.5f, 0.5f}, .color = {1, 1, 1, 1}, .uv = {0, 1}},
  };

  uint32_t *pixels = malloc(WIDTH * HEIGHT * sizeof(*pixels));
  memset(pixels, 0, WIDTH * HEIGHT * sizeof(*pixels));

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
                                verts[j].position.x,
                                verts[j].position.y,
                                verts[j].position.z,
                                1.0f,
                            }),
          float4x4_vec_mult(transform,
                            (float4){
                                verts[j + 1].position.x,
                                verts[j + 1].position.y,
                                verts[j + 1].position.z,
                                1.0f,
                            }),
          float4x4_vec_mult(transform,
                            (float4){
                                verts[j + 2].position.x,
                                verts[j + 2].position.y,
                                verts[j + 2].position.z,
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

      float backface =
          float2_cross(float2_sub(screen_coords[1], screen_coords[0]),
                       float2_sub(screen_coords[2], screen_coords[0]));

      if (backface < 0)
        continue;

      float3 weights;
      if (is_inside_triangle(screen_uv, screen_coords, &weights)) {
        // uint8_t r = weights.x * 255;
        // uint8_t g = weights.y * 255;
        // uint8_t b = weights.z * 255;
        // uint8_t a = 255;

        uint8_t r = verts[j].color.x * 255;
        uint8_t g = verts[j].color.y * 255;
        uint8_t b = verts[j].color.z * 255;
        uint8_t a = verts[j].color.w * 255;

        pixels[i] = (a << 24) | (r << 16) | (g << 8) | (b);
        break;
      }
    }
  }

  write_bmp_image("out.bmp", WIDTH, HEIGHT, pixels);

  free(pixels);

  return 0;
}