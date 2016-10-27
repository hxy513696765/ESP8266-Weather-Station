#ifndef __ICON_H__
#define __ICON_H__
#include "c_types.h"

#define FONT_NUM 95//69//
#define BIG_NUM 15
struct Font_dat_16X8
{
    const char string;
    const uint8_t Font_tab[16];
};
struct Font_dat_32X16
{
    const char string;
    const uint8_t Font_tab[64];
};
extern const struct Font_dat_16X8 word_string[FONT_NUM];
extern const struct Font_dat_32X16 Big_font_tab[BIG_NUM];
extern const uint8_t lcd_show[];
extern const uint8_t net_erro[];
#endif
