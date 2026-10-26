#include "sh3ds/papermap.h"

#include <string.h>

/*
 * bodyprog_mapscreen_80066D90.c: D_800AE8A0 holds, per map, sprite rects
 * {u, v, w, h} followed by placements {x/2, y/2 (s8, centred), sprite_a, sprite_b}.
 */
static const uint32_t s_mark_data[327] = {
    0x00000000, 0x1F0F0000, 0x1F0F0010, 0x1F0F0020, 0x1F0F0030, 0x1F0F0040, 0x1F0F0050, 0x1F0F0060, 0x2F1F7050,
    0x1F1F2020, 0x1F1F2040, 0x3F1F4000, 0x2F0F4020, 0x5F4F8000, 0x2F1F4050, 0x1F1F2060, 0x5F0F4070, 0x3F1FA050,
    0x00018E07, 0x0004E20A, 0x0005E90C, 0x000EBEF8, 0x0001BDDF, 0x0001ADCA, 0x0001DDC1, 0x00016ACA, 0x00015CDD,
    0x00015C00, 0x00013606, 0x00014324, 0x000D4703, 0x00010F24, 0x00010E07, 0x0001BC37, 0x000120DE, 0x00011AE8,
    0x000122EF, 0x000137E8, 0x000B20E2, 0x0001F9DD, 0x0009DFDB, 0x000944DB, 0x000FBB25, 0x0010C715, 0x0011001F,
    0x0601203C, 0x000A1339, 0x03022B0D, 0x000029C4, 0x03024EBD, 0x000844DB, 0x0007E0DF, 0x00000000, 0x1F3F8000,
    0x3F1F4030, 0x1F1F4050, 0x3F1F4000, 0x1F0F0000, 0x1F0F0010, 0x1F0F0020, 0x1F0F0040, 0x0001962C, 0x00065BF0,
    0x0002CCE4, 0x000369E4, 0x0002F232, 0x00024131, 0x00038820, 0x0001962C, 0x00065BF0, 0x0002CAE4, 0x000369E4,
    0x00038820, 0x0005D124, 0x00052224, 0x00051C06, 0x00023FF9, 0x0002F245, 0x0002EFCE, 0x0002A13C, 0x00050442,
    0x000088B0, 0x00023EE8, 0x00059710, 0x0002AC2C, 0x000509F1, 0x00050FEA, 0x0002EDC0, 0x0005F300, 0x07064DE1,
    0x0008EAC8, 0x00000000, 0x1F0F0000, 0x1F0F0010, 0x1F0F0020, 0x1F0F0030, 0x1F0F0040, 0x1F0F0050, 0x1F0F0060,
    0x1F1F2000, 0x1F1F2020, 0x1F0F2050, 0x1F0F2040, 0x1F0F2060, 0x1F1F4000, 0x04025224, 0x04003824, 0x0300580C,
    0x0300380C, 0x040B30F4, 0x030144F6, 0x03003817, 0x0301200C, 0x040B3024, 0x040BC024, 0x0001B826, 0x0402BE2C,
    0x0402F22C, 0x0402FE2C, 0x0402322C, 0x0300F839, 0x0300D00C, 0x0001B817, 0x0300B80C, 0x0001B8F9, 0x0400C0F4,
    0x0001980C, 0x0002BDEC, 0x0002F1EC, 0x0002FEEC, 0x000231EC, 0x040044EC, 0x040054EC, 0x00073CFE, 0x00061C1E,
    0x000230F4, 0x0002C024, 0x0001580C, 0x0300380C, 0x040B30F4, 0x030044F6, 0x03003816, 0x030A200C, 0x04003024,
    0x040BC024, 0x0300B826, 0x0400BE2C, 0x0400F22C, 0x0402FE2C, 0x0400322C, 0x0300F839, 0x0300D00C, 0x0300B817,
    0x0300B80C, 0x0300B8F9, 0x040BC0F4, 0x0400BDEC, 0x0002F1EC, 0x0400FEEC, 0x040031EC, 0x040044EC, 0x040054EC,
    0x0300F8DF, 0x0001F8F0, 0x0001F828, 0x030A3828, 0x0007F80C, 0x00059C0C, 0x040B4EF4, 0x000C3928, 0x040B30F4,
    0x030038F7, 0x03003817, 0x04023024, 0x0402C024, 0x0301B827, 0x0402BE2C, 0x0402F22C, 0x0402FE2C, 0x0402322C,
    0x0300F83A, 0x030AB821, 0x0300B800, 0x0300B8FA, 0x0400C0F4, 0x0400BEEC, 0x0400F2EC, 0x0400FEEC, 0x040032EC,
    0x0300F8DF, 0x040044EC, 0x040054EC, 0x04009E24, 0x0009AF08, 0x000854F8, 0x0007441E, 0x040030F4, 0x000138F7,
    0x03003817, 0x040B3024, 0x040BC024, 0x0300B828, 0x040BBE2C, 0x040BF22C, 0x0400FE2C, 0x0400322C, 0x0300F83A,
    0x030AB821, 0x0001B801, 0x0001B8FB, 0x0400C0F4, 0x0400BEEC, 0x0400F2EC, 0x0400FEEC, 0x040032EC, 0x0300F8DF,
    0x040044EC, 0x040054EC, 0x0001F8F0, 0x000138F0, 0x0001F828, 0x04009E24, 0x0402A0F3, 0x000DA922, 0x00000000,
    0x0F070040, 0x0F070038, 0x0F070010, 0x0F070018, 0x2F1F1000, 0x0301240C, 0x0402C32E, 0x0402B72E, 0x0301CB2F,
    0x04022336, 0x04022322, 0x040224FE, 0x04022DE6, 0x000509DC, 0x0005E528, 0x00000000, 0x1F0F0000, 0x1F0F0010,
    0x1F0F0020, 0x1F0F0030, 0x1F0F0040, 0x1F0F0050, 0x1F0F0060, 0x1F0F2050, 0x1F0F2040, 0x1F1F2000, 0x000110E1,
    0x0400F3E8, 0x0300C8F7, 0x0300E1EB, 0x0400FF00, 0x03001014, 0x00012812, 0x0001281E, 0x0300342D, 0x04021C24,
    0x0402FE24, 0x0402E524, 0x0409C324, 0x0002C324, 0x00021D0C, 0x0402A018, 0x000110E1, 0x0400F3E8, 0x0001E1EB,
    0x0409FF00, 0x03011014, 0x03002812, 0x0300281E, 0x00021C24, 0x0400FE24, 0x0400E524, 0x0400C324, 0x04001D0C,
    0x0002A018, 0x000128FC, 0x00012804, 0x0400CD24, 0x0002B018, 0x0002DB18, 0x0400E624, 0x0002FE24, 0x0400CD24,
    0x0400B018, 0x0409DB18, 0x0400E624, 0x0002FE24, 0x0001BC1E, 0x0308BC1E, 0x0400C324, 0x0002E624, 0x04000D24,
    0x03002818, 0x04004024, 0x03081015, 0x04091D0C, 0x00012804, 0x000128FC, 0x04001DF4, 0x040008F4, 0x0002EFF4,
    0x0002DCF4, 0x0400C0F4, 0x0400FF00, 0x0001BC1E, 0x0308BC1E, 0x0002C324, 0x0400DC24, 0x00020C24, 0x03002818,
    0x00024024, 0x03001015, 0x04001C0C, 0x030028FC, 0x00012803, 0x04001DF4, 0x040009F4, 0x0400F1F4, 0x0002DCF4,
    0x0400C0F4, 0x04000000, 0x000AE5E0,
};

