// Auto-generated geometry code

#ifndef GEOMETRY_MATH_H
#define GEOMETRY_MATH_H

#include <stdint.h>
#include <math.h>
#include <immintrin.h>
#include <stdalign.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
	float x;
	float y;
} xyf;

static inline xyf xyf_add(xyf a, xyf b) {
	xyf result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	return result;
}

static inline xyf xyf_sub(xyf a, xyf b) {
	xyf result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	return result;
}

static inline xyf xyf_mul(xyf a, xyf b) {
	xyf result;
	result.x = a.x * b.x;
	result.y = a.y * b.y;
	return result;
}

static inline xyf xyf_div(xyf a, xyf b) {
	xyf result;
	result.x = a.x / b.x;
	result.y = a.y / b.y;
	return result;
}

static inline float xyf_dot(xyf a, xyf b) {
	float result = 0;
	result += a.x * b.x;
	result += a.y * b.y;
	return result;
}

static inline double xyf_length(xyf v) {
	return sqrt(xyf_dot(v, v));
}

static inline xyf xyf_normalize(xyf v) {
	float length = (float)xyf_length(v);
	if (length == 0) return v; // Avoid division by zero
	xyf result;
	result.x = v.x / length;
	result.y = v.y / length;
	return result;
}

static inline float xyf_sqr_length(xyf v) {
	return xyf_dot(v, v);
}

typedef struct {
	double x;
	double y;
} xyd;

static inline xyd xyd_add(xyd a, xyd b) {
	xyd result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	return result;
}

static inline xyd xyd_sub(xyd a, xyd b) {
	xyd result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	return result;
}

static inline xyd xyd_mul(xyd a, xyd b) {
	xyd result;
	result.x = a.x * b.x;
	result.y = a.y * b.y;
	return result;
}

static inline xyd xyd_div(xyd a, xyd b) {
	xyd result;
	result.x = a.x / b.x;
	result.y = a.y / b.y;
	return result;
}

static inline double xyd_dot(xyd a, xyd b) {
	double result = 0;
	result += a.x * b.x;
	result += a.y * b.y;
	return result;
}

static inline double xyd_length(xyd v) {
	return sqrt(xyd_dot(v, v));
}

static inline xyd xyd_normalize(xyd v) {
	double length = xyd_length(v);
	if (length == 0) return v; // Avoid division by zero
	xyd result;
	result.x = v.x / length;
	result.y = v.y / length;
	return result;
}

static inline double xyd_sqr_length(xyd v) {
	return xyd_dot(v, v);
}

typedef struct {
	int32_t x;
	int32_t y;
} xy;

static inline xy xy_add(xy a, xy b) {
	xy result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	return result;
}

static inline xy xy_sub(xy a, xy b) {
	xy result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	return result;
}

static inline xy xy_mul(xy a, xy b) {
	xy result;
	result.x = a.x * b.x;
	result.y = a.y * b.y;
	return result;
}

static inline xy xy_div(xy a, xy b) {
	xy result;
	result.x = a.x / b.x;
	result.y = a.y / b.y;
	return result;
}

static inline int32_t xy_dot(xy a, xy b) {
	int32_t result = 0;
	result += a.x * b.x;
	result += a.y * b.y;
	return result;
}

static inline double xy_length(xy v) {
	return sqrt(xy_dot(v, v));
}

static inline int32_t xy_sqr_length(xy v) {
	return xy_dot(v, v);
}

typedef struct {
	float x;
	float y;
	float z;
} xyzf;

static inline xyzf xyzf_add(xyzf a, xyzf b) {
	xyzf result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	result.z = a.z + b.z;
	return result;
}

static inline xyzf xyzf_sub(xyzf a, xyzf b) {
	xyzf result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	result.z = a.z - b.z;
	return result;
}

static inline xyzf xyzf_mul(xyzf a, xyzf b) {
	xyzf result;
	result.x = a.x * b.x;
	result.y = a.y * b.y;
	result.z = a.z * b.z;
	return result;
}

static inline xyzf xyzf_div(xyzf a, xyzf b) {
	xyzf result;
	result.x = a.x / b.x;
	result.y = a.y / b.y;
	result.z = a.z / b.z;
	return result;
}

static inline float xyzf_dot(xyzf a, xyzf b) {
	float result = 0;
	result += a.x * b.x;
	result += a.y * b.y;
	result += a.z * b.z;
	return result;
}

static inline double xyzf_length(xyzf v) {
	return sqrt(xyzf_dot(v, v));
}

