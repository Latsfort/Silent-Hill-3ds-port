#pragma once
#include <stdint.h>
/* Generated from bodyprog/map/map.h. */
typedef struct
{
    union
    {
        int32_t vx_0; 
        struct
        {
            int16_t field_0;
            int16_t field_2;
        } s_0;
        struct
        {
            uint16_t field_0;
            uint16_t field_2;
        } s_1;
    } field_0;
    union
    {
        int32_t vz_4;
        struct
        {
            int16_t field_0;
            int16_t field_2;
        } s_0;
    } field_4;
    int16_t vy_8;
    uint8_t    field_A;
    uint8_t    field_B; 
    union
    {
        int32_t field_0;
        struct
        {
            int16_t field_0; 
            int16_t field_2; 
        } s_0;
        struct
        {
            uint8_t field_0;
            uint8_t field_1;
            uint8_t field_2;
            uint8_t field_3;
        } s_1;
        struct
        {
            uint16_t field_0;
            uint16_t   field_2;
        } s_2;
    } field_C;
    union
    {
        uint32_t field_0; 
        struct
        {
            int16_t field_0;
            int16_t field_2;
        } s_0;
        struct
        {
            int16_t field_0;
            uint8_t  field_2;
            uint8_t  field_3;
        } s_1;
        struct
        {
            uint8_t field_0;
            uint8_t field_1;
            uint8_t field_2;
            uint8_t field_3;
        } s_2;
        struct
        {
            uint16_t field_0;
            uint16_t field_2;
        } s_3;
    } field_10;
} ShSchoolBossParticleSlot;
