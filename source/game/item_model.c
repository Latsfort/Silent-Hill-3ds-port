#include "sh3ds/item_model.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Gfx_PickupItemAnimate: item at Q8(-32.6875), camera at Q12(-2.5), offset Q8(0.25) down. */
#define ITEM_VIEW_DIST 1872.0f
#define ITEM_VIEW_DROP 64.0f
#define ITEM_METERS_PER_UNIT (1.0f / 2048.0f)
/* PS1 projection: geom screen 224 for a 224-line view. */
#define ITEM_PS1_TAN_HALF_FOV 0.5f
#define ITEM_MAX_TRIS (SH_ITEM_MAX_VERTS / 3)

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

typedef struct ItemObj {
    const uint8_t *verts;
    uint32_t nverts;
    const uint8_t *norms;
    uint32_t nnorms;
    const uint8_t *prims;
    uint32_t nprims;
    const uint8_t *end;
} ItemObj;

static bool item_obj(const uint8_t *tmd, size_t size, ItemObj *o)
{
    if (!tmd || size < 12 + 28 || rd32(tmd) != 0x41 || rd32(tmd + 8) < 1) {
        return false;
    }
    const uint8_t *base = tmd + 12;
    const uint32_t vt = rd32(base), nv = rd32(base + 4), nt = rd32(base + 8), nn = rd32(base + 12);
    const uint32_t pt = rd32(base + 16), np = rd32(base + 20);
    const size_t avail = size - 12;
    if ((size_t)vt + (size_t)nv * 8u > avail || (size_t)nt + (size_t)nn * 8u > avail || pt >= avail) {
        return false;
    }
    o->verts = base + vt;
    o->nverts = nv;
    o->norms = base + nt;
    o->nnorms = nn;
    o->prims = base + pt;
    o->nprims = np;
    o->end = tmd + size;
    return true;
}

bool sh_item_model_init(ShItemModel *model, const uint8_t *tmd, size_t tmd_size, const uint8_t *tim_file,
                        size_t tim_size)
{
    if (!model) {
        return false;
    }
    memset(model, 0, sizeof(*model));
    ItemObj o;
    if (!item_obj(tmd, tmd_size, &o) || !tim_file || tim_size < 20 || !(rd32(tim_file + 4) & 8)) {
        return false;
    }
    const unsigned clut_x = rd16(tim_file + 12) / 16u;
    const unsigned clut_y = rd16(tim_file + 14);
    const uint8_t *p = o.prims;
    for (uint32_t i = 0; i < o.nprims && p + 12 <= o.end; i++) {
        const int ilen = p[1];
        if (p[3] & 4) {
            const unsigned cba = rd16(p + 6);
            const unsigned row = (cba >> 6) - clut_y;
            const unsigned col = (cba & 63u) - clut_x;
            model->clut_delta = (row << 6) | (col & 63u);
            break;
        }
        p += 4 + ilen * 4;
    }
    model->tmd = tmd;
    model->tmd_size = tmd_size;
    model->ok = true;
    return true;
}

typedef struct ItemTri {
    ShMapVertex v[3];
    float depth;
} ItemTri;

static ItemTri s_tris[ITEM_MAX_TRIS];

static int cmp_tri_far_first(const void *a, const void *b)
{
    const float da = ((const ItemTri *)a)->depth, db = ((const ItemTri *)b)->depth;
    return (da < db) - (da > db);
}

typedef struct ItemXform {
    float m[3][3];
    float scale;  /* model units → PS1 view units */
    float shrink; /* matches the 3DS FOV to the PS1 projection */
} ItemXform;

static void rotate(const ItemXform *x, const float in[3], float out[3])
{
    for (int r = 0; r < 3; r++) {
        out[r] = x->m[r][0] * in[0] + x->m[r][1] * in[1] + x->m[r][2] * in[2];
    }
}

static void load_vec(const uint8_t *p, float out[3])
{
    out[0] = (float)(int16_t)rd16(p);
    out[1] = (float)(int16_t)rd16(p + 2);
    out[2] = (float)(int16_t)rd16(p + 4);
}

