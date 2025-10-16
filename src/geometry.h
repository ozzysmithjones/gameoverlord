#ifndef GEOMETRY_H
#define GEOMETRY_H
#include <stdbool.h>
#include <math.h>
#include <stdalign.h>
#include <immintrin.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    float x, y;
} vector2;

static inline vector2 vector2_add(vector2 a, vector2 b) {
    return (vector2) {
        a.x + b.x, a.y + b.y
    };
}

static inline vector2 vector2_sub(vector2 a, vector2 b) {
    return (vector2) {
        a.x - b.x, a.y - b.y
    };
}

static inline vector2 vector2_scale(vector2 v, float s) {
    return (vector2) {
        v.x* s, v.y* s
    };
}

static inline float vector2_dot(vector2 a, vector2 b) {
    return a.x * b.x + a.y * b.y;
}

static inline float vector2_length_squared(vector2 v) {
    return vector2_dot(v, v);
}

static inline float vector2_length(vector2 v) {
    return sqrtf(v.x * v.x + v.y * v.y);
}

static inline vector2 vector2_reflect(vector2 v, vector2 normal) {
    float dot = vector2_dot(v, normal);
    return vector2_sub(v, vector2_scale(normal, 2.0f * dot));
}

static inline vector2 vector2_normalize(vector2 v) {
    float len = vector2_length(v);
    if (len == 0.0f) {
        return v; // Cannot normalize zero vector
    }
    return (vector2) {
        v.x / len, v.y / len
    };
}

static inline vector2 vector2_lerp(vector2 a, vector2 b, float t) {
    return (vector2) {
        a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t
    };
}

static inline vector2 vector2_cross(vector2 a, vector2 b) {
    return (vector2) {
        a.y* b.x - a.x * b.y, a.x* b.y - a.y * b.x
    };
}

typedef struct {
    float x, y, z;
} vector3;

static inline vector3 vector3_add(vector3 a, vector3 b) {
    return (vector3) {
        a.x + b.x, a.y + b.y, a.z + b.z
    };
}

static inline vector3 vector3_sub(vector3 a, vector3 b) {
    return (vector3) {
        a.x - b.x, a.y - b.y, a.z - b.z
    };
}

static inline vector3 vector3_scale(vector3 v, float s) {
    return (vector3) {
        v.x* s, v.y* s, v.z* s
    };
}

