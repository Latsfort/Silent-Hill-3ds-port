#include "spu.h"

#include <string.h>

/* PS1 SPU: PS-ADPCM voices, hardware ADSR, reverb unit and CD input, plus the libspu calls
 * used by libsd. Timing is per 44.1 kHz output sample, as on the console. */

extern u32 _spu_rev_param[];
extern s32 sd_reverb_area_size[];

enum { ENV_OFF, ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE };

typedef struct SpuVoice {
    s16 vol_l;
    s16 vol_r;
    u16 pitch;
    u32 start;  /* byte address */
    u32 repeat; /* byte address */
    u16 adsr1;
    u16 adsr2;
    bool reverb;

    u32 cur;     /* byte address of the decoded block */
    u8 flags;    /* flags of the decoded block */
    u32 counter; /* 4.12 position inside the block */
    s16 buf[3 + 28];
    s16 old;
    s16 older;

    int phase;
    s32 level;
    s32 env_counter;
} SpuVoice;

typedef struct SpuReverb {
    u16 r[32];
    u32 base; /* byte address */
    u32 cur;  /* byte address */
    s16 depth_l;
    s16 depth_r;
    s16 out_l;
    s16 out_r;
    s32 in_l;
    s32 in_r;
    bool odd;
} SpuReverb;

static u8 s_ram[SPU_RAM_SIZE] __attribute__((aligned(4)));
static SpuVoice s_voice[SPU_VOICES];
static u32 s_keyed;
static SpuReverb s_rev;
static bool s_rev_on;
static s16 s_mvol_l = 0x3FFF;
static s16 s_mvol_r = 0x3FFF;
static s16 s_cd_vol_l;
static s16 s_cd_vol_r;
static bool s_cd_mix;
static bool s_cd_rev;
static u32 s_transfer_addr;
static SpuCdSource s_cd_source;
static s32 s_rev_mode;

static const s32 k_f0[5] = {0, 60, 115, 98, 122};
static const s32 k_f1[5] = {0, 0, -52, -55, -60};

static inline s16 clamp16(s32 v)
{
    return (s16)(v < -32768 ? -32768 : (v > 32767 ? 32767 : v));
}

u8 *spu_ram(void)
{
    return s_ram;
}

void spu_set_cd_source(SpuCdSource src)
{
    s_cd_source = src;
}

void spu_reset(void)
{
    memset(s_voice, 0, sizeof(s_voice));
    memset(&s_rev, 0, sizeof(s_rev));
    s_keyed = 0;
    s_rev_on = false;
    s_mvol_l = s_mvol_r = 0x3FFF;
    s_cd_vol_l = s_cd_vol_r = 0;
    s_cd_mix = false;
    s_cd_rev = false;
    s_rev_mode = 0;
}

/* ---- voices -------------------------------------------------------------- */

static void decode_block(SpuVoice *v)
{
    const u8 *b = &s_ram[v->cur & (SPU_RAM_SIZE - 1) & ~15u];
    int shift = b[0] & 0x0F;
    int filter = (b[0] >> 4) & 0x07;
    if (shift > 12) {
        shift = 9;
    }
    if (filter > 4) {
        filter = 4;
    }
    v->flags = b[1];
    if (v->flags & 4) {
        v->repeat = v->cur;
    }
    v->buf[0] = v->buf[28];
    v->buf[1] = v->buf[29];
    v->buf[2] = v->buf[30];
    s32 old = v->old;
    s32 older = v->older;
    for (int i = 0; i < 28; i++) {
        const int nib = (b[2 + (i >> 1)] >> ((i & 1) * 4)) & 0x0F;
        s32 s = (s32)((s16)(nib << 12)) >> shift;
        s += (old * k_f0[filter] + older * k_f1[filter] + 32) >> 6;
        s = clamp16(s);
        older = old;
        old = s;
        v->buf[3 + i] = (s16)s;
    }
    v->old = (s16)old;
    v->older = (s16)older;
}

