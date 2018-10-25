#ifndef PIC_UTILS_H
#define PIC_UTILS_H

#include "types.h"

typedef enum isFlipped_e{
    NOT_FLIPPED,
    FLIPPED
}isFlipped_t;


typedef struct rgb_palette_s{
    BYTE red, green, blue;
}rgb_palette_t;


/* functions declarations */
void pic_handler(BYTE *rawData, DWORD width, DWORD height, isFlipped_t isFlipped);
void initPalette(void);


#endif /* PIC_UTILS_H */