/* D_800AEDBC: sprite table, placement table, first/last marking flag. */
typedef struct MarkSet {
    int sprites;
    int places;
    unsigned flag_first;
    unsigned flag_last;
} MarkSet;

static const MarkSet s_mark_sets[] = {
    {-1, -1, 0, 0},           {0, 18, 0x0363, 0x03A8},    {52, 61, 0x03AF, 0x03BE},   {52, 68, 0x03D1, 0x03F0},
    {52, 83, 0x03F3, 0x0404}, {91, 105, 0x0415, 0x041A},  {91, 107, 0x0429, 0x0466},  {91, 170, 0x04C5, 0x04FA},
    {91, 222, 0x0549, 0x054E}, {91, 105, 0x041F, 0x0424}, {91, 137, 0x0477, 0x04BA},  {91, 196, 0x0507, 0x053C},
    {91, 222, 0x0551, 0x0556}, {224, 230, 0x0559, 0x0562}, {224, 234, 0x056D, 0x0578}, {224, 239, 0x0585, 0x0588},
    {240, 282, 0x05E7, 0x05F2}, {240, 251, 0x058F, 0x05B0}, {240, 292, 0x060F, 0x0612}, {240, 309, 0x0615, 0x0618},
    {240, 287, 0x05FB, 0x0606}, {240, 267, 0x05BB, 0x05DA}, {240, 293, 0x061B, 0x063C}, {240, 310, 0x0649, 0x066C},
};
#define MARK_SET_COUNT ((int)(sizeof(s_mark_sets) / sizeof(s_mark_sets[0])))