static inline xyzf xyzf_normalize(xyzf v) {
	float length = (float)xyzf_length(v);
	if (length == 0) return v; // Avoid division by zero
	xyzf result;
	result.x = v.x / length;
	result.y = v.y / length;
	result.z = v.z / length;
	return result;
}

static inline float xyzf_sqr_length(xyzf v) {
	return xyzf_dot(v, v);
}

typedef struct {
	double x;
	double y;
	double z;
} xyzd;

static inline xyzd xyzd_add(xyzd a, xyzd b) {
	xyzd result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	result.z = a.z + b.z;
	return result;
}

static inline xyzd xyzd_sub(xyzd a, xyzd b) {
	xyzd result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	result.z = a.z - b.z;
	return result;
}

static inline xyzd xyzd_mul(xyzd a, xyzd b) {
	xyzd result;
	result.x = a.x * b.x;
	result.y = a.y * b.y;
	result.z = a.z * b.z;
	return result;
}

static inline xyzd xyzd_div(xyzd a, xyzd b) {
	xyzd result;
	result.x = a.x / b.x;
	result.y = a.y / b.y;
	result.z = a.z / b.z;
	return result;
}

static inline double xyzd_dot(xyzd a, xyzd b) {
	double result = 0;
	result += a.x * b.x;
	result += a.y * b.y;
	result += a.z * b.z;
	return result;
}

static inline double xyzd_length(xyzd v) {
	return sqrt(xyzd_dot(v, v));
}

static inline xyzd xyzd_normalize(xyzd v) {
	double length = xyzd_length(v);
	if (length == 0) return v; // Avoid division by zero
	xyzd result;
	result.x = v.x / length;
	result.y = v.y / length;
	result.z = v.z / length;
	return result;
}

static inline double xyzd_sqr_length(xyzd v) {
	return xyzd_dot(v, v);
}

typedef struct {
	int32_t x;
	int32_t y;
	int32_t z;
} xyz;

static inline xyz xyz_add(xyz a, xyz b) {
	xyz result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	result.z = a.z + b.z;
	return result;
}

static inline xyz xyz_sub(xyz a, xyz b) {
	xyz result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	result.z = a.z - b.z;
	return result;
}

static inline xyz xyz_mul(xyz a, xyz b) {
	xyz result;
	result.x = a.x * b.x;
	result.y = a.y * b.y;
	result.z = a.z * b.z;
	return result;
}

static inline xyz xyz_div(xyz a, xyz b) {
	xyz result;
	result.x = a.x / b.x;
	result.y = a.y / b.y;
	result.z = a.z / b.z;
	return result;
}

static inline int32_t xyz_dot(xyz a, xyz b) {
	int32_t result = 0;
	result += a.x * b.x;
	result += a.y * b.y;
	result += a.z * b.z;
	return result;
}

static inline double xyz_length(xyz v) {
	return sqrt(xyz_dot(v, v));
}

static inline int32_t xyz_sqr_length(xyz v) {
	return xyz_dot(v, v);
}

typedef struct {
	float x;
	float y;
	float z;
	float w;
} xyzwf;

static inline xyzwf xyzwf_add(xyzwf a, xyzwf b) {
	xyzwf result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	result.z = a.z + b.z;
	result.w = a.w + b.w;
	return result;
}

static inline xyzwf xyzwf_sub(xyzwf a, xyzwf b) {
	xyzwf result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	result.z = a.z - b.z;
	result.w = a.w - b.w;
	return result;
}

static inline xyzwf xyzwf_mul(xyzwf a, xyzwf b) {
	xyzwf result;
	result.x = a.x * b.x;
	result.y = a.y * b.y;
	result.z = a.z * b.z;
	result.w = a.w * b.w;
	return result;
}

static inline xyzwf xyzwf_div(xyzwf a, xyzwf b) {
	xyzwf result;
	result.x = a.x / b.x;
	result.y = a.y / b.y;
	result.z = a.z / b.z;
	result.w = a.w / b.w;
	return result;
}

static inline float xyzwf_dot(xyzwf a, xyzwf b) {
	float result = 0;
	result += a.x * b.x;
	result += a.y * b.y;
	result += a.z * b.z;
	result += a.w * b.w;
	return result;
}

static inline double xyzwf_length(xyzwf v) {
	return sqrt(xyzwf_dot(v, v));
}

