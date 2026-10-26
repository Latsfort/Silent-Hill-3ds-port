#include "sh3ds/border.h"

#include "border_rect_data.h"

/* Screen_CutsceneCameraStateUpdate: source state numbers and fallthroughs. */
void sh_border_update(ShBorder *border, int32_t delta)
{
    switch (border->state) {
    case SH_BORDER_FADEINSTART:
        border->state++;
        /* fall through */
    case SH_BORDER_FADINGIN:
        border->shade += delta;
        if (border->shade >= 4095) {
            border->shade = 4095;
            border->state++;
        }
        break;
    case SH_BORDER_FORCESHOW:
    case SH_BORDER_FADEOUTSTART:
        border->shade = 4095;
        border->state++;
        /* fall through */
    case SH_BORDER_SHOWN:
        break;
    case SH_BORDER_FADINGOUT:
        border->shade -= delta;
        if (border->shade <= 0) {
            border->shade = 0;
            border->state = SH_BORDER_RESET;
        }
        break;
    case SH_BORDER_RESET:
        border->shade = 0;
        border->state = SH_BORDER_NONE;
        break;
    case SH_BORDER_NONE:
        break;
    }
}

const ShBorderRect *sh_border_rects(int *count)
{
    *count = (int)(sizeof(s_border_rects) / sizeof(s_border_rects[0]));
    return s_border_rects;
}

/* Screen_BlackBorderDraw. */
void sh_border_colors(int32_t shade, uint8_t *outer, uint8_t *inner)
{
    *outer = (uint8_t)(shade >> 4);
    *inner = shade == 4095 ? 255 : (uint8_t)(shade >> 5);
}