static inline float vector3_dot(vector3 a, vector3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline float vector3_length_squared(vector3 v) {
    return vector3_dot(v, v);
}

static inline float vector3_length(vector3 v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static inline vector3 vector3_normalize(vector3 v) {
    float len = vector3_length(v);
    if (len == 0.0f) {
        return v; // Cannot normalize zero vector
    }
    return (vector3) {
        v.x / len, v.y / len, v.z / len
    };
}

static inline vector3 vector3_cross(vector3 a, vector3 b) {
    return (vector3) {
        a.y* b.z - a.z * b.y,
            a.z* b.x - a.x * b.z,
            a.x* b.y - a.y * b.x
    };
}

static inline vector3 vector3_lerp(vector3 a, vector3 b, float t) {
    return (vector3) {
        a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t
    };
}

typedef struct {
    alignas(16) float m[4][4];
} matrix;

static inline matrix identity_matrix(void) {
    matrix result = { 0 };
    result.m[0][0] = 1.0f;
    result.m[1][1] = 1.0f;
    result.m[2][2] = 1.0f;
    result.m[3][3] = 1.0f;
    return result;
}

static matrix view_matrix(vector3 eye, vector3 target, vector3 up) {
    vector3 zaxis = vector3_normalize(vector3_sub(target, eye));    // The "forward" vector.
    vector3 xaxis = vector3_normalize(vector3_cross(up, zaxis)); // The "right" vector.
    vector3 yaxis = vector3_cross(zaxis, xaxis);     // The "up" vector.

    matrix view = { 0 };
    view.m[0][0] = xaxis.x;
    view.m[1][0] = xaxis.y;
    view.m[2][0] = xaxis.z;
    view.m[3][0] = -vector3_dot(xaxis, eye);

    view.m[0][1] = yaxis.x;
    view.m[1][1] = yaxis.y;
    view.m[2][1] = yaxis.z;
    view.m[3][1] = -vector3_dot(yaxis, eye);

    view.m[0][2] = -zaxis.x;
    view.m[1][2] = -zaxis.y;
    view.m[2][2] = -zaxis.z;
    view.m[3][2] = vector3_dot(zaxis, eye);

    view.m[0][3] = 0.0f;
    view.m[1][3] = 0.0f;
    view.m[2][3] = 0.0f;
    view.m[3][3] = 1.0f;

    return view;
}

static inline matrix translation_matrix(float tx, float ty, float tz) {
    matrix result = identity_matrix();
    result.m[3][0] = tx;
    result.m[3][1] = ty;
    result.m[3][2] = tz;
    return result;
}

static inline matrix scaling_matrix(float sx, float sy, float sz) {
    matrix result = { 0 };
    result.m[0][0] = sx;
    result.m[1][1] = sy;
    result.m[2][2] = sz;
    result.m[3][3] = 1.0f;
    return result;
}

static inline matrix rotation_x_matrix(float angle_rad) {
    matrix result = identity_matrix();
    float c = cosf(angle_rad);
    float s = sinf(angle_rad);
    result.m[1][1] = c;
    result.m[1][2] = s;
    result.m[2][1] = -s;
    result.m[2][2] = c;
    return result;
}

static inline matrix rotation_y_matrix(float angle_rad) {
    matrix result = identity_matrix();
    float c = cosf(angle_rad);
    float s = sinf(angle_rad);
    result.m[0][0] = c;
    result.m[0][2] = -s;
    result.m[2][0] = s;
    result.m[2][2] = c;
    return result;
}

static inline matrix rotation_z_matrix(float angle_rad) {
    matrix result = identity_matrix();
    float c = cosf(angle_rad);
    float s = sinf(angle_rad);
    result.m[0][0] = c;
    result.m[0][1] = s;
    result.m[1][0] = -s;
    result.m[1][1] = c;
    return result;
}

static inline matrix perspective_matrix(float fov_y, float aspect, float near_plane, float far_plane) {
    float f = 1.0f / tanf(fov_y / 2.0f);
    matrix result = { 0 };
    result.m[0][0] = f / aspect;
    result.m[1][1] = f;
    result.m[2][2] = (far_plane + near_plane) / (near_plane - far_plane);
    result.m[2][3] = -1.0f;
    result.m[3][2] = (2.0f * far_plane * near_plane) / (near_plane - far_plane);
    return result;
}

static inline matrix orthographic_matrix(float left, float right, float bottom, float top, float near_plane, float far_plane) {
    matrix result = { 0 };
    float width = right - left;
    float height = top - bottom;
    float depth = far_plane - near_plane;

    // Map x from [left, right] to [-1, 1]
    result.m[0][0] = 2.0f / width;
    result.m[3][0] = -(right + left) / width;

    // Map y from [bottom, top] to [-1, 1] (flipped for DirectX)
    result.m[1][1] = 2.0f / height;
    result.m[3][1] = -(top + bottom) / height;

    // Map z from [near, far] to [0, 1]
    result.m[2][2] = -2.0f / depth;
    result.m[3][2] = -(far_plane + near_plane) / depth;

    result.m[3][3] = 1.0f;
    return result;
}

static inline matrix matrix_transpose(matrix m) {
    matrix result = { 0 };

    // fast transpose using SIMD
    __m128 row1 = _mm_load_ps(&m.m[0][0]);
    __m128 row2 = _mm_load_ps(&m.m[1][0]);
    __m128 row3 = _mm_load_ps(&m.m[2][0]);
    __m128 row4 = _mm_load_ps(&m.m[3][0]);
    _MM_TRANSPOSE4_PS(row1, row2, row3, row4);
    _mm_store_ps(&result.m[0][0], row1);
    _mm_store_ps(&result.m[1][0], row2);
    _mm_store_ps(&result.m[2][0], row3);
    _mm_store_ps(&result.m[3][0], row4);

    return result;
}

static inline matrix matrix_multiply(matrix a, matrix b) {
    matrix return_value = { 0 };

    // Fast matrix multiplication using SIMD
    // Assuming that b is transposed for better cache performance
    __m128 b_row0 = _mm_load_ps(&b.m[0][0]);
    __m128 b_row1 = _mm_load_ps(&b.m[1][0]);
    __m128 b_row2 = _mm_load_ps(&b.m[2][0]);
    __m128 b_row3 = _mm_load_ps(&b.m[3][0]);
    for (int i = 0; i < 4; ++i) {
        __m128 a_elem = _mm_set1_ps(a.m[i][0]);
        __m128 res = _mm_mul_ps(a_elem, b_row0);

        a_elem = _mm_set1_ps(a.m[i][1]);
        res = _mm_add_ps(res, _mm_mul_ps(a_elem, b_row1));

        a_elem = _mm_set1_ps(a.m[i][2]);
        res = _mm_add_ps(res, _mm_mul_ps(a_elem, b_row2));

        a_elem = _mm_set1_ps(a.m[i][3]);
        res = _mm_add_ps(res, _mm_mul_ps(a_elem, b_row3));

        _mm_store_ps(&return_value.m[i][0], res);
    }

    return return_value;
}

typedef struct {
    vector2 min;
    vector2 max;
} rect;

static inline float rect_area(rect r) {
    vector2 size = vector2_sub(r.max, r.min);
    return size.x * size.y;
}

static inline rect rect_union(rect a, rect b) {
    return (rect) {
        .min = {
            fminf(a.min.x, b.min.x),
            fminf(a.min.y, b.min.y)
        },
            .max = {
                fmaxf(a.max.x, b.max.x),
                fmaxf(a.max.y, b.max.y)
        }
    };
}

static inline rect rect_intersection(rect a, rect b) {
    return (rect) {
        .min = {
            fmaxf(a.min.x, b.min.x),
            fmaxf(a.min.y, b.min.y)
        },
            .max = {
                fminf(a.max.x, b.max.x),
                fminf(a.max.y, b.max.y)
        }
    };
}

static inline bool rect_overlaps_rect(rect a, rect b) {
    return a.min.x < b.max.x && a.max.x > b.min.x &&
        a.min.y < b.max.y && a.max.y > b.min.y;
}

static inline int rect_contains_point(rect r, vector2 point) {
    return point.x >= r.min.x && point.x <= r.max.x &&
        point.y >= r.min.y && point.y <= r.max.y;
}

typedef struct {
    vector2 center;
    float radius;
} circle;

static inline float circle_area(circle c) {
    return (float)(M_PI * c.radius * c.radius);
}

static inline bool circle_overlaps_circle(circle a, circle b) {
    vector2 diff = vector2_sub(a.center, b.center);
    float dist_sq = vector2_length_squared(diff);
    float radius_sum = a.radius + b.radius;
    return dist_sq < (radius_sum * radius_sum);
}

static inline bool circle_overlaps_rect(circle c, rect r) {
    float closest_x = fmaxf(r.min.x, fminf(c.center.x, r.max.x));
    float closest_y = fmaxf(r.min.y, fminf(c.center.y, r.max.y));
    vector2 closest_point = { closest_x, closest_y };
    vector2 diff = vector2_sub(c.center, closest_point);
    float dist_sq = vector2_length_squared(diff);
    return dist_sq < (c.radius * c.radius);
}

static inline bool rect_overlaps_circle(rect r, circle c) {
    return circle_overlaps_rect(c, r);
}

static inline bool circle_contains_point(circle c, vector2 point) {
    vector2 diff = vector2_sub(c.center, point);
    float dist_sq = vector2_length_squared(diff);
    return dist_sq < (c.radius * c.radius);
}

#endif // GEOMETRY_H