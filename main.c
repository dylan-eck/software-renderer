/*
 * Single File Software Renderer
 *
 * This is a very basic software renderer that can load and render obj files.
 */

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* MACROS */
#define DARRAY_DEFINE(type, name)                                              \
    typedef struct {                                                           \
        size_t capacity;                                                       \
        size_t size;                                                           \
        type *items;                                                           \
    } name;

#define DARRAY_APPEND(array, item)                                             \
    do {                                                                       \
        if (array.size >= array.capacity) {                                    \
            if (array.capacity == 0) array.capacity = 256;                     \
            else                                                               \
                array.capacity *= 2;                                           \
            array.items =                                                      \
                realloc(array.items, array.capacity * sizeof(*array.items));   \
        }                                                                      \
        array.items[array.size++] = item;                                      \
    } while (0)

/* TYPE DEFINITIONS */
// clang-format off
typedef struct { float x, y; } vec2;
typedef struct { float x, y, z; } vec3;
typedef struct { float x, y, z, w; } vec4;
typedef struct { vec4 col0, col1, col2, col3; } mat4;
// clang-format on
typedef struct {
    vec3 position;
    float p0;
    vec3 normal;
    float p1;
    vec4 color;
    vec2 uv;
} Vertex;

DARRAY_DEFINE(vec2, vec2_da);
DARRAY_DEFINE(vec3, vec3_da);
DARRAY_DEFINE(Vertex, Vertex_da);

/* FUNCTION DECLARATIONS */
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

mat4 mat4_identity();
mat4 mat4_scale(float factor);
mat4 mat4_look_at(vec3 eye, vec3 center, vec3 up);
mat4 mat4_perspective(float aspect, float fov, float near, float far);

vec3 calc_barycentric_weights(vec2 p, vec2 tri[3]);
int is_inside_triangle(vec2 point, vec2 triangle[3], vec3 *bweights);

int load_obj(const char *file_path, size_t *vertex_count, Vertex **vertices);

void render(const uint32_t vertex_count, const Vertex *vertices,
            uint32_t *colorBuffer, float *depthBuffer, const uint32_t width,
            const uint32_t height);

void write_uint32_t_le(uint8_t *buffer, uint32_t data);
int write_bmp_image(const char *file_name, uint32_t width, uint32_t height,
                    const uint32_t *pixels);

/* MAIN */
int main() {
    uint32_t width = 1920;
    uint32_t height = 1080;

    size_t vertex_count;
    Vertex *vertices;
    load_obj("./test/suzanne_smooth.obj", &vertex_count, &vertices);

    vec3 avg_position = {};
    for (size_t i = 0; i < vertex_count; i++) {
        avg_position = vec3_add(avg_position, vertices[i].position);
    }
    avg_position = vec3_div(avg_position, vertex_count);

    for (size_t i = 0; i < vertex_count; i++) {
        vertices[i].position = vec3_sub(vertices[i].position, avg_position);
    }

    uint32_t *colorBuffer = calloc(width * height, sizeof(*colorBuffer));
    float *depthBuffer = malloc(width * height * sizeof(*depthBuffer));
    render(vertex_count, vertices, colorBuffer, depthBuffer, width, height);

    write_bmp_image("out.bmp", width, height, colorBuffer);

    free(colorBuffer);

    return 0;
}

/* FUNCTION IMPLEMENTATIONS */
vec2 vec2_sub(vec2 v1, vec2 v2) { return (vec2){v1.x - v2.x, v1.y - v2.y}; }

vec3 vec3_add(vec3 v1, vec3 v2) {
    return (vec3){v1.x + v2.x, v1.y + v2.y, v1.z + v2.z};
}

vec3 vec3_div(vec3 v, float d) { return (vec3){v.x / d, v.y / d, v.z / d}; }

vec4 vec4_div(vec4 v, float d) {
    return (vec4){v.x / d, v.y / d, v.z / d, v.w / d};
}

vec3 vec3_sub(vec3 v1, vec3 v2) {
    return (vec3){v1.x - v2.x, v1.y - v2.y, v1.z - v2.z};
}

float vec2_cross(vec2 v1, vec2 v2) { return v1.x * v2.y - v1.y * v2.x; }

vec3 vec3_cross(vec3 v1, vec3 v2) {
    return (vec3){
        v1.y * v2.z - v1.z * v2.y,
        v1.z * v2.x - v1.x * v2.z,
        v1.x * v2.y - v1.y * v2.x,
    };
}