static void env_tick(SpuVoice *v)
{
    int rate;
    bool exp;
    bool dec;
    switch (v->phase) {
    case ENV_ATTACK:
        rate = (v->adsr1 >> 8) & 0x7F;
        exp = (v->adsr1 >> 15) & 1;
        dec = false;
        break;
    case ENV_DECAY:
        rate = ((v->adsr1 >> 4) & 0x0F) << 2;
        exp = true;
        dec = true;
        break;
    case ENV_SUSTAIN:
        rate = (v->adsr2 >> 6) & 0x7F;
        exp = (v->adsr2 >> 15) & 1;
        dec = (v->adsr2 >> 14) & 1;
        break;
    case ENV_RELEASE:
        rate = (v->adsr2 & 0x1F) << 2;
        exp = (v->adsr2 >> 5) & 1;
        dec = true;
        break;
    default:
        return;
    }

    const int shift = rate >> 2;
    s32 step = dec ? (-8 + (rate & 3)) : (7 - (rate & 3));
    s32 cycles = 1 << (shift > 11 ? shift - 11 : 0);
    step <<= (shift < 11 ? 11 - shift : 0);
    if (exp && !dec && v->level > 0x6000) {
        cycles <<= 2;
    }
    if (exp && dec) {
        step = (step * v->level) >> 15;
    }

    if (++v->env_counter < cycles) {
        return;
    }
    v->env_counter = 0;
    v->level += step;
    if (v->level < 0) {
        v->level = 0;
    }
    if (v->level > 0x7FFF) {
        v->level = 0x7FFF;
    }

    switch (v->phase) {
    case ENV_ATTACK:
        if (v->level >= 0x7FFF) {
            v->phase = ENV_DECAY;
        }
        break;
    case ENV_DECAY:
        if (v->level <= (((v->adsr1 & 0x0F) + 1) << 11)) {
            v->phase = ENV_SUSTAIN;
        }
        break;
    case ENV_RELEASE:
        if (v->level <= 0) {
            v->phase = ENV_OFF;
        }
        break;
    default:
        break;
    }
}

static void voice_key_on(SpuVoice *v)
{
    v->cur = v->start;
    v->counter = 0;
    v->old = v->older = 0;
    memset(v->buf, 0, sizeof(v->buf));
    v->level = 0;
    v->env_counter = 0;
    v->phase = ENV_ATTACK;
    decode_block(v);
}

static inline s32 voice_sample(SpuVoice *v)
{
    const u32 idx = v->counter >> 12;
    const s32 frac = (s32)(v->counter & 0xFFF);
    const s32 a = v->buf[2 + idx];
    const s32 b = v->buf[3 + idx];
    s32 s = a + (((b - a) * frac) >> 12);

    u32 step = v->pitch > 0x4000 ? 0x4000 : v->pitch;
    v->counter += step;
    while ((v->counter >> 12) >= 28) {
        v->counter -= 28 << 12;
        if (v->flags & 1) {
            v->cur = v->repeat;
            if (!(v->flags & 2)) {
                v->phase = ENV_RELEASE;
                v->level = 0;
            }
        } else {
            v->cur += 16;
        }
        decode_block(v);
    }

    env_tick(v);
    return (s * v->level) >> 15;
}

/* ---- reverb -------------------------------------------------------------- */

enum {
    R_dAPF1, R_dAPF2, R_vIIR, R_vCOMB1, R_vCOMB2, R_vCOMB3, R_vCOMB4, R_vWALL,
    R_vAPF1, R_vAPF2, R_mLSAME, R_mRSAME, R_mLCOMB1, R_mRCOMB1, R_mLCOMB2, R_mRCOMB2,
    R_dLSAME, R_dRSAME, R_mLDIFF, R_mRDIFF, R_mLCOMB3, R_mRCOMB3, R_mLCOMB4, R_mRCOMB4,
    R_dLDIFF, R_dRDIFF, R_mLAPF1, R_mRAPF1, R_mLAPF2, R_mRAPF2, R_vLIN, R_vRIN
};