static int s_tri_count;

static void emit(const ItemObj *o, const ItemXform *x, const unsigned vi[3], const unsigned ni[3],
                 const uint8_t uv[3][2], bool double_sided)
{
    if (s_tri_count >= ITEM_MAX_TRIS) {
        return;
    }
    float view[3][3];
    float nsum[3] = {0.0f, 0.0f, 0.0f};
    float light[3];
    for (int k = 0; k < 3; k++) {
        if (vi[k] >= o->nverts || ni[k] >= o->nnorms) {
            return;
        }
        float v[3], r[3], n[3], rn[3];
        load_vec(o->verts + vi[k] * 8u, v);
        rotate(x, v, r);
        view[k][0] = r[0] * x->scale;
        view[k][1] = r[1] * x->scale + ITEM_VIEW_DROP;
        view[k][2] = r[2] * x->scale + ITEM_VIEW_DIST;
        load_vec(o->norms + ni[k] * 8u, n);
        rotate(x, n, rn);
        const float len = sqrtf(rn[0] * rn[0] + rn[1] * rn[1] + rn[2] * rn[2]);
        if (len > 1e-6f) {
            rn[0] /= len;
            rn[1] /= len;
            rn[2] /= len;
        }
        nsum[0] += rn[0];
        nsum[1] += rn[1];
        nsum[2] += rn[2];
        /* Flat lights travel +Z (into the screen) and +X; ambient 2048/4096. */
        float l = 0.5f;
        if (-rn[2] > 0.0f) l += -rn[2];
        if (-rn[0] > 0.0f) l += -rn[0];
        light[k] = l;
    }

    if (!double_sided) {
        const float ax = view[1][0] - view[0][0], ay = view[1][1] - view[0][1], az = view[1][2] - view[0][2];
        const float bx = view[2][0] - view[0][0], by = view[2][1] - view[0][1], bz = view[2][2] - view[0][2];
        float gx = ay * bz - az * by, gy = az * bx - ax * bz, gz = ax * by - ay * bx;
        if (gx * nsum[0] + gy * nsum[1] + gz * nsum[2] < 0.0f) {
            gx = -gx;
            gy = -gy;
            gz = -gz;
        }
        if (gx * view[0][0] + gy * view[0][1] + gz * view[0][2] >= 0.0f) {
            return;
        }
    }

    ItemTri *t = &s_tris[s_tri_count++];
    t->depth = (view[0][2] + view[1][2] + view[2][2]) * (1.0f / 3.0f);
    for (int k = 0; k < 3; k++) {
        ShMapVertex *v = &t->v[k];
        const float cx = (view[k][0]) * x->shrink;
        const float cy = (view[k][1] - ITEM_VIEW_DROP) * x->shrink + ITEM_VIEW_DROP;
        const float cz = (view[k][2] - ITEM_VIEW_DIST) * x->shrink + ITEM_VIEW_DIST;
        v->pos[0] = cx * ITEM_METERS_PER_UNIT;
        v->pos[1] = -cy * ITEM_METERS_PER_UNIT;
        v->pos[2] = -cz * ITEM_METERS_PER_UNIT;
        v->uv[0] = (float)uv[k][0] / (float)SH_ITEM_TEX_SIZE;
        v->uv[1] = 1.0f - (float)uv[k][1] / (float)SH_ITEM_TEX_SIZE;
        float c = light[k] * 128.0f;
        if (c > 255.0f) c = 255.0f;
        v->color[0] = v->color[1] = v->color[2] = (uint8_t)c;
        v->color[3] = 255;
    }
}