float vec3_dot(vec3 v1, vec3 v2) {
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

float vec4_dot(vec4 v1, vec4 v2) {
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z + v1.w * v2.w;
}

float vec3_mag(vec3 v) { return sqrt(vec3_dot(v, v)); }
float vec4_mag(vec4 v) { return sqrt(vec4_dot(v, v)); }

vec3 vec3_norm(vec3 v) { return vec3_div(v, vec3_mag(v)); }

vec4 vec4_norm(vec4 v) { return vec4_div(v, vec4_mag(v)); }

mat4 mat4_mult(mat4 m1, mat4 m2) {
    vec4 row0 = {m1.col0.x, m1.col1.x, m1.col2.x, m1.col3.x};
    vec4 row1 = {m1.col0.y, m1.col1.y, m1.col2.y, m1.col3.y};
    vec4 row2 = {m1.col0.z, m1.col1.z, m1.col2.z, m1.col3.z};
    vec4 row3 = {m1.col0.w, m1.col1.w, m1.col2.w, m1.col3.w};

    return (mat4){
        {
            vec4_dot(row0, m2.col0),
            vec4_dot(row1, m2.col0),
            vec4_dot(row2, m2.col0),
            vec4_dot(row3, m2.col0),
        },
        {
            vec4_dot(row0, m2.col1),
            vec4_dot(row1, m2.col1),
            vec4_dot(row2, m2.col1),
            vec4_dot(row3, m2.col1),
        },
        {
            vec4_dot(row0, m2.col2),
            vec4_dot(row1, m2.col2),
            vec4_dot(row2, m2.col2),
            vec4_dot(row3, m2.col2),
        },
        {
            vec4_dot(row0, m2.col3),
            vec4_dot(row1, m2.col3),
            vec4_dot(row2, m2.col3),
            vec4_dot(row3, m2.col3),
        },
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

mat4 mat4_identity() {
    return (mat4){{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
}

mat4 mat4_scale(float factor) {
    return (mat4){
        {factor, 0, 0, 0}, {0, factor, 0, 0}, {0, 0, factor, 0}, {0, 0, 0, 1}};
}

mat4 mat4_look_at(vec3 eye, vec3 center, vec3 up) {
    vec3 f = vec3_norm(vec3_sub(center, eye));
    vec3 s = vec3_norm(vec3_cross(f, up));
    vec3 u = vec3_cross(s, f);

    return (mat4){
        {s.x, u.x, -f.x, 0},
        {s.y, u.y, -f.y, 0},
        {s.z, u.z, -f.z, 0},
        {-vec3_dot(s, eye), -vec3_dot(u, eye), vec3_dot(f, eye), 1},
    };
}

mat4 mat4_perspective(float aspect, float fov, float near, float far) {
    return (mat4){
        {1 / (aspect * tanf(fov / 2)), 0, 0, 0},
        {0, 1 / tanf(fov / 2), 0, 0},
        {0, 0, far / (near - far), -1},
        {0, 0, -far * near / (far - near), 0},
    };
}

vec3 calc_barycentric_weights(vec2 p, vec2 tri[3]) {
    vec2 a = tri[0];
    vec2 b = tri[1];
    vec2 c = tri[2];

    float xd = vec2_cross(a, b) + vec2_cross(b, c) + vec2_cross(c, a);
    if (fabsf(xd) < 1e-6f) return (vec3){-1, -1, -1};

    float xa = vec2_cross(b, c) + vec2_cross(p, vec2_sub(b, c));
    float xb = vec2_cross(c, a) + vec2_cross(p, vec2_sub(c, a));
    float xc = vec2_cross(a, b) + vec2_cross(p, vec2_sub(a, b));

    return (vec3){xa / xd, xb / xd, xc / xd};
}

int is_inside_triangle(vec2 point, vec2 triangle[3],
                       vec3 *barycentric_weights) {

    vec3 w = calc_barycentric_weights(point, triangle);

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

int load_obj(const char *file_path, size_t *vertex_count, Vertex **vertices) {
    FILE *f;
    f = fopen(file_path, "r");
    if (f == NULL) {
        printf("failed to open file %s\n", file_path);
        exit(EXIT_FAILURE);
    }

    vec3_da vs = {0};
    vec2_da vts = {0};
    vec3_da vns = {0};
    Vertex_da tris = {0};

    char *line = NULL;
    size_t len = 0;
    ssize_t nread;

    while ((nread = getline(&line, &len, f)) != -1) {
        float x, y, z;
        sscanf(line + 2, "%f %f %f", &x, &y, &z);

        if (line[0] == 'f') {
            char *ptr = strtok(line + 2, " ");

            int vc = 0;
            Vertex verts[4];

            while (ptr != NULL) {
                int32_t nums[3];
                int num_count = 0;
                int slash_count = 0;
                char *p = ptr;

                while (*p && num_count < 3) {
                    if (*p == '\r' || *p == '\n') break;

                    nums[num_count++] = (int32_t)strtol(p, (char **)&p, 10);
                    while (*p == '/') {
                        slash_count++;
                        p++;
                    };
                }

                // TODO: clean this up
                if (num_count == 1) {
                    vec3 v = vs.items[nums[0] - 1];
                    verts[vc++] = (Vertex){.position = v};
                } else if (num_count == 3) { // v, vn, vt
                    vec3 v = vs.items[nums[0] - 1];
                    vec2 vt = vts.items[nums[1] - 1];
                    vec3 vn = vns.items[nums[2] - 1];
                    verts[vc++] =
                        (Vertex){.position = v, .normal = vn, .uv = vt};

                } else if (slash_count == 2) {
                    vec3 v = vs.items[nums[0] - 1];
                    vec3 vn = vns.items[nums[1] - 1];
                    verts[vc++] = (Vertex){.position = v, .normal = vn};
                } else {
                    vec3 v = vs.items[nums[0] - 1];
                    vec2 vt = vts.items[nums[1] - 1];

                    verts[vc++] = (Vertex){.position = v, .uv = vt};
                }

                ptr = strtok(NULL, " \n");
            }

            for (int i = 2; i < vc; i++) {
                DARRAY_APPEND(tris, verts[0]);
                DARRAY_APPEND(tris, verts[i - 1]);
                DARRAY_APPEND(tris, verts[i]);
            }
        } else if (line[0] == 'v') {
            if (line[1] == ' ') {
                DARRAY_APPEND(vs, ((vec3){x, y, z}));
            } else if (line[1] == 'n') {
                DARRAY_APPEND(vns, ((vec3){x, y, z}));
            } else if (line[1] == 't') {
                DARRAY_APPEND(vts, ((vec2){x, y}));
            }
        }
    }

    free(vs.items);
    free(vns.items);
    free(vts.items);

    if (vns.size == 0) {
        for (int i = 0; i < tris.size; i += 3) {
            vec3 v0 = tris.items[i].position;
            vec3 v1 = tris.items[i + 1].position;
            vec3 v2 = tris.items[i + 2].position;

            vec3 normal =
                vec3_norm(vec3_cross(vec3_sub(v1, v0), vec3_sub(v2, v0)));

            tris.items[i].normal = normal;
            tris.items[i + 1].normal = normal;
            tris.items[i + 2].normal = normal;
        }
    }

    *vertices = tris.items;
    *vertex_count = tris.size;

    free(line);
    fclose(f);

    return 1;
}

void write_uint32_t_le(uint8_t *buffer, uint32_t data) {
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

    FILE *f = fopen(file_name, "wb");
    if (!f) return 1;

    fwrite(header, sizeof(uint8_t), header_size, f);
    fwrite(pixels, sizeof(uint32_t), width * height, f);

    fclose(f);

    return 0;
}

void set_pixel(uint32_t *pixels, size_t index, float r, float g, float b,
               float a) {
    pixels[index] = ((uint8_t)(a * 255) << 24) | ((uint8_t)(r * 255) << 16) |
                    ((uint8_t)(g * 255) << 8) | (uint8_t)(b * 255);
}

void render(const uint32_t vertex_count, const Vertex *vertices,
            uint32_t *colorBuffer, float *depthBuffer, const uint32_t width,
            const uint32_t height) {

    for (size_t i = 0; i < width * height; i++) {
        depthBuffer[i] = 1.0f;
    }

    mat4 model_mat = mat4_mult(mat4_scale(1.0), mat4_identity());

    mat4 view_mat =
        mat4_look_at((vec3){0, 0, 5}, (vec3){0, 0, 0}, (vec3){0, -1, 0});

    mat4 projection_mat =
        mat4_perspective((float)width / height, 3.1415 / 4, 0.1f, 500.0f);

    mat4 transform = mat4_mult(mat4_mult(projection_mat, view_mat), model_mat);

    vec2 pixel_size = {.x = 2.0f / width, .y = 2.0f / height};

    vec3 sun_direction = {.x = 0, .y = 1, .z = 0};
    sun_direction = vec3_norm(sun_direction);

    for (int i = 0; i < vertex_count; i += 3) {
        vec2 scr_pos[3];
        float depths[3];
        int shouldClip = 0;

        for (int j = 0; j < 3; j++) {
            vec3 pos = vertices[i + j].position;

            vec4 clip_pos =
                mat4_vec4_mult(transform, (vec4){pos.x, pos.y, pos.z, 1});

            if (clip_pos.x < -clip_pos.w || clip_pos.x > clip_pos.w ||
                clip_pos.y < -clip_pos.w || clip_pos.y > clip_pos.w ||
                clip_pos.z < -clip_pos.w || clip_pos.x > clip_pos.w) {

                shouldClip = 1;
                break;
            }

            scr_pos[j] =
                (vec2){clip_pos.x / clip_pos.w, clip_pos.y / clip_pos.w};
            depths[j] = clip_pos.z / clip_pos.w;
        }

        // TODO: clip triangles that are partially inside the viewing volume
        if (shouldClip == 1) continue;

        float backface = vec2_cross(vec2_sub(scr_pos[1], scr_pos[0]),
                                    vec2_sub(scr_pos[2], scr_pos[0]));
        if (backface < 0) continue;

        // TODO: clean this mess up
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

        int x_start_px = floor(((scr_x_min + 1) / 2) * width);
        if (x_start_px < 0) x_start_px = 0;

        int x_end_px = floor(((scr_x_max + 1) / 2) * width);
        if (x_end_px >= width) x_end_px = width - 1;

        int y_start_px = floor(((scr_y_min + 1) / 2) * height);
        if (y_start_px < 0) y_start_px = 0;

        int y_end_px = floor(((scr_y_max + 1) / 2) * height);
        if (y_end_px >= height) y_end_px = height - 1;

        // TODO: We are checking pixels here that we maybe don't need to check
        for (int ypx = y_start_px; ypx < y_end_px; ypx++) {
            for (int xpx = x_start_px; xpx < x_end_px; xpx++) {
                if (xpx < 0 || xpx > width || ypx < 0 || ypx > height) {
                    continue;
                }

                vec2 scr_pt;

                scr_pt.x = -1.0f + (xpx + 0.5f) * pixel_size.x;
                scr_pt.y = -1.0f + (ypx + 0.5f) * pixel_size.y;

                uint32_t idx = xpx + ypx * width;

                vec3 weights;
                if (!is_inside_triangle(scr_pt, scr_pos, &weights)) continue;

                float depth = weights.x * depths[0] + weights.y * depths[1] +
                              weights.z * depths[2];

                if (depth > depthBuffer[idx]) continue;
                depthBuffer[idx] = depth;

                float cutoff = 0.02;
                if (weights.x < cutoff || weights.y < cutoff ||
                    weights.z < cutoff) {

                    set_pixel(colorBuffer, idx, 1, 0, 0, 1);
                    continue;
                }

                float nx = weights.x * vertices[i + 0].normal.x +
                           weights.y * vertices[i + 1].normal.x +
                           weights.z * vertices[i + 2].normal.x;

                float ny = weights.x * vertices[i + 0].normal.y +
                           weights.y * vertices[i + 1].normal.y +
                           weights.z * vertices[i + 2].normal.y;

                float nz = weights.x * vertices[i + 0].normal.z +
                           weights.y * vertices[i + 1].normal.z +
                           weights.z * vertices[i + 2].normal.z;

                vec4 world_normal = vec4_norm(mat4_vec4_mult(
                    model_mat, (vec4){.x = nx, .y = ny, .z = nz, .w = 0}));

                float intensity = vec3_dot((vec3){.x = world_normal.x,
                                                  .y = world_normal.y,
                                                  .z = world_normal.z},
                                           sun_direction);

                intensity = fmax(intensity, 0);
                intensity = fmin(intensity + 0.1, 1);

                set_pixel(colorBuffer, idx, intensity, intensity, intensity, 1);
            }
        }
    }
}

const Vertex demoModelVerts[3] = {
    {.position = {0, 0, 0}},
    {.position = {1, 0, 0}},
    {.position = {0, 1, 0}},
};
