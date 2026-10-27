#include "sh3ds/savefile.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char *path = "build/savefile-regression.sav";

static void cleanup(void)
{
    remove(path);
    remove("build/savefile-regression.sav.bak");
    remove("build/savefile-regression.sav.tmp");
}

static ShAutosave fixture(void)
{
    ShAutosave snapshot = {0};
    sh_save_init(&snapshot.save);
    sh_save_item_add(&snapshot.save, SH_ITEM_HANDGUN, 1);
    sh_save_item_add(&snapshot.save, SH_ITEM_HANDGUN_BULLETS, 37);
    for (unsigned i = 0; i < SH_EVENT_FLAG_WORDS; i++) {
        snapshot.save.event_flags[i] = UINT32_C(0x87654321) ^ i;
    }
    snapshot.save.equipped_weapon = SH_ITEM_HANDGUN;
    snapshot.save.health = SH_Q12(53.25f);
    snapshot.save.health_saturation = SH_Q12(12.0f);
    snapshot.save.paper_map_flags = UINT32_C(0x98765432);
    snapshot.save.map_idx = 3;
    snapshot.save.map_room_idx = 7;
    snapshot.save.location_id = 4;
    snapshot.save.has_map = true;
    snapshot.save.item_toggle_flags = 3;
    snapshot.save.picked_up_item_count = 123;
    snapshot.save.gameplay_timer = SH_Q12(1234.0f);
    snapshot.save.walk_distance = SH_Q12(43.0f);
    snapshot.save.run_distance = SH_Q12(76.0f);
    snapshot.save.map_enemy_slots = UINT32_C(0xfedc9876);
    snapshot.position[0] = SH_Q12(-252.0f);
    snapshot.position[1] = SH_Q12(-0.25f);
    snapshot.position[2] = SH_Q12(222.0f);
    snapshot.yaw = 3072;
    snapshot.valid = true;
    return snapshot;
}

int main(int argc, char **argv)
{
    ShAutosave expected = fixture();
    ShAutosave actual = {0};
    if (argc > 1 && strcmp(argv[1], "write") == 0) {
        cleanup();
        assert(sh_savefile_write(path, &expected) == SH_SAVEFILE_OK);
        puts("PASS: persistent save written by first process");
        return 0;
    }
    assert(sh_savefile_read(path, &actual) == SH_SAVEFILE_OK);
    assert(memcmp(&actual, &expected, sizeof(actual)) == 0);
    puts("PASS: fresh process restores every snapshot field");
    ShAutosave newer = expected;
    newer.save.health = SH_Q12(77.0f);
    assert(sh_savefile_write(path, &newer) == SH_SAVEFILE_OK);
    assert(sh_savefile_read(path, &actual) == SH_SAVEFILE_OK && actual.save.health == newer.save.health);

    FILE *file = fopen(path, "r+b");
    assert(file && fseek(file, 23, SEEK_SET) == 0);
    const int byte = fgetc(file);
    assert(byte != EOF && fseek(file, 23, SEEK_SET) == 0);
    assert(fputc(byte ^ 0x80, file) != EOF && fclose(file) == 0);
    assert(sh_savefile_read(path, &actual) == SH_SAVEFILE_OK);
    assert(memcmp(&actual, &expected, sizeof(actual)) == 0);
    puts("PASS: checksum corruption recovers the previous valid save");

    remove("build/savefile-regression.sav.bak");
    actual = expected;
    assert(sh_savefile_read(path, &actual) == SH_SAVEFILE_INVALID);
    assert(memcmp(&actual, &expected, sizeof(actual)) == 0);
    assert(sh_savefile_write(path, &expected) == SH_SAVEFILE_OK);
    uint8_t bytes[512];
    file = fopen(path, "rb");
    assert(file);
    size_t size = fread(bytes, 1, sizeof(bytes), file);
    assert(size > 0 && size < sizeof(bytes) && fclose(file) == 0);
    for (size_t length = 0; length < size; length++) {
        file = fopen(path, "wb");
        assert(file && fwrite(bytes, 1, length, file) == length && fclose(file) == 0);
        assert(sh_savefile_read(path, &actual) == SH_SAVEFILE_INVALID);
        assert(memcmp(&actual, &expected, sizeof(actual)) == 0);
    }
    puts("PASS: every truncated-file length is rejected without changing the destination");
    cleanup();
    assert(sh_savefile_read(path, &actual) == SH_SAVEFILE_MISSING);
    assert(sh_savefile_write(path, &expected) == SH_SAVEFILE_OK);
    assert(rename(path, "build/savefile-regression.sav.bak") == 0);
    assert(sh_savefile_read(path, &actual) == SH_SAVEFILE_OK);
    assert(memcmp(&actual, &expected, sizeof(actual)) == 0);
    newer.valid = false;
    assert(sh_savefile_write(path, &newer) == SH_SAVEFILE_INVALID);
    assert(sh_savefile_read(path, &actual) == SH_SAVEFILE_OK && actual.save.health == expected.save.health);
    assert(sh_savefile_write("build/no-save-directory/invalid.sav", &expected) == SH_SAVEFILE_IO_ERROR);
    cleanup();
    puts("PASS: interrupted rename recovery, missing files and failed writes");
    return 0;
}
