#include "sh3ds/savefile.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#define SAVEFILE_CAPACITY 512
#define SAVEFILE_PATH_CAPACITY 512

typedef struct SaveCodec {
    uint8_t bytes[SAVEFILE_CAPACITY];
    size_t position;
    bool reading;
} SaveCodec;

static uint32_t save_checksum(const uint8_t *data, size_t size)
{
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

static uint32_t save_integer(SaveCodec *codec, uint32_t value, unsigned bytes)
{
    uint32_t result = 0;
    for (unsigned i = 0; i < bytes; i++) {
        if (codec->reading) {
            result |= (uint32_t)codec->bytes[codec->position++] << (i * 8);
        } else {
            codec->bytes[codec->position++] = (uint8_t)(value >> (i * 8));
        }
    }
    return codec->reading ? result : value;
}

static bool save_valid(const ShAutosave *snapshot)
{
    const ShSavegame *save = &snapshot->save;
    if (!snapshot->valid || save->map_idx < 0 || save->map_room_idx < 0 ||
        save->health < 0 || save->health > SH_Q12(100.0f) ||
        save->health_saturation < 0 || save->health_saturation > SH_Q12(300.0f) ||
        save->inv_slot_count > SH_INV_SLOT_MAX || save->gameplay_timer < 0 ||
        save->walk_distance < 0 || save->run_distance < 0 ||
        (save->equipped_weapon && sh_save_item_count(save, save->equipped_weapon) == 0)) {
        return false;
    }
    return true;
}

/* The port format is versioned explicitly; compiler padding and bool layout are never written. */
static void save_payload(SaveCodec *codec, ShAutosave *snapshot)
{
    ShSavegame *save = &snapshot->save;
#define FIELD(value, size) ((value) = save_integer(codec, (uint32_t)(value), (size)))
    for (unsigned i = 0; i < SH_INV_SLOT_MAX; i++) {
        FIELD(save->items[i].id, 1);
        FIELD(save->items[i].count, 1);
        FIELD(save->items[i].unk0, 1);
        FIELD(save->items[i].unk1, 1);
    }
    for (unsigned i = 0; i < SH_EVENT_FLAG_WORDS; i++) { FIELD(save->event_flags[i], 4); }
    FIELD(save->health, 4);
    FIELD(save->health_saturation, 4);
    FIELD(save->equipped_weapon, 1);
    FIELD(save->item_toggle_flags, 1);
    FIELD(save->inv_slot_count, 1);
    FIELD(save->picked_up_item_count, 2);
    FIELD(save->has_map, 1);
    FIELD(save->paper_map_flags, 4);
    FIELD(save->gameplay_timer, 4);
    FIELD(save->walk_distance, 4);
    FIELD(save->run_distance, 4);
    FIELD(save->map_idx, 1);
    FIELD(save->map_room_idx, 1);
    FIELD(save->location_id, 1);
    FIELD(save->map_enemy_slots, 4);
    for (unsigned i = 0; i < 3; i++) { FIELD(snapshot->position[i], 4); }
    FIELD(snapshot->yaw, 2);
#undef FIELD
}

static ShSavefileResult save_read_single(const char *path, ShAutosave *snapshot)
{
    FILE *file = fopen(path, "rb");
    if (!file) { return errno == ENOENT ? SH_SAVEFILE_MISSING : SH_SAVEFILE_IO_ERROR; }
    SaveCodec codec = {0};
    ShAutosave loaded = {0};
    const size_t size = fread(codec.bytes, 1, sizeof(codec.bytes), file);
    const bool io_error = ferror(file) != 0;
    const int extra = fgetc(file);
    const int closed = fclose(file);
    if (io_error || closed != 0) { return SH_SAVEFILE_IO_ERROR; }
    if (size < 12 || extra != EOF || memcmp(codec.bytes, "SH3DSV01", 8) != 0) {
        return SH_SAVEFILE_INVALID;
    }
    codec.position = 8;
    codec.reading = true;
    save_payload(&codec, &loaded);
    const size_t payload_end = codec.position;
    const uint32_t checksum = save_integer(&codec, 0, 4);
    loaded.valid = true;
    if (size != codec.position || checksum != save_checksum(codec.bytes, payload_end) || !save_valid(&loaded)) {
        return SH_SAVEFILE_INVALID;
    }
    *snapshot = loaded;
    return SH_SAVEFILE_OK;
}

static bool save_path(const char *path, const char *suffix, char output[SAVEFILE_PATH_CAPACITY])
{
    const int length = snprintf(output, SAVEFILE_PATH_CAPACITY, "%s%s", path, suffix);
    return length >= 0 && length < SAVEFILE_PATH_CAPACITY;
}

ShSavefileResult sh_savefile_read(const char *path, ShAutosave *snapshot)
{
    if (!path || !snapshot) { return SH_SAVEFILE_INVALID; }
    char backup[SAVEFILE_PATH_CAPACITY];
    if (!save_path(path, ".bak", backup)) { return SH_SAVEFILE_INVALID; }
    const ShSavefileResult result = save_read_single(path, snapshot);
    if (result == SH_SAVEFILE_OK) { return result; }
    if (save_read_single(backup, snapshot) == SH_SAVEFILE_OK) { return SH_SAVEFILE_OK; }
    return result;
}

ShSavefileResult sh_savefile_write(const char *path, const ShAutosave *snapshot)
{
    if (!path || !snapshot || !save_valid(snapshot)) { return SH_SAVEFILE_INVALID; }
    char temporary[SAVEFILE_PATH_CAPACITY], backup[SAVEFILE_PATH_CAPACITY];
    if (!save_path(path, ".tmp", temporary) || !save_path(path, ".bak", backup)) {
        return SH_SAVEFILE_INVALID;
    }
    SaveCodec codec = {0};
    ShAutosave copy = *snapshot;
    memcpy(codec.bytes, "SH3DSV01", 8);
    codec.position = 8;
    save_payload(&codec, &copy);
    const uint32_t checksum = save_checksum(codec.bytes, codec.position);
    save_integer(&codec, checksum, 4);
    FILE *file = fopen(temporary, "wb");
    if (!file) { return SH_SAVEFILE_IO_ERROR; }
    bool written = fwrite(codec.bytes, 1, codec.position, file) == codec.position;
    if (fflush(file) != 0) { written = false; }
    if (fclose(file) != 0) { written = false; }
    if (!written) {
        remove(temporary);
        return SH_SAVEFILE_IO_ERROR;
    }
    ShAutosave previous;
    const bool have_previous = save_read_single(path, &previous) == SH_SAVEFILE_OK;
    if (have_previous) {
        if (remove(backup) != 0 && errno != ENOENT) {
            remove(temporary);
            return SH_SAVEFILE_IO_ERROR;
        }
        if (rename(path, backup) != 0) {
            remove(temporary);
            return SH_SAVEFILE_IO_ERROR;
        }
    } else if (remove(path) != 0 && errno != ENOENT) {
        remove(temporary);
        return SH_SAVEFILE_IO_ERROR;
    }
    if (rename(temporary, path) != 0) {
        if (have_previous) { rename(backup, path); }
        remove(temporary);
        return SH_SAVEFILE_IO_ERROR;
    }
    return SH_SAVEFILE_OK;
}