static inline xyzwf xyzwf_normalize(xyzwf v) {
	double length = xyzwf_length(v);
	if (length == 0) return v; // Avoid division by zero
	xyzwf result;
	result.x = v.x / length;
	result.y = v.y / length;
	result.z = v.z / length;
	result.w = v.w / length;
	return result;
}

static inline float xyzwf_sqr_length(xyzwf v) {
	return xyzwf_dot(v, v);
}

typedef struct {
	double x;
	double y;
	double z;
	double w;
} xyzwd;

static inline xyzwd xyzwd_add(xyzwd a, xyzwd b) {
	xyzwd result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	result.z = a.z + b.z;
	result.w = a.w + b.w;
	return result;
}

static inline xyzwd xyzwd_sub(xyzwd a, xyzwd b) {
	xyzwd result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	result.z = a.z - b.z;
	result.w = a.w - b.w;
	return result;
}

static inline xyzwd xyzwd_mul(xyzwd a, xyzwd b) {
	xyzwd result;
	result.x = a.x * b.x;
	result.y = a.y * b.y;
	result.z = a.z * b.z;
	result.w = a.w * b.w;
	return result;
}

static inline xyzwd xyzwd_div(xyzwd a, xyzwd b) {
	xyzwd result;
	result.x = a.x / b.x;
	result.y = a.y / b.y;
	result.z = a.z / b.z;
	result.w = a.w / b.w;
	return result;
}

static inline double xyzwd_dot(xyzwd a, xyzwd b) {
	double result = 0;
	result += a.x * b.x;
	result += a.y * b.y;
	result += a.z * b.z;
	result += a.w * b.w;
	return result;
}

static inline double xyzwd_length(xyzwd v) {
	return sqrt(xyzwd_dot(v, v));
}

static inline xyzwd xyzwd_normalize(xyzwd v) {
	double length = xyzwd_length(v);
	if (length == 0) return v; // Avoid division by zero
	xyzwd result;
	result.x = v.x / length;
	result.y = v.y / length;
	result.z = v.z / length;
	result.w = v.w / length;
	return result;
}

static inline double xyzwd_sqr_length(xyzwd v) {
	return xyzwd_dot(v, v);
}

typedef struct {
	int32_t x;
	int32_t y;
	int32_t z;
	int32_t w;
} xyzw;

static inline xyzw xyzw_add(xyzw a, xyzw b) {
	xyzw result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	result.z = a.z + b.z;
	result.w = a.w + b.w;
	return result;
}

static inline xyzw xyzw_sub(xyzw a, xyzw b) {
	xyzw result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	result.z = a.z - b.z;
	result.w = a.w - b.w;
	return result;
}

static inline xyzw xyzw_mul(xyzw a, xyzw b) {
	xyzw result;
	result.x = a.x * b.x;
	result.y = a.y * b.y;
	result.z = a.z * b.z;
	result.w = a.w * b.w;
	return result;
}

static inline xyzw xyzw_div(xyzw a, xyzw b) {
	xyzw result;
	result.x = a.x / b.x;
	result.y = a.y / b.y;
	result.z = a.z / b.z;
	result.w = a.w / b.w;
	return result;
}

static inline int32_t xyzw_dot(xyzw a, xyzw b) {
	int32_t result = 0;
	result += a.x * b.x;
	result += a.y * b.y;
	result += a.z * b.z;
	result += a.w * b.w;
	return result;
}

static inline double xyzw_length(xyzw v) {
	return sqrt(xyzw_dot(v, v));
}

static inline int32_t xyzw_sqr_length(xyzw v) {
	return xyzw_dot(v, v);
}

typedef struct {
	alignas(16) float m[4][4];
} matrix;

static inline matrix matrix_identity(void) {
	matrix result = { 0 };
	for (int i = 0; i < 4; ++i) {
		result.m[i][i] = 1;
	}
	return result;
}
static inline matrix matrix_transpose(matrix m) {
	matrix result;

	// Load rows from source matrix
	__m128 row0 = _mm_load_ps(m.m[0]);
	__m128 row1 = _mm_load_ps(m.m[1]);
	__m128 row2 = _mm_load_ps(m.m[2]);
	__m128 row3 = _mm_load_ps(m.m[3]);

	// Transpose using _MM_TRANSPOSE4_PS macro
	_MM_TRANSPOSE4_PS(row0, row1, row2, row3);

	// Store the transposed rows to result
	_mm_store_ps(result.m[0], row0);
	_mm_store_ps(result.m[1], row1);
	_mm_store_ps(result.m[2], row2);
	_mm_store_ps(result.m[3], row3);

	return result;
}