static inline s16 *rev_at(s32 offset_bytes)
{
    const u32 size = SPU_RAM_SIZE - s_rev.base;
    u32 rel = (s_rev.cur - s_rev.base) + (u32)offset_bytes;
    rel %= size;
    return (s16 *)&s_ram[(s_rev.base + rel) & ~1u];
}

#define RV(x) ((s32)(s16)s_rev.r[R_##x])
#define RA(x) ((s32)s_rev.r[R_##x] * 8)

static void reverb_tick(s32 in_l, s32 in_r, s32 *out_l, s32 *out_r)
{
    const s32 lin = (in_l * RV(vLIN)) >> 15;
    const s32 rin = (in_r * RV(vRIN)) >> 15;

    s32 v;
    v = ((lin + ((*rev_at(RA(dLSAME)) * RV(vWALL)) >> 15) - *rev_at(RA(mLSAME) - 2)) * RV(vIIR) >> 15) +
        *rev_at(RA(mLSAME) - 2);
    *rev_at(RA(mLSAME)) = clamp16(v);
    v = ((rin + ((*rev_at(RA(dRSAME)) * RV(vWALL)) >> 15) - *rev_at(RA(mRSAME) - 2)) * RV(vIIR) >> 15) +
        *rev_at(RA(mRSAME) - 2);
    *rev_at(RA(mRSAME)) = clamp16(v);
    v = ((lin + ((*rev_at(RA(dRDIFF)) * RV(vWALL)) >> 15) - *rev_at(RA(mLDIFF) - 2)) * RV(vIIR) >> 15) +
        *rev_at(RA(mLDIFF) - 2);
    *rev_at(RA(mLDIFF)) = clamp16(v);
    v = ((rin + ((*rev_at(RA(dLDIFF)) * RV(vWALL)) >> 15) - *rev_at(RA(mRDIFF) - 2)) * RV(vIIR) >> 15) +
        *rev_at(RA(mRDIFF) - 2);
    *rev_at(RA(mRDIFF)) = clamp16(v);

    s32 l = (RV(vCOMB1) * *rev_at(RA(mLCOMB1)) + RV(vCOMB2) * *rev_at(RA(mLCOMB2)) +
             RV(vCOMB3) * *rev_at(RA(mLCOMB3)) + RV(vCOMB4) * *rev_at(RA(mLCOMB4))) >> 15;
    s32 r = (RV(vCOMB1) * *rev_at(RA(mRCOMB1)) + RV(vCOMB2) * *rev_at(RA(mRCOMB2)) +
             RV(vCOMB3) * *rev_at(RA(mRCOMB3)) + RV(vCOMB4) * *rev_at(RA(mRCOMB4))) >> 15;

    s32 t = *rev_at(RA(mLAPF1) - RA(dAPF1));
    l = clamp16(l - ((RV(vAPF1) * t) >> 15));
    *rev_at(RA(mLAPF1)) = (s16)l;
    l = ((l * RV(vAPF1)) >> 15) + t;
    t = *rev_at(RA(mRAPF1) - RA(dAPF1));
    r = clamp16(r - ((RV(vAPF1) * t) >> 15));
    *rev_at(RA(mRAPF1)) = (s16)r;
    r = ((r * RV(vAPF1)) >> 15) + t;

    t = *rev_at(RA(mLAPF2) - RA(dAPF2));
    l = clamp16(l - ((RV(vAPF2) * t) >> 15));
    *rev_at(RA(mLAPF2)) = (s16)l;
    l = ((l * RV(vAPF2)) >> 15) + t;
    t = *rev_at(RA(mRAPF2) - RA(dAPF2));
    r = clamp16(r - ((RV(vAPF2) * t) >> 15));
    *rev_at(RA(mRAPF2)) = (s16)r;
    r = ((r * RV(vAPF2)) >> 15) + t;

    *out_l = (clamp16(l) * s_rev.depth_l) >> 15;
    *out_r = (clamp16(r) * s_rev.depth_r) >> 15;

    s_rev.cur += 2;
    if (s_rev.cur >= SPU_RAM_SIZE) {
        s_rev.cur = s_rev.base;
    }
}