static const MarkSet *mark_set(int map_idx)
{
    if (map_idx <= 0 || map_idx >= MARK_SET_COUNT || s_mark_sets[map_idx].sprites < 0) {
        return NULL;
    }
    return &s_mark_sets[map_idx];
}

/* func_80068E0C quad placement, mapped from the 320x480 interlaced screen to 320x240. */
static bool place(const MarkSet *set, int i, int j, float shade, float scroll_x, float scroll_y, float scale,
                  ShMapSprite *out)
{
    const uint32_t p = s_mark_data[set->places + i];
    const int8_t px = (int8_t)(p & 0xFF);
    const int8_t py = (int8_t)((p >> 8) & 0xFF);
    const unsigned spr = j ? (p >> 24) & 0xFF : (p >> 16) & 0xFF;
    if (spr == 0) {
        return false;
    }
    const uint32_t s = s_mark_data[set->sprites + spr];
    out->u = (uint8_t)(s & 0xFF);
    out->v = (uint8_t)((s >> 8) & 0xFF);
    out->w = (uint8_t)((s >> 16) & 0xFF);
    out->h = (uint8_t)((s >> 24) & 0xFF);
    out->x = (2.0f * px - scroll_x + 160.0f) / scale;
    out->y = ((float)py - scroll_y + 120.0f) / scale;
    out->sw = (float)out->w / scale;
    out->sh = (float)out->h / (2.0f * scale);
    out->shade = shade;
    return true;
}

int sh_papermap_marks(const ShSavegame *save, int map_idx, float scroll_x, float scroll_y, float scale,
                      ShMapSprite *out, int max)
{
    const MarkSet *set = mark_set(map_idx);
    if (!set || !save || !out || scale <= 0.0f) {
        return 0;
    }
    int n = 0;
    const int count = (int)(set->flag_last - set->flag_first) >> 1;
    for (int i = 0; i < count && n < max; i++) {
        for (int j = 0; j < 2 && n < max; j++) {
            if (!sh_save_event_get(save, set->flag_first + (unsigned)(i * 2) + (unsigned)j + 1u)) {
                continue;
            }
            if (place(set, i, j, 1.0f, scroll_x, scroll_y, scale, &out[n])) {
                n++;
            }
        }
    }
    return n;
}

bool sh_papermap_mark_flag(int map_idx, unsigned flag, float shade, float scroll_x, float scroll_y, float scale,
                           ShMapSprite *out)
{
    const MarkSet *set = mark_set(map_idx);
    if (!set || !out || scale <= 0.0f || flag < set->flag_first) {
        return false;
    }
    return place(set, (int)(flag - set->flag_first) >> 1, (int)(flag & 1u), shade, scroll_x, scroll_y, scale, out);
}

/* MapCoordIdxGet — bodyprog_mapscreen_80066D90.c */
static int map_coord_idx_get(float coord_m, int bias, int shift, int offset)
{
    int c = (int)(coord_m * 4096.0f);
    if (c < 0) {
        c += bias;
    }
    return (c >> shift) + offset;
}

/* func_80067914 PaperMapIdx_OldTown / MapIdx_MAP2_S00 (cafe + Levin St chunks). */
/* func_80067914 PaperMapIdx_OldTown fixed cells (cellX, cellZ, mapX, mapZ, angle add in turns). */
typedef struct {
    int8_t cx, cz;
    int16_t mx, mz;
    float turn;
} PaperCell;

