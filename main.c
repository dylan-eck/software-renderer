#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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
    float p0;
    float3 normal;
    float p1;
    float4 color;
    float2 uv;
} Vertex;

typedef struct {
    size_t capacity;
    size_t size;
    float2 *items;
} float2_da;

typedef struct {
    size_t capacity;
    size_t size;
    float3 *items;
} float3_da;

typedef struct {
    size_t capacity;
    size_t size;
    Vertex *items;
} Vertex_da;

extern const Vertex demoModelVerts[3];

static inline void float2_darray_append(float2_da *da, float2 item);
static inline void float3_darray_append(float3_da *da, float3 item);
static inline void Vertex_darray_append(Vertex_da *da, Vertex item0);

float2 float2_sub(float2 v1, float2 v2);
float3 float3_sub(float3 v1, float3 v2);

float float2_cross(float2 v1, float2 v2);
float3 float3_cross(float3 v1, float3 v2);

float float3_dot(float3 v1, float3 v2);
float float4_dot(float4 v1, float4 v2);

float float3_mag(float3 v) { return sqrt(float3_dot(v, v)); }

float3 float3_div(float3 v, float s);

float3 float3_norm(float3 v);

float4x4 float4x4_mat_mult(float4x4 m1, float4x4 m2);
float4 float4x4_vec_mult(float4x4 m, float4 v);

float4x4 float4x4_identity();
float4x4 float4x4_look_at(float3 eye, float3 center, float3 up);
float4x4 float4x4_perspective(float aspect, float fov, float near, float far);

float3 calc_barycentric_weights(float2 p, float2 tri[3]);
int is_inside_triangle(
    float2 point, float2 triangle[3], float3 *barycentric_weights);

void write_uint32_t(uint8_t *buffer, uint32_t data);

int load_obj(const char *file_path, size_t *vertex_count, Vertex **vertices);

int write_bmp_image(
    const char *file_name,
    uint32_t width,
    uint32_t height,
    const uint32_t *pixels);

void render(
    const uint32_t vertex_count,
    const Vertex *vertices,
    uint32_t *buffer,
    const uint32_t width,
    const uint32_t height);

int main() {
    srand(time(NULL));

    uint32_t width = 1920;
    uint32_t height = 1080;

    size_t vertex_count;
    Vertex *vertices;
    load_obj("./utah_teapot.obj", &vertex_count, &vertices);

    printf("vertex count: %lu\n", vertex_count);

    uint32_t *pixels = malloc(width * height * sizeof(*pixels));
    render(vertex_count, vertices, pixels, width, height);

    write_bmp_image("out.bmp", width, height, pixels);

    free(pixels);

    return 0;
}

static inline void float2_darray_append(float2_da *da, float2 item) {
    if (da->size >= da->capacity) {
        da->capacity = da->capacity ? da->capacity * 2 : 64;
        da->items = realloc(da->items, da->capacity * sizeof *da->items);
    }
    da->items[da->size++] = item;
}

static inline void float3_darray_append(float3_da *da, float3 item) {
    if (da->size >= da->capacity) {
        da->capacity = da->capacity ? da->capacity * 2 : 64;
        da->items = realloc(da->items, da->capacity * sizeof *da->items);
    }
    da->items[da->size++] = item;
}

static inline void Vertex_darray_append(Vertex_da *da, Vertex item) {
    if (da->size >= da->capacity) {
        da->capacity = da->capacity ? da->capacity * 2 : 64;
        da->items = realloc(da->items, da->capacity * sizeof *da->items);
    }
    da->items[da->size++] = item;
}

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

    return (float4){float4_dot(row0, v),
                    float4_dot(row1, v),
                    float4_dot(row2, v),
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
        return (float3){-1, -1, -1};
    }

    float xa = float2_cross(b, c) + float2_cross(p, float2_sub(b, c));
    float xb = float2_cross(c, a) + float2_cross(p, float2_sub(c, a));
    float xc = float2_cross(a, b) + float2_cross(p, float2_sub(a, b));

    return (float3){xa / xd, xb / xd, xc / xd};
}

