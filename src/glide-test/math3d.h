// math3d.h - Minimal 3D math for q16 Engine
#ifndef MATH3D_H
#define MATH3D_H

#include <math.h>

typedef struct {
  float x, y, z;
} Vec3;

static inline Vec3 vec3(float x, float y, float z) {
  return (Vec3){x, y, z};
}

static inline Vec3 vec3_sub(Vec3 a, Vec3 b) {
  return (Vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static inline Vec3 vec3_cross(Vec3 a, Vec3 b) {
  return (Vec3){
      a.y * b.z - a.z * b.y,
      a.z * b.x - a.x * b.z,
      a.x * b.y - a.y * b.x,
  };
}

static inline float vec3_dot(Vec3 a, Vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline float vec3_length(Vec3 v) {
  return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static inline Vec3 vec3_normalize(Vec3 v) {
  float len = vec3_length(v);
  if (len > 1e-6f) {
    float inv = 1.0f / len;
    v.x *= inv;
    v.y *= inv;
    v.z *= inv;
  }
  return v;
}

static inline Vec3 vec3_rotate_y(Vec3 p, float angle) {
  float c = cosf(angle);
  float s = sinf(angle);
  return (Vec3){p.x * c + p.z * s, p.y, -p.x * s + p.z * c};
}

#endif // MATH3D_H