static const PaperCell s_cells_map2_s00[] = {
    {-5, 9, 43, -34, 0.0f},   {-5, 7, -11, -19, 0.0f},  {-3, 5, -11, -19, 0.0f}, {-3, 6, -11, -19, 0.0f},
    {1, 9, -63, 73, 0.0f},    {3, 9, -63, -28, 0.0f},   {2, 7, -58, 73, 0.0f},   {3, 7, -58, 73, 0.0f},
    {2, 5, -58, -28, 0.0f},   {3, 5, -58, -28, 0.0f},   {5, 5, -112, 51, 0.0f},  {-2, 8, 116, 47, 0.0f},
    {5, 8, 116, 47, 0.0f},
};
static const PaperCell s_cells_map0_s02[] = {
    {-3, 0, -103, 103, 0.0f}, {-4, 0, 36, -76, 0.25f}, {-5, 0, 62, 50, 0.5f}, {-4, 1, -58, -28, 0.0f}, /* JAP1+ fix */
};

static bool paper_cell_find(const PaperCell *tab, int n, int cx, int cz, int *mx, int *mz, float *turn)
{
    for (int i = 0; i < n; i++) {
        if (tab[i].cx == cx && tab[i].cz == cz) {
            *mx = tab[i].mx;
            *mz = tab[i].mz;
            *turn = tab[i].turn;
            return true;
        }
    }
    return false;
}

bool sh_papermap_player_map2_s00(int map_idx, float ps1_x, float ps1_z, float ps1_yaw, float scroll_x, float scroll_y,
                                 float scale, float *out_x, float *out_y, float *out_turn_add)
{
    if (!out_x || !out_y || scale <= 0.0f) {
        return false;
    }
    if (out_turn_add) {
        *out_turn_add = 0.0f;
    }
    int cell_x;
    int cell_z;
    if (ps1_x <= 0.0f) {
        cell_x = (int)((ps1_x - 40.0f) / 40.0f);
    } else {
        cell_x = (int)(ps1_x / 40.0f);
    }
    if (ps1_z <= 0.0f) {
        cell_z = (int)((ps1_z - 40.0f) / 40.0f);
    } else {
        cell_z = (int)(ps1_z / 40.0f);
    }
    int map_x;
    int map_z;
    float turn = 0.0f;
    if (map_idx == 2) { /* MapIdx_MAP0_S02 */
        if (!paper_cell_find(s_cells_map0_s02, 4, cell_x, cell_z, &map_x, &map_z, &turn)) {
            return false;
        }
    } else if (cell_z < 4 || (cell_z < 6 && cell_x >= -1 && cell_x < 1)) {
        map_x = map_coord_idx_get(ps1_x, 8191, 13, 19);
        map_z = map_coord_idx_get(-ps1_z, 8191, 13, 1);
    } else if (!paper_cell_find(s_cells_map2_s00, (int)(sizeof(s_cells_map2_s00) / sizeof(s_cells_map2_s00[0])),
                                cell_x, cell_z, &map_x, &map_z, &turn)) {
        return false;
    }
    if (out_turn_add) {
        *out_turn_add = turn;
    }
    /* Same 0..320 / 0..240 space as papermap marks (not PS1 centred OT). */
    const float sc = scale < 0.05f ? 1.0f : scale;
    *out_x = ((float)map_x - scroll_x + 160.0f) / sc;
    *out_y = ((float)map_z - scroll_y + 120.0f) / sc;
    /* func_80067914: interlaced map doubles player wedge Y (marks use 2*py in place()). */
    *out_y *= 2.0f;
    (void)ps1_yaw;
    return true;
}

void sh_papermap_boxes(float progress, const float start[4], const float end[4], float out[SH_MAP_MAX_BOXES][4])
{
    for (int i = 0; i < SH_MAP_MAX_BOXES; i++) {
        float t;
        if (progress > 0.0f) {
            t = (progress - (float)i * 0.5f / 5.0f) / 0.5f;
            t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        } else {
            t = -progress;
        }
        const float k = 1.0f - t;
        const float x0 = start[0] * k + end[0] * t;
        const float y0 = start[1] * k + end[1] * t;
        const float x1 = x0 + start[2] * k + end[2] * t;
        const float y1 = y0 + start[3] * k + end[3] * t;
        out[i][0] = x0 + 160.0f;
        out[i][1] = (y0 + 240.0f) * 0.5f;
        out[i][2] = x1 + 160.0f;
        out[i][3] = (y1 + 240.0f) * 0.5f;
    }
}