int is_inside_triangle(
    float2 point, float2 triangle[3], float3 *barycentric_weights) {

    float3 w = calc_barycentric_weights(point, triangle);

    const float eps = 1e-6f;
    if (w.x >= -eps && w.y >= -eps && w.z >= -eps && w.x <= 1 + eps &&
        w.y <= 1 + eps && w.z <= 1 + eps) {

        if (barycentric_weights != NULL) {
            barycentric_weights->x = w.x;
            barycentric_weights->y = w.y;
            barycentric_weights->z = w.z;
        }
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

int load_obj(const char *file_path, size_t *vertex_count, Vertex **vertices) {
    FILE *f;
    f = fopen(file_path, "r");
    if (f == NULL) {
        printf("failed to open file %s\n", file_path);
        exit(EXIT_FAILURE);
    }

    float3_da vs = {0};
    float3_da vns = {0};
    float2_da vts = {0};
    Vertex_da tris = {0};

    char *line = NULL;
    size_t len = 0;
    ssize_t nread;

    while ((nread = getline(&line, &len, f)) != -1) {
        float x, y, z;
        sscanf(line + 2, "%f %f %f", &x, &y, &z);

        if (line[0] == 'f') {
            char *ptr = strtok(line + 2, " ");
            int i = 0;
            int vidxs[4] = {INT_MAX, INT_MAX, INT_MAX, INT_MAX};

            while (ptr != NULL) {
                vidxs[i] = atoi(ptr) - 1;
                i++;
                ptr = strtok(NULL, " ");
            }

            for (int i = 2; i < 4; i++) {
                if (vidxs[i] == INT_MAX) break;

                float4 color = {
                    .x = (float)rand() / RAND_MAX,
                    .y = (float)rand() / RAND_MAX,
                    .z = (float)rand() / RAND_MAX,
                    .w = 1.0f,
                };

                size_t v0idx = vidxs[0];
                size_t v1idx = vidxs[i - 1];
                size_t v2idx = vidxs[i];

                Vertex_darray_append(
                    &tris,
                    (Vertex){
                        .position = vs.items[v0idx],
                        .color = color,
                    });

                Vertex_darray_append(
                    &tris,
                    (Vertex){
                        .position = vs.items[v1idx],
                        .color = color,
                    });

                Vertex_darray_append(
                    &tris,
                    (Vertex){
                        .position = vs.items[v2idx],
                        .color = color,
                    });
            }
        } else if (line[0] == 'v') {
            if (line[1] == ' ') {
                float3_darray_append(&vs, (float3){x, y, z});
            } else if (line[1] == 'n') {
                float3_darray_append(&vns, (float3){x, y, z});
            } else if (line[1] == 't') {
                float2_darray_append(&vts, (float2){x, y});
            }
        }
    }

    // printf("loaded obj file: %s\n", file_path);
    // printf("       v  count: %lu\n", vs.size);
    // printf("       vn count: %lu\n", vns.size);
    // printf("       vt count: %lu\n", vts.size);
    // printf("   vertex count: %lu\n", tris.size);

    free(vs.items);
    free(vns.items);
    free(vts.items);

    *vertices = tris.items;
    *vertex_count = tris.size;

    free(line);
    fclose(f);

    return 1;
}

int write_bmp_image(
    const char *file_name,
    uint32_t width,
    uint32_t height,
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

    // DIB header
    write_uint32_t(&header[18], width);
    write_uint32_t(&header[22], height);
    header[26] = 0x01; // number of color planes (always 1)
    header[28] = 0x20; // bits per pixel
    write_uint32_t(&header[34], pixel_data_size);

    FILE *f = fopen(file_name, "wb");
    if (!f) return 1;

    fwrite(header, sizeof(uint8_t), header_size, f);
    fwrite(pixels, sizeof(uint32_t), width * height, f);

    fclose(f);

    return 0;
}

void render(
    const uint32_t vertex_count,
    const Vertex *vertices,
    uint32_t *buffer,
    const uint32_t width,
    const uint32_t height) {

    float4x4 model_mat = float4x4_identity();

    float4x4 view_mat = float4x4_look_at(
        (float3){4, 4, 4}, (float3){0, 0, 0}, (float3){0, 1, 0});

    float4x4 projection_mat =
        float4x4_perspective((float)width / height, 3.1415 / 4, 0.1f, 100.0f);

    float4x4 transform = float4x4_mat_mult(
        float4x4_mat_mult(projection_mat, view_mat), model_mat);

    memset(buffer, 0, width * height * sizeof(*buffer));

    float2 pixel_size = {.x = 2.0f / width, .y = 2.0f / height};

    for (int i = 0; i < vertex_count; i += 3) {

        float4 clip_pos[3];
        float3 ndc_pos[3];
        float2 scr_pos[3];
        for (int j = 0; j < 3; j++) {
            float3 pos = vertices[i + j].position;

            // TODO: clip tris outside the viewing volume;
            clip_pos[j] =
                float4x4_vec_mult(transform, (float4){pos.x, pos.y, pos.z, 1});

            ndc_pos[j] = (float3){clip_pos[j].x / clip_pos[j].w,
                                  clip_pos[j].y / clip_pos[j].w,
                                  clip_pos[j].z / clip_pos[j].w};

            scr_pos[j] = (float2){ndc_pos[j].x, -ndc_pos[j].y};
        }

        float backface = float2_cross(
            float2_sub(scr_pos[1], scr_pos[0]),
            float2_sub(scr_pos[2], scr_pos[0]));

        if (backface < 0) continue;

        float scr_x_min = FLT_MAX;
        float scr_x_max = FLT_MIN;
        float scr_y_min = FLT_MAX;
        float scr_y_max = FLT_MIN;

        for (int j = 0; j < 3; j++) {
            if (scr_pos[j].x < scr_x_min) scr_x_min = scr_pos[j].x;
            if (scr_pos[j].x > scr_x_max) scr_x_max = scr_pos[j].x;
            if (scr_pos[j].y < scr_y_min) scr_y_min = scr_pos[j].y;
            if (scr_pos[j].y > scr_y_max) scr_y_max = scr_pos[j].y;
        }

        float3 color = (float3){
            .x = (float)rand() / RAND_MAX,
            .y = (float)rand() / RAND_MAX,
            .z = (float)rand() / RAND_MAX,
        };

        int x_start_px = floor(((scr_x_min + 1) / 2) * width);
        int x_end_px = floor(((scr_x_max + 1) / 2) * width);
        int y_start_px = floor(((scr_y_min + 1) / 2) * height);
        int y_end_px = floor(((scr_y_max + 1) / 2) * height);

        // TODO: We are checking pixels here that we maybe don't need to check
        for (int ypx = y_start_px; ypx < y_end_px; ypx++) {
            for (int xpx = x_start_px; xpx < x_end_px; xpx++) {
                float2 scr_pt;

                scr_pt.x = -1.0f + (xpx + 0.5f) * pixel_size.x;
                scr_pt.y = -1.0f + (ypx + 0.5f) * pixel_size.y;

                uint32_t idx = xpx + ypx * width;

                float3 weights;
                if (!is_inside_triangle(scr_pt, scr_pos, &weights)) continue;

                uint8_t r = color.x * 255;
                uint8_t g = color.y * 255;
                uint8_t b = color.z * 255;
                uint8_t a = 255;

                buffer[idx] = (a << 24) | (r << 16) | (g << 8) | (b);
            }
        }
    }
}

const Vertex demoModelVerts[3] = {
    {.position = {0, 0, 0}},
    {.position = {1, 0, 0}},
    {.position = {0, 1, 0}},
};
