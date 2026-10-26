#include "sh3ds/fixed.h"
#include <inttypes.h>
#include <stdio.h>

int main(void)
{
    char op;
    int32_t a, b;
    while (scanf(" %c %" SCNd32, &op, &a) == 2) {
        if (op == 'a') {
            if (scanf(" %" SCNd32, &b) != 1) { return 1; }
            printf("%" PRId32 "\n", sh_fixed_atan2(a, b));
        } else if (op == 's' && a >= 0) {
            printf("%" PRIu32 "\n", sh_fixed_sqrt0((uint32_t)a));
        } else { return 1; }
    }
    return 0;
}
