// Auto-generated geometry code

#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <stdint.h>
#include <math.h>

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
	double length = xyf_length(v);
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
	double length = xyzf_length(v);
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

#endif // GEOMETRY_H
