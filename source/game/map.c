#include "sh3ds/map.h"

#include <math.h>
#include <string.h>

/* Files store 32-bit offsets, so every structure is read field by field. */

#define IPD_MAGIC 20
#define LM_MAGIC '0'
#define LM_VERSION 6

#define CHUNK_SIZE_Q8 (40 * 256)
#define Q8_TO_METERS (1.0f / 256.0f)

#define LM_MATERIAL_SIZE 24
#define LM_MODEL_SIZE 16
#define LM_MESH_SIZE 24
#define PRIM_SIZE 20
#define IPD_MODEL_INFO_SIZE 16
#define IPD_MODEL_BUFFER_SIZE 24
#define IPD_INSTANCE_SIZE 36

typedef struct Blob {
    const uint8_t *data;
    size_t size;
} Blob;

static bool in_range(const Blob *blob, size_t offset, size_t len)
{
    return offset <= blob->size && len <= blob->size - offset;
}

static uint32_t rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int16_t rd_s16(const uint8_t *p)
{
    return (int16_t)(p[0] | (p[1] << 8));
}

/* An LM library embedded at `base` inside `blob`; its offsets are relative to `base`. */
typedef struct Lm {
    Blob blob;
    size_t base;
    unsigned model_count;
    size_t models;
    unsigned material_count;
    size_t materials;
} Lm;

static bool lm_open(Lm *lm, const uint8_t *data, size_t size, size_t base)
{
    lm->blob.data = data;
    lm->blob.size = size;
    lm->base = base;

    if (!data || !in_range(&lm->blob, base, 0x14)) {
        return false;
    }
    const uint8_t *hdr = data + base;
    if (hdr[0] != LM_MAGIC || hdr[1] != LM_VERSION) {
        return false;
    }

    lm->model_count = hdr[8];
    lm->models = base + rd_u32(hdr + 0xC);
    lm->material_count = hdr[3];
    lm->materials = base + rd_u32(hdr + 0x4);
    if (!in_range(&lm->blob, lm->materials, (size_t)lm->material_count * LM_MATERIAL_SIZE)) {
        lm->material_count = 0;
    }
    return in_range(&lm->blob, lm->models, (size_t)lm->model_count * LM_MODEL_SIZE);
}

static const uint8_t *lm_find_model(const Lm *lm, const uint8_t name[8])
{
    for (unsigned i = 0; i < lm->model_count; i++) {
        const uint8_t *model = lm->blob.data + lm->models + (size_t)i * LM_MODEL_SIZE;
        if (memcmp(model, name, 8) == 0) {
            return model;
        }
    }
    return NULL;
}

typedef struct Transform {
    int32_t m[3][3]; /* Q12 */
    int32_t t[3];    /* Q8 world units */
} Transform;

static void expand_bounds(ShMapBuilder *b, const float p[3])
{
    for (int i = 0; i < 3; i++) {
        if (p[i] < b->min[i]) {
            b->min[i] = p[i];
        }
        if (p[i] > b->max[i]) {
            b->max[i] = p[i];
        }
    }
}

typedef struct Corner {
    const float *pos;
    uint8_t u;
    uint8_t v;
} Corner;

