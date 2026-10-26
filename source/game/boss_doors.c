#include "sh3ds/boss_doors.h"
#include "sh3ds/fixed.h"
#include <string.h>

static int32_t door_min(int32_t a, int32_t b) { return a < b ? a : b; }
static int32_t door_max(int32_t a, int32_t b) { return a > b ? a : b; }

#include "boss_door_steps.h"
