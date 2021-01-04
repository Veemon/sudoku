#ifndef PROJ_MATH_H
#define PROJ_MATH_H

// local
#include "proj_types.h"

// system
#include "stdio.h"


#define EPS         1e-6

#define F32_MIN     0xff7fffff
#define F32_MAX     0x7f7fffff

#define PI          3.14159265358979323846
#define TAU         6.28318530717958647693


const u32 pow_10[] = {
    1,
    10,
    100,
    1000,
    10000,
    100000,
    1000000,
    10000000,
    100000000,
    1000000000,
};

f32 abs(f32 x);
f32 clip(f32 x, f32 a, f32 b);




union vec3 {
    struct {
        f32 x;
        f32 y;
        f32 z;
    };
    f32 a[3];
};

union vec4 {
    struct {
        f32 x;
        f32 y;
        f32 z;
        f32 w;
    };
    f32 a[4];
};

union mat4 {
    struct {
        vec4 x;
        vec4 y;
        vec4 z;
        vec4 w;
    };
    vec4 v[4];
    f32  a[16];
};

void print_vec4(vec4* v);
void print_mat4(mat4* m);
void zero_vec4(vec4* v);

void zero_mat4(mat4* m);
void identity(mat4* m);

void project_orthographic(mat4* m, f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far);
void translate_mat4(mat4* m, vec3 v);
void scale_mat4(mat4* m, vec3 v);

#endif
