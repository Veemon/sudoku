#include "proj_math.h"

f32 abs(f32 x) {
    if (x > EPS) return x;
    return x * -1.0f;
}

f32 clip(f32 x, f32 a, f32 b) {
    if (x < a) return a;
    if (x > b) return b;
    return x;
}


void print_vec4(vec4* v) {
    printf("(  ");
    for (u32 i = 0; i < 4; i++) {
        printf("%+.4f  ",v->a[i]);
    }
    printf(")\n");
}

void print_mat4(mat4* m) {
    printf("[\n");
    for (u32 i = 0; i < 4; i++) {
        printf("    ");
        for (u32 j = 0; j < 4; j++) {
            printf("%+.4f  ",m->v[j].a[i]);
        }
        printf("\n");
    }
    printf("]\n");
}

void zero_vec4(vec4* v) {
    for (u32 i = 0; i < 4; i++) {
        v->a[i] = 0.0f;
    }
}

void zero_mat4(mat4* m) {
    for (u32 i = 0; i < 4; i++) {
        zero_vec4(&m->v[i]);
    }
}

void identity(mat4* m) {
    zero_mat4(m);
    for(u32 i = 0; i < 4; i++) {
        m->v[i].a[i] = 1.0;
    }
}

void project_orthographic(mat4* m, f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far) {
    // expects identity matrix
    f32 drl = right - left;
    f32 dtb = top   - bottom;
    f32 dfn = far   - near;

    m->x.x =  2 / drl;
    m->y.y =  2 / dtb;
    m->z.z = -2 / dfn;

    m->w.x = -(right + left  ) / drl;
    m->w.y = -(top   + bottom) / dtb;
    m->w.z = -(far   + near  ) / dfn;
}

void translate_mat4(mat4* m, vec3 v) {
    // expects identity matrix
    m->w.x = v.x;
    m->w.y = v.y;
    m->w.z = v.z;
}

void scale_mat4(mat4* m, vec3 v) {
    // expects identity matrix
    m->x.x = v.x;
    m->y.y = v.y;
    m->z.z = v.z;
}