/* ---- mixer --------------------------------------------------------------- */

void spu_render(s16 *out, int frames)
{
    s16 cd[2 * 256];
    while (frames > 0) {
        const int n = frames > 256 ? 256 : frames;
        if (s_cd_mix && s_cd_source) {
            s_cd_source(cd, n);
        } else {
            memset(cd, 0, (size_t)n * 2 * sizeof(s16));
        }
        for (int i = 0; i < n; i++) {
            s32 l = 0;
            s32 r = 0;
            s32 rl = 0;
            s32 rr = 0;
            for (int vi = 0; vi < SPU_VOICES; vi++) {
                SpuVoice *v = &s_voice[vi];
                if (v->phase == ENV_OFF) {
                    continue;
                }
                const s32 s = voice_sample(v);
                const s32 vl = (s * (v->vol_l * 2)) >> 15;
                const s32 vr = (s * (v->vol_r * 2)) >> 15;
                l += vl;
                r += vr;
                if (v->reverb) {
                    rl += vl;
                    rr += vr;
                }
            }
            const s32 cl = (cd[i * 2] * s_cd_vol_l) >> 15;
            const s32 cr = (cd[i * 2 + 1] * s_cd_vol_r) >> 15;
            l += cl;
            r += cr;
            if (s_cd_rev) {
                rl += cl;
                rr += cr;
            }

            if (s_rev_on && s_rev_mode != 0) {
                s_rev.in_l += rl;
                s_rev.in_r += rr;
                if (s_rev.odd) {
                    s32 ol;
                    s32 or_;
                    reverb_tick(clamp16(s_rev.in_l >> 1), clamp16(s_rev.in_r >> 1), &ol, &or_);
                    s_rev.out_l = clamp16(ol);
                    s_rev.out_r = clamp16(or_);
                    s_rev.in_l = s_rev.in_r = 0;
                }
                s_rev.odd = !s_rev.odd;
                l += s_rev.out_l;
                r += s_rev.out_r;
            }

            l = (clamp16(l) * (s_mvol_l * 2)) >> 15;
            r = (clamp16(r) * (s_mvol_r * 2)) >> 15;
            out[i * 2] = clamp16(l);
            out[i * 2 + 1] = clamp16(r);
        }
        out += n * 2;
        frames -= n;
    }
}

/* ---- libspu -------------------------------------------------------------- */

void SpuInit(void)
{
    spu_reset();
    memset(s_ram, 0, sizeof(s_ram));
}

void SpuQuit(void) {}

s32 SpuInitMalloc(s32 num, char *top)
{
    (void)num;
    (void)top;
    return 0;
}

static u16 mode_bits_attack(s32 mode)
{
    return (mode == SPU_VOICE_EXPIncN || mode == SPU_VOICE_EXPIncR) ? 0x8000 : 0;
}

static u16 mode_bits_sustain(s32 mode)
{
    switch (mode) {
    case SPU_VOICE_LINEARDecN:
    case SPU_VOICE_LINEARDecR:
        return 0x4000;
    case SPU_VOICE_EXPIncN:
    case SPU_VOICE_EXPIncR:
        return 0x8000;
    case SPU_VOICE_EXPDec:
        return 0xC000;
    default:
        return 0;
    }
}

