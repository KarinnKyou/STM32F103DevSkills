#ifndef __FONT6X8_H
#define __FONT6X8_H

/* ASCII 0x20..0x7A (space..z), 6 columns x 8 rows, column-major (page)
 * layout, plus one extra glyph for horizontal lines at index 91.
 * Generated from the vendor OLED code table by tools/gen_font6x8.py. */
#define FONT6X8_COUNT   92
#define FONT6X8_FIRST   0x20

extern const unsigned char F6x8[FONT6X8_COUNT][6];

#endif /* __FONT6X8_H */
