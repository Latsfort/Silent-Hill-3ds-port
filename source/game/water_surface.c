#include "sh3ds/water_surface.h"
#include "sh3ds/fixed.h"

#define SURFACE_Q12(value) ((value) * 4096)
#include "water_surface_steps.h"