/// @brief Assumes b is transposed.
/// @brief Multiplies two 4x4 matrices using SIMD for performance.
/// @param a Matrix a
/// @param b Matrix b (transposed)
/// @return Resulting matrix after multiplication
static inline matrix matrix_mul(matrix a, matrix b) {
	matrix result = { 0 };

	// Use SIMD to compute matrix multiplication
	// Assuming b is transposed (b.m[j][i] is the transpose of b)
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			// Load row from first matrix
			__m128 row = _mm_load_ps(a.m[i]);
			// Load row from second matrix (which is column in original)
			__m128 col = _mm_load_ps(b.m[j]);
			// Multiply and add all elements in one operation
			__m128 prod = _mm_mul_ps(row, col);
			// Horizontal add to get sum of all four multiplied pairs
			__m128 sum1 = _mm_hadd_ps(prod, prod);
			__m128 sum2 = _mm_hadd_ps(sum1, sum1);
			// Store the result
			result.m[i][j] = _mm_cvtss_f32(sum2);
		}
	}

	return result;
}

static inline matrix matrix_scale(float sx, float sy, float sz) {
	matrix result = matrix_identity();
	result.m[0][0] = sx;
	result.m[1][1] = sy;
	result.m[2][2] = sz;
	return result;
}

static inline matrix matrix_translate(float tx, float ty, float tz) {
	matrix result = matrix_identity();
	result.m[0][3] = tx;
	result.m[1][3] = ty;
	result.m[2][3] = tz;
	return result;
}

static inline matrix matrix_rotate_x(float angle) {
	matrix result = matrix_identity();
	float c = cos(angle);
	float s = sin(angle);
	result.m[1][1] = c;
	result.m[1][2] = -s;
	result.m[2][1] = s;
	result.m[2][2] = c;
	return result;
}

static inline xyzf matrix_get_translation(matrix m) {
	return (xyzf) {
		m.m[0][3],
			m.m[1][3],
			m.m[2][3]
	};
}

static inline xyzf matrix_get_scale(matrix m) {
	return (xyzf) {
		sqrt(m.m[0][0] * m.m[0][0] + m.m[0][1] * m.m[0][1] + m.m[0][2] * m.m[0][2]),
			sqrt(m.m[1][0] * m.m[1][0] + m.m[1][1] * m.m[1][1] + m.m[1][2] * m.m[1][2]),
			sqrt(m.m[2][0] * m.m[2][0] + m.m[2][1] * m.m[2][1] + m.m[2][2] * m.m[2][2])
	};
}

static inline xyzf matrix_get_rotation(matrix m) {
	// Assuming the matrix is orthogonal, we can extract the rotation angles
	xyzf rotation;
	rotation.x = atan2(m.m[2][1], m.m[2][2]); // Roll
	rotation.y = atan2(-m.m[2][0], sqrt(m.m[2][1] * m.m[2][1] + m.m[2][2] * m.m[2][2])); // Pitch
	rotation.z = atan2(m.m[1][0], m.m[0][0]); // Yaw
	return rotation;
}

static inline xyzf matrix_get_up(matrix m) {
	return (xyzf) {
		m.m[1][0],
			m.m[1][1],
			m.m[1][2]
	};
}

static inline xyzf matrix_get_right(matrix m) {
	return (xyzf) {
		m.m[0][0],
			m.m[0][1],
			m.m[0][2]
	};
}

static inline xyzf matrix_get_forward(matrix m) {
	return (xyzf) {
		-m.m[2][0],
			-m.m[2][1],
			-m.m[2][2]
	};
}

static inline matrix matrix_rotate_y(float angle) {
	matrix result = matrix_identity();
	float c = cos(angle);
	float s = sin(angle);
	result.m[0][0] = c;
	result.m[0][2] = s;
	result.m[2][0] = -s;
	result.m[2][2] = c;
	return result;
}

static inline matrix matrix_rotate_z(float angle) {
	matrix result = matrix_identity();
	float c = cos(angle);
	float s = sin(angle);
	result.m[0][0] = c;
	result.m[0][1] = -s;
	result.m[1][0] = s;
	result.m[1][1] = c;
	return result;
}