static void emit_triangle(ShMapBuilder *b, Corner c0, Corner c1, Corner c2, unsigned slot)
{
    if (b->count + 3 > b->capacity) {
        b->overflow = true;
        return;
    }

    const float *p0 = c0.pos;
    const float *p1 = c1.pos;
    const float *p2 = c2.pos;

    const float ux = p1[0] - p0[0], uy = p1[1] - p0[1], uz = p1[2] - p0[2];
    const float vx = p2[0] - p0[0], vy = p2[1] - p0[1], vz = p2[2] - p0[2];
    float nx = uy * vz - uz * vy;
    float ny = uz * vx - ux * vz;
    float nz = ux * vy - uy * vx;
    const float len = sqrtf(nx * nx + ny * ny + nz * nz);
    if (len < 1e-6f) {
        return;
    }
    nx /= len;
    ny /= len;
    nz /= len;

    /* Two-sided lambert from a high, slightly tilted light: floors bright, walls vary. */
    const float lx = 0.35f, ly = 0.85f, lz = 0.40f;
    const float diffuse = fabsf(nx * lx + ny * ly + nz * lz);
    const bool textured = slot != SH_MAP_NO_TEXTURE;
    const float shade = textured ? 0.55f + 0.45f * diffuse : 0.22f + 0.78f * diffuse;
    const uint8_t r = (uint8_t)(shade * (textured ? 255.0f : 205.0f));
    const uint8_t g = (uint8_t)(shade * (textured ? 255.0f : 200.0f));
    const uint8_t bl = (uint8_t)(shade * (textured ? 255.0f : 190.0f));

    if (b->tri_slots) {
        b->tri_slots[b->count / 3] = (uint8_t)slot;
    }

    const Corner corners[3] = {c0, c1, c2};
    for (int i = 0; i < 3; i++) {
        ShMapVertex *v = &b->verts[b->count++];
        memcpy(v->pos, corners[i].pos, sizeof(v->pos));
        v->uv[0] = corners[i].u;
        v->uv[1] = corners[i].v;
        v->color[0] = r;
        v->color[1] = g;
        v->color[2] = bl;
        v->color[3] = 255;
        expand_bounds(b, corners[i].pos);
    }
}

typedef struct SlotMemo {
    const uint8_t *material;
    unsigned clut_delta;
    unsigned slot;
} SlotMemo;

static unsigned resolve_slot(ShMapBuilder *b, const Lm *lm, const uint8_t *prim, SlotMemo *memo)
{
    const unsigned material_idx = prim[7] & 0x7F;
    if (!b->resolve || material_idx >= lm->material_count) {
        return SH_MAP_NO_TEXTURE;
    }

    const uint8_t *material = lm->blob.data + lm->materials + (size_t)material_idx * LM_MATERIAL_SIZE;
    /* CLUT id relative to the one the file was authored with (Model_MaterialFlagsApply). */
    const unsigned clut_delta = (unsigned)((prim[2] | (prim[3] << 8)) -
                                           (material[0x12] | (material[0x13] << 8))) & 0xFFFFu;

    if (memo->material != material || memo->clut_delta != clut_delta) {
        memo->material = material;
        memo->clut_delta = clut_delta;
        memo->slot = b->resolve(b->resolve_user, (const char *)material, clut_delta);
    }
    return memo->slot;
}

static void draw_model(ShMapBuilder *b, const Lm *lm, const uint8_t *model, const Transform *xf)
{
    /* Mirrors the engine's per-model vertex scratch: meshes write at vertexOffset. */
    static float scratch[512][3];
    memset(scratch, 0, sizeof(scratch));

    const unsigned mesh_count = model[8];
    const unsigned vert_offset = model[9];
    const size_t meshes = lm->base + rd_u32(model + 0xC);
    if (!in_range(&lm->blob, meshes, (size_t)mesh_count * LM_MESH_SIZE)) {
        return;
    }

    for (unsigned m = 0; m < mesh_count; m++) {
        const uint8_t *mesh = lm->blob.data + meshes + (size_t)m * LM_MESH_SIZE;
        const unsigned prim_count = mesh[0];
        const unsigned vert_count = mesh[1];
        const size_t prims = lm->base + rd_u32(mesh + 0x4);
        const size_t xy = lm->base + rd_u32(mesh + 0x8);
        const size_t z = lm->base + rd_u32(mesh + 0xC);

        if (!in_range(&lm->blob, prims, (size_t)prim_count * PRIM_SIZE) ||
            !in_range(&lm->blob, xy, (size_t)vert_count * 4) ||
            !in_range(&lm->blob, z, (size_t)vert_count * 2)) {
            continue;
        }

        for (unsigned i = 0; i < vert_count; i++) {
            const int32_t lx = rd_s16(lm->blob.data + xy + i * 4);
            const int32_t ly = rd_s16(lm->blob.data + xy + i * 4 + 2);
            const int32_t lz = rd_s16(lm->blob.data + z + i * 2);
            int32_t w[3];
            for (int r = 0; r < 3; r++) {
                w[r] = ((xf->m[r][0] * lx + xf->m[r][1] * ly + xf->m[r][2] * lz) >> 12) + xf->t[r];
            }
            float *out = scratch[vert_offset + i];
            out[0] = w[0] * Q8_TO_METERS;
            out[1] = -w[1] * Q8_TO_METERS;
            out[2] = -w[2] * Q8_TO_METERS;
        }

        SlotMemo memo = {NULL, 0, SH_MAP_NO_TEXTURE};
        for (unsigned p = 0; p < prim_count; p++) {
            const uint8_t *prim = lm->blob.data + prims + (size_t)p * PRIM_SIZE;
            const uint8_t *idx = prim + 0xC;
            /* UVs are already local to the material's TIM (field_16 is 0 in the files). */
            const Corner c0 = {scratch[idx[0]], prim[0x0], prim[0x1]};
            const Corner c1 = {scratch[idx[1]], prim[0x4], prim[0x5]};
            const Corner c2 = {scratch[idx[2]], prim[0x8], prim[0x9]};
            const unsigned slot = resolve_slot(b, lm, prim, &memo);

            emit_triangle(b, c0, c1, c2, slot);
            if (idx[3] != 0xFF) {
                /* PS1 quads are a 2x2 strip: (0,1,2) + (1,3,2). */
                const Corner c3 = {scratch[idx[3]], prim[0xA], prim[0xB]};
                emit_triangle(b, c1, c3, c2, slot);
            }
        }
    }
}