bool sh_item_model_bake(const ShItemModel *model, ShMapVertex *out, size_t capacity, size_t *count_out,
                        float rot_x, float rot_y, float rot_z, float scale, float fov_deg)
{
    if (!model || !model->ok || !out || !count_out) {
        return false;
    }
    *count_out = 0;
    ItemObj o;
    if (!item_obj(model->tmd, model->tmd_size, &o) || scale <= 0.0f) {
        return false;
    }

    /* Math_RotMatrixZxyNegGte: Z first, then X, then Y (PS1 axes, Y down). */
    const float cx = cosf(rot_x), sx = sinf(rot_x);
    const float cy = cosf(rot_y), sy = sinf(rot_y);
    const float cz = cosf(rot_z), sz = sinf(rot_z);
    const float rz[3][3] = {{cz, -sz, 0.0f}, {sz, cz, 0.0f}, {0.0f, 0.0f, 1.0f}};
    const float rx[3][3] = {{1.0f, 0.0f, 0.0f}, {0.0f, cx, -sx}, {0.0f, sx, cx}};
    const float ry[3][3] = {{cy, 0.0f, sy}, {0.0f, 1.0f, 0.0f}, {-sy, 0.0f, cy}};
    float xz[3][3];
    ItemXform x;
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            xz[r][c] = rx[r][0] * rz[0][c] + rx[r][1] * rz[1][c] + rx[r][2] * rz[2][c];
        }
    }
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            x.m[r][c] = ry[r][0] * xz[0][c] + ry[r][1] * xz[1][c] + ry[r][2] * xz[2][c];
        }
    }
    x.scale = scale;
    const float fov = fov_deg > 10.0f && fov_deg < 140.0f ? fov_deg : 48.0f;
    x.shrink = tanf(fov * 0.5f * 3.14159265f / 180.0f) / ITEM_PS1_TAN_HALF_FOV;

    s_tri_count = 0;
    const uint8_t *p = o.prims;
    for (uint32_t i = 0; i < o.nprims; i++) {
        if (p + 4 > o.end) {
            break;
        }
        const int ilen = p[1];
        const int flag = p[2];
        const int mode = p[3] & ~3;
        const uint8_t *d = p + 4;
        if (d + ilen * 4 > o.end) {
            break;
        }
        const bool two = (flag & 2) != 0;
        uint8_t uv[4][2];
        unsigned vi[4], ni[4];
        int nvtx = 0;
        if (mode == 0x24 || mode == 0x2C || mode == 0x34 || mode == 0x3C) {
            const bool quad = (mode & 8) != 0;
            const bool gouraud = (mode & 0x10) != 0;
            nvtx = quad ? 4 : 3;
            for (int k = 0; k < nvtx; k++) {
                uv[k][0] = d[k * 4];
                uv[k][1] = d[k * 4 + 1];
            }
            const uint8_t *idx = d + (quad ? 16 : 12);
            if (gouraud) {
                for (int k = 0; k < nvtx; k++) {
                    ni[k] = rd16(idx + k * 4);
                    vi[k] = rd16(idx + k * 4 + 2);
                }
            } else {
                const unsigned n0 = rd16(idx);
                for (int k = 0; k < nvtx; k++) {
                    ni[k] = n0;
                    vi[k] = rd16(idx + 2 + k * 2);
                }
            }
        }
        if (nvtx >= 3) {
            const unsigned a[3] = {vi[0], vi[1], vi[2]};
            const unsigned an[3] = {ni[0], ni[1], ni[2]};
            const uint8_t auv[3][2] = {{uv[0][0], uv[0][1]}, {uv[1][0], uv[1][1]}, {uv[2][0], uv[2][1]}};
            emit(&o, &x, a, an, auv, two);
            if (nvtx == 4) {
                const unsigned b[3] = {vi[1], vi[3], vi[2]};
                const unsigned bn[3] = {ni[1], ni[3], ni[2]};
                const uint8_t buv[3][2] = {{uv[1][0], uv[1][1]}, {uv[3][0], uv[3][1]}, {uv[2][0], uv[2][1]}};
                emit(&o, &x, b, bn, buv, two);
            }
        }
        p += 4 + ilen * 4;
    }

    qsort(s_tris, (size_t)s_tri_count, sizeof(s_tris[0]), cmp_tri_far_first);
    size_t n = 0;
    for (int t = 0; t < s_tri_count && n + 3 <= capacity; t++) {
        memcpy(out + n, s_tris[t].v, sizeof(s_tris[t].v));
        n += 3;
    }
    *count_out = n;
    return n >= 3;
}