static inline matrix matrix_rotate_axis(float angle, float x, float y, float z) {
	matrix result = matrix_identity();
	float c = cos(angle);
	float s = sin(angle);
	float t = 1 - c;

	result.m[0][0] = t * x * x + c;
	result.m[0][1] = t * x * y - s * z;
	result.m[0][2] = t * x * z + s * y;

	result.m[1][0] = t * y * x + s * z;
	result.m[1][1] = t * y * y + c;
	result.m[1][2] = t * y * z - s * x;

	result.m[2][0] = t * z * x - s * y;
	result.m[2][1] = t * z * y + s * x;
	result.m[2][2] = t * z * z + c;

	return result;
}

static inline matrix matrix_perspective(float fov, float aspect, float near, float far) {
	matrix result = { 0 };
	float f = 1.0f / tan(fov / 2);
	result.m[0][0] = f / aspect;
	result.m[1][1] = f;
	result.m[2][2] = (far + near) / (near - far);
	result.m[2][3] = (2 * far * near) / (near - far);
	result.m[3][2] = -1;
	return result;
}

static inline matrix matrix_orthographic(float left, float right, float bottom, float top, float near, float far) {
	matrix result = { 0 };
	result.m[0][0] = 2 / (right - left);
	result.m[1][1] = 2 / (top - bottom);
	result.m[2][2] = -2 / (far - near);
	result.m[0][3] = -(right + left) / (right - left);
	result.m[1][3] = -(top + bottom) / (top - bottom);
	result.m[2][3] = -(far + near) / (far - near);
	result.m[3][3] = 1;
	return result;
}


typedef struct {
	float x, y, z, w;
} quaternion;

static inline quaternion quaternion_identity(void) {
	quaternion q = { 0, 0, 0, 1 };
	return q;
}

static inline quaternion quaternion_mul(quaternion a, quaternion b) {
	quaternion result;
	result.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
	result.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
	result.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
	result.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
	return result;
}

static inline quaternion quaternion_normalize(quaternion q) {
	float length = sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
	if (length == 0) return q; // Avoid division by zero
	quaternion result;
	result.x = q.x / length;
	result.y = q.y / length;
	result.z = q.z / length;
	result.w = q.w / length;
	return result;
}

static inline quaternion quaternion_from_axis_angle(float x, float y, float z, float angle) {
	quaternion q;
	float half_angle = angle * 0.5f;
	float s = sin(half_angle);
	q.x = x * s;
	q.y = y * s;
	q.z = z * s;
	q.w = cos(half_angle);
	return quaternion_normalize(q);
}

static inline quaternion quaternion_from_euler(float pitch, float yaw, float roll) {
	quaternion q;
	float cy = cos(yaw * 0.5f);
	float sy = sin(yaw * 0.5f);
	float cp = cos(pitch * 0.5f);
	float sp = sin(pitch * 0.5f);
	float cr = cos(roll * 0.5f);
	float sr = sin(roll * 0.5f);

	q.x = sr * cp * cy - cr * sp * sy;
	q.y = cr * sp * cy + sr * cp * sy;
	q.z = cr * cp * sy - sr * sp * cy;
	q.w = cr * cp * cy + sr * sp * sy;

	return quaternion_normalize(q);
}

static inline quaternion quaternion_conjugate(quaternion q) {
	quaternion result;
	result.x = -q.x;
	result.y = -q.y;
	result.z = -q.z;
	result.w = q.w;
	return result;
}

static inline quaternion quaternion_inverse(quaternion q) {
	float length_squared = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
	if (length_squared == 0) return q; // Avoid division by zero
	quaternion result = quaternion_conjugate(q);
	result.x /= length_squared;
	result.y /= length_squared;
	result.z /= length_squared;
	result.w /= length_squared;
	return result;
}

static inline quaternion quaternion_slerp(quaternion a, quaternion b, float t) {
	float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
	if (dot < 0.0f) {
		b.x = -b.x;
		b.y = -b.y;
		b.z = -b.z;
		b.w = -b.w;
		dot = -dot;
	}

	if (dot > 0.9995f) {
		quaternion result;
		result.x = a.x + t * (b.x - a.x);
		result.y = a.y + t * (b.y - a.y);
		result.z = a.z + t * (b.z - a.z);
		result.w = a.w + t * (b.w - a.w);
		return quaternion_normalize(result);
	}

	float theta_0 = acos(dot);
	float theta = theta_0 * t;
	float sin_theta = sin(theta);
	float sin_theta_0 = sin(theta_0);

	float s0 = cos(theta) - dot * sin_theta / sin_theta_0;
	float s1 = sin_theta / sin_theta_0;

	quaternion result;
	result.x = s0 * a.x + s1 * b.x;
	result.y = s0 * a.y + s1 * b.y;
	result.z = s0 * a.z + s1 * b.z;
	result.w = s0 * a.w + s1 * b.w;

	return quaternion_normalize(result);
}