bool sh_map_add_named_model(ShMapBuilder *b, const uint8_t *ipd, size_t ipd_size, const uint8_t *glb,
                            size_t glb_size, const char *name, const float pos_ps1[3], const float rot_deg[3])
{
    const Blob file = {ipd, ipd_size};
    if (!b || !ipd || !name || !in_range(&file, 0, 0x54) || ipd[0] != IPD_MAGIC) {
        return false;
    }

    uint8_t key[8];
    memset(key, 0, sizeof(key));
    for (int i = 0; i < 8 && name[i]; i++) {
        key[i] = (uint8_t)name[i];
    }

    uint8_t key_sp[8];
    memcpy(key_sp, key, 8);
    for (int i = 0; i < 8; i++) {
        if (key_sp[i] == 0) {
            key_sp[i] = ' ';
        }
    }

    Lm local;
    Lm global;
    const Lm *lib = NULL;
    const uint8_t *model = NULL;
    if (lm_open(&local, ipd, ipd_size, rd_u32(ipd + 0x4))) {
        model = lm_find_model(&local, key);
        if (!model) {
            model = lm_find_model(&local, key_sp);
        }
        lib = &local;
    }
    if (!model && glb && lm_open(&global, glb, glb_size, 0)) {
        model = lm_find_model(&global, key);
        if (!model) {
            model = lm_find_model(&global, key_sp);
        }
        lib = &global;
    }
    if (!model) {
        return false;
    }

    /* Ry * Rx * Rz in PS1 space (Math_RotMatrixZxyNeg: Z applied first), Q12. */
    const float d2r = 3.14159265f / 180.0f;
    const float rx = rot_deg ? rot_deg[0] * d2r : 0.0f;
    const float ry = rot_deg ? rot_deg[1] * d2r : 0.0f;
    const float rz = rot_deg ? rot_deg[2] * d2r : 0.0f;
    const float cx = cosf(rx), sx = sinf(rx);
    const float cy = cosf(ry), sy = sinf(ry);
    const float cz = cosf(rz), sz = sinf(rz);
    float m[3][3];
    const float Ry[3][3] = {{cy, 0, sy}, {0, 1, 0}, {-sy, 0, cy}};
    const float Rx[3][3] = {{1, 0, 0}, {0, cx, -sx}, {0, sx, cx}};
    const float Rz[3][3] = {{cz, -sz, 0}, {sz, cz, 0}, {0, 0, 1}};
    float t[3][3];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            t[i][j] = Rx[i][0] * Rz[0][j] + Rx[i][1] * Rz[1][j] + Rx[i][2] * Rz[2][j];
        }
    }
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            m[i][j] = Ry[i][0] * t[0][j] + Ry[i][1] * t[1][j] + Ry[i][2] * t[2][j];
        }
    }

    Transform xf;
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            xf.m[r][c] = (int32_t)(m[r][c] * 4096.0f);
        }
        xf.t[r] = (int32_t)(pos_ps1[r] * 256.0f);
    }
    draw_model(b, lib, model, &xf);
    b->instances++;
    return true;
}

