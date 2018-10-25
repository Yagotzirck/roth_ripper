#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pic_utils.h"
#include "tga_utils.h"
#include "types.h"

extern char path[];

/* local functions declarations */
static void imgFlipMirror(BYTE dest[], const BYTE src[], WORD width, WORD height);
static void swapDWord(DWORD *x, DWORD *y);

/* functions definitions */
void pic_handler(BYTE *rawData, DWORD width, DWORD height, isFlipped_t isFlipped){
    FILE *tga_fp;

    BYTE *bufToShrink, *bufToWrite;

    BYTE *shrunkBuf;
    BYTE *flippedBuf = NULL;

    int shrunkSize;
    WORD CMapLen;

    enum tgaImageType imgType;

    DWORD rawBytesToWrite;


    if((tga_fp = fopen(path, "wb")) == NULL){
        fprintf(stderr, "Couldn't create %s\n", path);
        exit(EXIT_FAILURE);
    }

    if(isFlipped){
        if((flippedBuf = malloc(width * height)) == NULL){
            fprintf(stderr, "Couldn't allocate %u bytes for the flipped image buffer\n", width * height);
            exit(EXIT_FAILURE);
        }
        bufToShrink = flippedBuf;
        imgFlipMirror(flippedBuf, rawData, width, height);
        swapDWord(&width, &height);
    }
    else
        bufToShrink = rawData;

    // in the worst case scenario the RLE compression results in twice the size of the original data
    if((shrunkBuf = malloc(width * height * 2)) == NULL){
        fprintf(stderr, "Couldn't allocate %u bytes for the shrunk image buffer\n", width * height * 2);
        exit(EXIT_FAILURE);
    }

    shrunkSize = shrink_tga(shrunkBuf, bufToShrink, width * height, &CMapLen);

    /* RLE compression resulted in increased data size */
    if(shrunkSize == -1){
        bufToWrite = bufToShrink;
        imgType = IMGTYPE_COLORMAPPED;
        rawBytesToWrite = width * height;
    }
    /* RLE compression worked fine; save the compressed data */
    else{
        bufToWrite = shrunkBuf;
        imgType = IMGTYPE_COLORMAPPED_RLE;
        rawBytesToWrite = shrunkSize;
    }

    /* set and write the tga header */
    set_tga_hdr(PALETTED, imgType, CMapLen, 32, width, height, 8, ATTRIB_BITS | TOP_LEFT);
    write_tga_hdr(tga_fp);


    /* write the tga palette */
    write_shrunk_tga_pal(tga_fp);

    /* write the image data */
    fwrite(bufToWrite, 1, rawBytesToWrite, tga_fp);

    fclose(tga_fp);
    free(shrunkBuf);
    free(flippedBuf);
}

void initPalette(void){
    FILE *in_fp;
    const long paletteOffset = 0x8934;

    static DWORD isLoaded = 0;  // boolean variable to check whether the palette has already been loaded
    static rgb_palette_t rgb_palette[256];

    if(!isLoaded){
        if((in_fp = fopen("DEMO.DAS", "rb")) == NULL){
            fputs("Couldn't open DEMO.DAS to get the palette\n", stderr);
            exit(EXIT_FAILURE);
        }

        fseek(in_fp, paletteOffset, SEEK_SET);
        fread(rgb_palette, sizeof(rgb_palette[0]), sizeof(rgb_palette) / sizeof(rgb_palette[0]), in_fp);
        fclose(in_fp);
        isLoaded = 1;
    }

    rgb6bppToTgaPal(rgb_palette);
}


static void imgFlipMirror(BYTE dest[], const BYTE src[], WORD width, WORD height){
    unsigned int x, y;

    for(x = 0; x < height; ++x)
        for(y = 0; y < width; ++y)
            dest[y*height + x] = src[x*width + y];
}

static void swapDWord(DWORD *x, DWORD *y){
    DWORD temp = *x;
    *x = *y;
    *y = temp;
}