void SpuSetVoiceAttr(SpuVoiceAttr *a)
{
    const u32 m = a->mask;
    for (int i = 0; i < SPU_VOICES; i++) {
        if (!(a->voice & (1u << i))) {
            continue;
        }
        SpuVoice *v = &s_voice[i];
        if (m == 0 || (m & SPU_VOICE_VOLL)) {
            v->vol_l = (s16)(a->volume.left & 0x7FFF);
        }
        if (m == 0 || (m & SPU_VOICE_VOLR)) {
            v->vol_r = (s16)(a->volume.right & 0x7FFF);
        }
        if (m == 0 || (m & SPU_VOICE_PITCH)) {
            v->pitch = a->pitch;
        }
        if (m == 0 || (m & SPU_VOICE_WDSA)) {
            v->start = a->addr & (SPU_RAM_SIZE - 8);
        }
        if (m == 0 || (m & SPU_VOICE_LSAX)) {
            v->repeat = a->loop_addr & (SPU_RAM_SIZE - 8);
        }
        if (m == 0 || (m & SPU_VOICE_ADSR_ADSR1)) {
            v->adsr1 = a->adsr1;
        }
        if (m == 0 || (m & SPU_VOICE_ADSR_ADSR2)) {
            v->adsr2 = a->adsr2;
        }
        if (m & SPU_VOICE_ADSR_AR) {
            v->adsr1 = (u16)((v->adsr1 & 0x00FF) | ((a->ar & 0x7F) << 8) | mode_bits_attack(a->a_mode));
        }
        if (m & SPU_VOICE_ADSR_DR) {
            v->adsr1 = (u16)((v->adsr1 & 0xFF0F) | ((a->dr & 0x0F) << 4));
        }
        if (m & SPU_VOICE_ADSR_SL) {
            v->adsr1 = (u16)((v->adsr1 & 0xFFF0) | (a->sl & 0x0F));
        }
        if (m & SPU_VOICE_ADSR_SR) {
            v->adsr2 = (u16)((v->adsr2 & 0x003F) | ((a->sr & 0x7F) << 6) | mode_bits_sustain(a->s_mode));
        }
        if (m & SPU_VOICE_ADSR_RR) {
            v->adsr2 = (u16)((v->adsr2 & 0xFFC0) | (a->rr & 0x1F) | (a->r_mode == SPU_VOICE_EXPDec ? 0x20 : 0));
        }
    }
}

void SpuGetVoiceAttr(SpuVoiceAttr *a)
{
    for (int i = 0; i < SPU_VOICES; i++) {
        if (!(a->voice & (1u << i))) {
            continue;
        }
        const SpuVoice *v = &s_voice[i];
        a->volume.left = v->vol_l;
        a->volume.right = v->vol_r;
        a->volmode.left = a->volmode.right = 0;
        a->volumex.left = v->vol_l;
        a->volumex.right = v->vol_r;
        a->pitch = v->pitch;
        a->envx = (s16)v->level;
        a->addr = v->start;
        a->loop_addr = v->repeat;
        a->adsr1 = v->adsr1;
        a->adsr2 = v->adsr2;
        a->ar = (v->adsr1 >> 8) & 0x7F;
        a->dr = (v->adsr1 >> 4) & 0x0F;
        a->sl = v->adsr1 & 0x0F;
        a->sr = (v->adsr2 >> 6) & 0x7F;
        a->rr = v->adsr2 & 0x1F;
        return;
    }
}

void SpuSetKey(s32 on_off, u32 voice_bit)
{
    for (int i = 0; i < SPU_VOICES; i++) {
        if (!(voice_bit & (1u << i))) {
            continue;
        }
        SpuVoice *v = &s_voice[i];
        if (on_off == SPU_ON) {
            s_keyed |= 1u << i;
            voice_key_on(v);
        } else {
            s_keyed &= ~(1u << i);
            if (v->phase != ENV_OFF) {
                v->phase = ENV_RELEASE;
                v->env_counter = 0;
            }
        }
    }
}

void SpuSetKeyOnWithAttr(SpuVoiceAttr *attr)
{
    SpuSetVoiceAttr(attr);
    SpuSetKey(SPU_ON, attr->voice);
}

s32 SpuGetKeyStatus(u32 voice_bit)
{
    for (int i = 0; i < SPU_VOICES; i++) {
        if (voice_bit & (1u << i)) {
            const bool env = s_voice[i].phase != ENV_OFF && s_voice[i].level > 0;
            if (s_keyed & (1u << i)) {
                return env ? SPU_ON : SPU_ON_ENV_OFF;
            }
            return env ? SPU_OFF_ENV_ON : SPU_OFF;
        }
    }
    return -1;
}