static inline quaternion quaternion_from_matrix(matrix m) {
	quaternion q;
	float trace = m.m[0][0] + m.m[1][1] + m.m[2][2];
	if (trace > 0) {
		float s = sqrt(trace + 1.0f) * 2; // S=4*q.w
		q.w = 0.25f * s;
		q.x = (m.m[2][1] - m.m[1][2]) / s;
		q.y = (m.m[0][2] - m.m[2][0]) / s;
		q.z = (m.m[1][0] - m.m[0][1]) / s;
	}
	else if ((m.m[0][0] > m.m[1][1]) && (m.m[0][0] > m.m[2][2])) {
		float s = sqrt(1.0f + m.m[0][0] - m.m[1][1] - m.m[2][2]) * 2; // S=4*q.x
		q.w = (m.m[2][1] - m.m[1][2]) / s;
		q.x = 0.25f * s;
		q.y = (m.m[0][1] + m.m[1][0]) / s;
		q.z = (m.m[0][2] + m.m[2][0]) / s;
	}
	else if (m.m[1][1] > m.m[2][2]) {
		float s = sqrt(1.0f + m.m[1][1] - m.m[0][0] - m.m[2][2]) * 2; // S=4*q.y
		q.w = (m.m[0][2] - m.m[2][0]) / s;
		q.x = (m.m[0][1] + m.m[1][0]) / s;
		q.y = 0.25f * s;
		q.z = (m.m[1][2] + m.m[2][1]) / s;
	}
	else {
		float s = sqrt(1.0f + m.m[2][2] - m.m[0][0] - m.m[1][1]) * 2; // S=4*q.z
		q.w = (m.m[1][0] - m.m[0][1]) / s;
		q.x = (m.m[0][2] + m.m[2][0]) / s;
		q.y = (m.m[1][2] + m.m[2][1]) / s;
		q.z = 0.25f * s;
	}
	return quaternion_normalize(q);
}

static inline matrix matrix_from_quaternion(quaternion q) {
	matrix result = { 0 };
	result.m[0][0] = 1 - 2 * (q.y * q.y + q.z * q.z);
	result.m[0][1] = 2 * (q.x * q.y - q.z * q.w);
	result.m[0][2] = 2 * (q.x * q.z + q.y * q.w);
	result.m[0][3] = 0;

	result.m[1][0] = 2 * (q.x * q.y + q.z * q.w);
	result.m[1][1] = 1 - 2 * (q.x * q.x + q.z * q.z);
	result.m[1][2] = 2 * (q.y * q.z - q.x * q.w);
	result.m[1][3] = 0;

	result.m[2][0] = 2 * (q.x * q.z - q.y * q.w);
	result.m[2][1] = 2 * (q.y * q.z + q.x * q.w);
	result.m[2][2] = 1 - 2 * (q.x * q.x + q.y * q.y);
	result.m[2][3] = 0;

	result.m[3][0] = 0;
	result.m[3][1] = 0;
	result.m[3][2] = 0;
	result.m[3][3] = 1;

	return result;
}

static xyzf quaternion_rotate_vector(quaternion q, xyzf v) {
	quaternion qv = { v.x, v.y, v.z, 0 };
	quaternion q_conjugate = quaternion_conjugate(q);
	quaternion result = quaternion_mul(quaternion_mul(q, qv), q_conjugate);
	return (xyzf) {
		result.x, result.y, result.z
	};
}

static xyzf quaternion_to_euler(quaternion q) {
	xyzf euler;
	float sinr_cosp = 2 * (q.w * q.x + q.y * q.z);
	float cosr_cosp = 1 - 2 * (q.x * q.x + q.y * q.y);
	euler.x = atan2(sinr_cosp, cosr_cosp); // Roll

	float sinp = 2 * (q.w * q.y - q.z * q.x);
	if (fabs(sinp) >= 1)
		euler.y = copysign(M_PI / 2, sinp); // Pitch
	else
		euler.y = asin(sinp);

	float siny_cosp = 2 * (q.w * q.z + q.x * q.y);
	float cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
	euler.z = atan2(siny_cosp, cosy_cosp); // Yaw

	return euler;
}

#endif // GEOMETRY_MATH_H
