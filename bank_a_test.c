/* Банк A: проверка дальнего вызова (до переноса VM) */
#include <stdint.h>
#include "scumm.h"
static int16_t calls;
int16_t a_test(int16_t x)
{
    ++calls;
    return (int16_t)(x * 2 + calls + (cur_page == (0120 | 04) ? 0 : 01000));
}