void SpuSetCommonAttr(SpuCommonAttr *a)
{
    const u32 m = a->mask;
    if (m == 0 || (m & SPU_COMMON_MVOLL)) {
        s_mvol_l = (s16)(a->mvol.left & 0x7FFF);
    }
    if (m == 0 || (m & SPU_COMMON_MVOLR)) {
        s_mvol_r = (s16)(a->mvol.right & 0x7FFF);
    }
    if (m == 0 || (m & SPU_COMMON_CDVOLL)) {
        s_cd_vol_l = a->cd.volume.left;
    }
    if (m == 0 || (m & SPU_COMMON_CDVOLR)) {
        s_cd_vol_r = a->cd.volume.right;
    }
    if (m == 0 || (m & SPU_COMMON_CDREV)) {
        s_cd_rev = a->cd.reverb != 0;
    }
    if (m == 0 || (m & SPU_COMMON_CDMIX)) {
        s_cd_mix = a->cd.mix != 0;
    }
}

s32 SpuSetReverb(s32 on_off)
{
    s_rev_on = on_off != 0;
    return on_off;
}

s32 SpuReserveReverbWorkArea(s32 on_off)
{
    return on_off;
}

s32 SpuClearReverbWorkArea(s32 mode)
{
    if (mode <= 0 || mode >= 10) {
        return 0;
    }
    const u32 size = (u32)sd_reverb_area_size[mode];
    memset(&s_ram[SPU_RAM_SIZE - size], 0, size);
    return 0;
}

s32 SpuSetReverbModeParam(SpuReverbAttr *a)
{
    if (a->mask & SPU_REV_MODE) {
        const s32 mode = a->mode & 0xFF;
        if (mode < 0 || mode >= 10) {
            return -1;
        }
        s_rev_mode = mode;
        const u32 *p = &_spu_rev_param[mode * 17 + 1];
        for (int i = 0; i < 16; i++) {
            s_rev.r[i * 2] = (u16)(p[i] & 0xFFFF);
            s_rev.r[i * 2 + 1] = (u16)(p[i] >> 16);
        }
        s_rev.base = SPU_RAM_SIZE - (u32)sd_reverb_area_size[mode];
        s_rev.cur = s_rev.base;
        if (a->mode & 0x100) {
            SpuClearReverbWorkArea(mode);
        }
        s_rev.depth_l = s_rev.depth_r = 0;
    }
    if (a->mask & SPU_REV_DEPTHL) {
        s_rev.depth_l = a->depth.left;
    }
    if (a->mask & SPU_REV_DEPTHR) {
        s_rev.depth_r = a->depth.right;
    }
    return 0;
}

u32 SpuSetReverbVoice(s32 on_off, u32 voice_bit)
{
    for (int i = 0; i < SPU_VOICES; i++) {
        if (voice_bit & (1u << i)) {
            s_voice[i].reverb = on_off != 0;
        }
    }
    return SpuGetReverbVoice();
}

u32 SpuGetReverbVoice(void)
{
    u32 bits = 0;
    for (int i = 0; i < SPU_VOICES; i++) {
        if (s_voice[i].reverb) {
            bits |= 1u << i;
        }
    }
    return bits;
}

u32 SpuSetTransferStartAddr(u32 addr)
{
    s_transfer_addr = addr & (SPU_RAM_SIZE - 8);
    return s_transfer_addr;
}

u32 SpuWrite(u8 *addr, u32 size)
{
    if (s_transfer_addr + size > SPU_RAM_SIZE) {
        size = SPU_RAM_SIZE - s_transfer_addr;
    }
    memcpy(&s_ram[s_transfer_addr], addr, size);
    s_transfer_addr += size;
    return size;
}

s32 SpuIsTransferCompleted(s32 flag)
{
    (void)flag;
    return 1;
}

s32 SpuSetTransferMode(s32 mode)
{
    return mode;
}
