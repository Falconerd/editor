#include "common.h"

void m4_ortho(m4 *m, f32 l, f32 r, f32 b, f32 t, f32 n, f32 f) {
    m->data[0][0] = 2.f / (r - l);
    m->data[0][1] = m->data[0][2] = m->data[0][3] = 0.f;

    m->data[1][1] = 2.f / (t - b);
    m->data[1][0] = m->data[1][2] = m->data[1][3] = 0.f;

    m->data[2][2] = -2.f / (f - n);
    m->data[2][0] = m->data[2][1] = m->data[2][3] = 0.f;

    m->data[3][0] = -(r + l) / (r - l);
    m->data[3][1] = -(t + b) / (t - b);
    m->data[3][2] = -(f + n) / (f - n);
    m->data[3][3] = 1.f;
}
