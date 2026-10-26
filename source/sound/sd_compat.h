#pragma once

/* PSY-Q / decomp environment for the libsd + sd_call sources (vendor/silent-hill-decomp). */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

/* The decompiled sources rely on PSY-Q era implicit pointer conversions. */
#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
#pragma GCC diagnostic ignored "-Wpointer-sign"
#pragma GCC diagnostic ignored "-Wmissing-braces"
#pragma GCC diagnostic ignored "-Wint-conversion"

typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef int64_t s64;

typedef s8 q0_7;
typedef u8 q0_8;
typedef s32 q19_12;
typedef s32 q20_12;
typedef s32 q23_8;
typedef s16 q3_12;

#define NO_VALUE (-1)
#define ABS(x) ((x) < 0 ? -(x) : (x))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define STATIC_ASSERT_SIZEOF(type, size)
#define STATIC_ASSERT(cond, msg)
#define Q8(x) ((s32)((x) * 256.0f))
#define Q12(x) ((s32)((x) * 4096.0f))

/* ---- libsnd ------------------------------------------------------------- */

typedef struct VabHdr {
    s32 form;
    s32 ver;
    s32 id;
    u32 fsize;
    u16 reserved0;
    u16 ps;
    u16 ts;
    u16 vs;
    u8 mvol;
    u8 pan;
    u8 attr1;
    u8 attr2;
    u32 reserved1;
} VabHdr;

typedef struct ProgAtr {
    u8 tones;
    u8 mvol;
    u8 prior;
    u8 mode;
    u8 mpan;
    s8 reserved0;
    s16 attr;
    u32 reserved1;
    u32 reserved2;
} ProgAtr;

typedef struct VagAtr {
    u8 prior;
    u8 mode;
    u8 vol;
    u8 pan;
    u8 center;
    u8 shift;
    u8 min;
    u8 max;
    u8 vibW;
    u8 vibT;
    u8 porW;
    u8 porT;
    u8 pbmin;
    u8 pbmax;
    u8 reserved1;
    u8 reserved2;
    u16 adsr1;
    u16 adsr2;
    s16 prog;
    s16 vag;
    s16 reserved[4];
} VagAtr;

#define SS_SERIAL_A 0
#define SS_SERIAL_B 1
#define SS_MIX 0
#define SS_REV 1

/* ---- libspu ------------------------------------------------------------- */

#define SPU_OFF 0
#define SPU_ON 1
#define SPU_OFF_ENV_ON 2
#define SPU_ON_ENV_OFF 3

typedef struct {
    s16 left;
    s16 right;
} SpuVolume;

typedef struct {
    u32 voice;
    u32 mask;
    SpuVolume volume;
    SpuVolume volmode;
    SpuVolume volumex;
    u16 pitch;
    u16 note;
    u16 sample_note;
    s16 envx;
    u32 addr;
    u32 loop_addr;
    s32 a_mode;
    s32 s_mode;
    s32 r_mode;
    u16 ar;
    u16 dr;
    u16 sr;
    u16 rr;
    u16 sl;
    u16 adsr1;
    u16 adsr2;
} SpuVoiceAttr;

typedef struct {
    u32 mask;
    s32 mode;
    SpuVolume depth;
    s32 delay;
    s32 feedback;
} SpuReverbAttr;

typedef struct {
    SpuVolume volume;
    s32 reverb;
    s32 mix;
} SpuExtAttr;

typedef struct {
    u32 mask;
    SpuVolume mvol;
    SpuVolume mvolmode;
    SpuVolume mvolx;
    SpuExtAttr cd;
    SpuExtAttr ext;
} SpuCommonAttr;

#define SPU_VOICE_VOLL (1 << 0)
#define SPU_VOICE_VOLR (1 << 1)
#define SPU_VOICE_VOLMODEL (1 << 2)
#define SPU_VOICE_VOLMODER (1 << 3)
#define SPU_VOICE_PITCH (1 << 4)
#define SPU_VOICE_NOTE (1 << 5)
#define SPU_VOICE_SAMPLE_NOTE (1 << 6)
#define SPU_VOICE_WDSA (1 << 7)
#define SPU_VOICE_ADSR_AMODE (1 << 8)
#define SPU_VOICE_ADSR_SMODE (1 << 9)
#define SPU_VOICE_ADSR_RMODE (1 << 10)
#define SPU_VOICE_ADSR_AR (1 << 11)
#define SPU_VOICE_ADSR_DR (1 << 12)
#define SPU_VOICE_ADSR_SR (1 << 13)
#define SPU_VOICE_ADSR_RR (1 << 14)
#define SPU_VOICE_ADSR_SL (1 << 15)
#define SPU_VOICE_LSAX (1 << 16)
#define SPU_VOICE_ADSR_ADSR1 (1 << 17)
#define SPU_VOICE_ADSR_ADSR2 (1 << 18)

#define SPU_VOICE_DIRECT 0
#define SPU_VOICE_LINEARIncN 1
#define SPU_VOICE_LINEARIncR 2
#define SPU_VOICE_LINEARDecN 3
#define SPU_VOICE_LINEARDecR 4
#define SPU_VOICE_EXPIncN 5
#define SPU_VOICE_EXPIncR 6
#define SPU_VOICE_EXPDec 7

#define SPU_COMMON_MVOLL (1 << 0)
#define SPU_COMMON_MVOLR (1 << 1)
#define SPU_COMMON_MVOLMODEL (1 << 2)
#define SPU_COMMON_MVOLMODER (1 << 3)
#define SPU_COMMON_RVOLL (1 << 4)
#define SPU_COMMON_RVOLR (1 << 5)
#define SPU_COMMON_CDVOLL (1 << 6)
#define SPU_COMMON_CDVOLR (1 << 7)
#define SPU_COMMON_CDREV (1 << 8)
#define SPU_COMMON_CDMIX (1 << 9)
#define SPU_COMMON_EXTVOLL (1 << 10)
#define SPU_COMMON_EXTVOLR (1 << 11)
#define SPU_COMMON_EXTREV (1 << 12)
#define SPU_COMMON_EXTMIX (1 << 13)

#define SPU_REV_MODE (1 << 0)
#define SPU_REV_DEPTHL (1 << 1)
#define SPU_REV_DEPTHR (1 << 2)
#define SPU_REV_DELAYTIME (1 << 3)
#define SPU_REV_FEEDBACK (1 << 4)

void SpuInit(void);
void SpuQuit(void);
s32 SpuInitMalloc(s32 num, char *top);
void SpuSetVoiceAttr(SpuVoiceAttr *attr);
void SpuGetVoiceAttr(SpuVoiceAttr *attr);
void SpuSetKey(s32 on_off, u32 voice_bit);
void SpuSetKeyOnWithAttr(SpuVoiceAttr *attr);
s32 SpuGetKeyStatus(u32 voice_bit);
void SpuSetCommonAttr(SpuCommonAttr *attr);
s32 SpuSetReverb(s32 on_off);
s32 SpuReserveReverbWorkArea(s32 on_off);
s32 SpuSetReverbModeParam(SpuReverbAttr *attr);
u32 SpuSetReverbVoice(s32 on_off, u32 voice_bit);
u32 SpuGetReverbVoice(void);
s32 SpuClearReverbWorkArea(s32 mode);
u32 SpuSetTransferStartAddr(u32 addr);
u32 SpuWrite(u8 *addr, u32 size);
s32 SpuIsTransferCompleted(s32 flag);
s32 SpuSetTransferMode(s32 mode);