void sh_map_builder_init(ShMapBuilder *b, ShMapVertex *storage, uint8_t *tri_slots, size_t capacity,
                         ShMapTextureResolver resolve, void *resolve_user)
{
    memset(b, 0, sizeof(*b));
    b->verts = storage;
    b->tri_slots = tri_slots;
    b->capacity = capacity;
    b->resolve = resolve;
    b->resolve_user = resolve_user;
    for (int i = 0; i < 3; i++) {
        b->min[i] = 1e30f;
        b->max[i] = -1e30f;
    }
}

bool sh_map_add_chunk(ShMapBuilder *b, const uint8_t *ipd, size_t ipd_size,
                      const uint8_t *glb, size_t glb_size)
{
    const Blob file = {ipd, ipd_size};
    if (!ipd || !in_range(&file, 0, 0x54) || ipd[0] != IPD_MAGIC) {
        return false;
    }

    Lm local;
    Lm global;
    if (!lm_open(&local, ipd, ipd_size, rd_u32(ipd + 0x4))) {
        return false;
    }
    const bool has_global = glb && lm_open(&global, glb, glb_size, 0);

    const int32_t chunk_x = (int8_t)ipd[2];
    const int32_t chunk_z = (int8_t)ipd[3];
    const unsigned model_count = ipd[8];
    const unsigned buffer_count = ipd[9];
    const size_t infos = rd_u32(ipd + 0x14);
    const size_t buffers = rd_u32(ipd + 0x18);

    if (!in_range(&file, infos, (size_t)model_count * IPD_MODEL_INFO_SIZE) ||
        !in_range(&file, buffers, (size_t)buffer_count * IPD_MODEL_BUFFER_SIZE)) {
        return false;
    }

    for (unsigned bi = 0; bi < buffer_count; bi++) {
        const uint8_t *buffer = ipd + buffers + (size_t)bi * IPD_MODEL_BUFFER_SIZE;
        const unsigned inst_count = buffer[0];
        const size_t insts = rd_u32(buffer + 0xC);
        if (!in_range(&file, insts, (size_t)inst_count * IPD_INSTANCE_SIZE)) {
            continue;
        }

        for (unsigned ii = 0; ii < inst_count; ii++) {
            const uint8_t *inst = ipd + insts + (size_t)ii * IPD_INSTANCE_SIZE;
            const uint32_t model_idx = rd_u32(inst);
            if (model_idx >= model_count) {
                b->missing_models++;
                continue;
            }

            const uint8_t *info = ipd + infos + (size_t)model_idx * IPD_MODEL_INFO_SIZE;
            const Lm *lib = info[0] ? (has_global ? &global : NULL) : &local;
            const uint8_t *model = lib ? lm_find_model(lib, info + 4) : NULL;
            if (!model) {
                b->missing_models++;
                continue;
            }

            Transform xf;
            for (int r = 0; r < 3; r++) {
                for (int c = 0; c < 3; c++) {
                    xf.m[r][c] = rd_s16(inst + 4 + (r * 3 + c) * 2);
                }
                xf.t[r] = (int32_t)rd_u32(inst + 4 + 20 + r * 4);
            }
            xf.t[0] += chunk_x * CHUNK_SIZE_Q8;
            xf.t[2] += chunk_z * CHUNK_SIZE_Q8;

            draw_model(b, lib, model, &xf);
            b->instances++;
        }
    }

    b->chunks++;
    return true;
}
