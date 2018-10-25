#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "icons_backdrop_extract.h"
#include "pic_utils.h"

extern char *fileNamePtr;

typedef struct rleImgHdr_s{
    WORD    imgType;
    WORD    unk;
    WORD    width;
    WORD    height;
}rleImgHdr_t;


/*** local functions declarations ***/
static void decodeRleImg(BYTE spriteLump[], unsigned numImage);


void BACKDROPextract(FILE *in_fp){
    BYTE *BACKDROP_Data;
    unsigned backdropDataSize;


    // load the file into memory
    fseek(in_fp, 0, SEEK_END);

    backdropDataSize = ftell(in_fp);

    if((BACKDROP_Data = malloc(backdropDataSize)) == NULL){
        fprintf(stderr, "Couldn't allocate %u bytes for BACKDROP.RAW\n", backdropDataSize);
        exit(EXIT_FAILURE);
    }


    rewind(in_fp);

    fread(BACKDROP_Data, 1, backdropDataSize, in_fp);

    initPalette();

    // start decoding
    decodeRleImg(BACKDROP_Data, 1);

    free(BACKDROP_Data);
}

void ICONSextract(FILE *in_fp){
    typedef struct iconsHdr_s{
        DWORD offset;
        DWORD size;
    }iconsHdr_t;

    BYTE *ICONS_Data;
    unsigned iconsDataSize;

    unsigned i;
    iconsHdr_t *iconsHdr;
    unsigned numEntries;


    // load the file into memory
    fseek(in_fp, 0, SEEK_END);

    iconsDataSize = ftell(in_fp);

    if((ICONS_Data = malloc(iconsDataSize)) == NULL){
        fprintf(stderr, "Couldn't allocate %u bytes for ICONS.ALL\n", iconsDataSize);
        exit(EXIT_FAILURE);
    }


    rewind(in_fp);

    fread(ICONS_Data, 1, iconsDataSize, in_fp);

    initPalette();

    // various initializations
    iconsHdr = ICONS_Data;
    numEntries = iconsHdr[0].offset / sizeof(iconsHdr_t);

    // start decoding
    for(i = 0; i < numEntries; ++i)
        decodeRleImg(ICONS_Data + iconsHdr[i].offset, i + 1);

    free(ICONS_Data);
}

/*** local functions definitions ***/
static void decodeRleImg(BYTE spriteLump[], unsigned numImage){
    rleImgHdr_t *rleImgHdr = spriteLump;

    BYTE *decodedSpriteBuf;

    unsigned srcIdx = sizeof(*rleImgHdr), destIdx = 0;

    unsigned decodedSpriteSize = rleImgHdr->width * rleImgHdr->height;

    if((decodedSpriteBuf = malloc(decodedSpriteSize)) == NULL){
        fprintf(stderr, "Couldn't allocate %u bytes for sprite # %u\n", decodedSpriteSize, numImage);
        exit(EXIT_FAILURE);
    }

    while(destIdx < decodedSpriteSize){
        // a byte with the top 4 bits set indicates a RLE byte; the low 4 bits indicate how many times to repeat the next byte
        if(spriteLump[srcIdx] > 0xF0){
            BYTE pixelCount = spriteLump[srcIdx++] & 0x0F;
            memset(decodedSpriteBuf + destIdx, spriteLump[srcIdx++], pixelCount);
            destIdx += pixelCount;
        }
        else
            decodedSpriteBuf[destIdx++] = spriteLump[srcIdx++];
    }

    sprintf(fileNamePtr, "%.3u.tga", numImage);
    pic_handler(decodedSpriteBuf, rleImgHdr->width, rleImgHdr->height, NOT_FLIPPED);

    free(decodedSpriteBuf);
}
