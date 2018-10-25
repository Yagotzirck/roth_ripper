#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dbase200extract.h"
#include "makedir.h"
#include "types.h"
#include "pic_utils.h"


#define     IMGTYPE_RLE         0x03
#define     IMGTYPE_ROWBGN_LEN  0x1E

extern char *fileNamePtr;

typedef struct rleImgHdr_s{
    DWORD       imgType;
    WORD        width;
    WORD        height;
}rleImgHdr_t;

typedef struct rowBgnLenImgHdr_s{
    DWORD       imgType;
    WORD        frameBufWidth;      // name is just a guess; not going to use this field anyway
    WORD        frameBufHeight;     // as above
    WORD        numSubSprites;
    WORD        currSubSpriteIdx;

    DWORD       currSubSpriteDataSize;  // including this header

    WORD        unk1;
    WORD        width;

    WORD        yOffset;    // from top to bottom
    WORD        height;

}rowBgnLenImgHdr_t;

// the following structure is used only for IMGTYPE_ROWBGN_LEN images
typedef struct imgRowDefs_s{
    WORD        startingOffset; // the offset where to start drawing; skipped bytes(pixels) stay set to 0 (transparent)
    WORD        pixelRun;       // the amount of pixels to copy at the offset specified in the field above
}imgRowDefs_t;


/** static functions declarations **/
static void decodeRleImg(BYTE spriteLump[], unsigned numImage);
static void decodeRowBgnImg(BYTE spriteLump[], unsigned numImage);
//static void rowFix(BYTE *row, DWORD pixelRun);


void dbase200extract(FILE *in_fp){
    BYTE *dbase200Data;

    unsigned dbase200DataSize;
    unsigned i = 1;
    unsigned idx = 8;
    DWORD       size;               // located at the begin of each sprite lump, before the header

    rleImgHdr_t *rleImgHdr;

    // load the file into memory
    fseek(in_fp, 0, SEEK_END);

    dbase200DataSize = ftell(in_fp);

    if((dbase200Data = malloc(dbase200DataSize)) == NULL){
        fprintf(stderr, "Couldn't allocate %u bytes for DBASE200\n", dbase200DataSize);
        exit(EXIT_FAILURE);
    }

    rewind(in_fp);

    fread(dbase200Data, 1, dbase200DataSize, in_fp);

    initPalette();

    // start decoding
    while(idx < dbase200DataSize){
        size = *((DWORD*)&dbase200Data[idx]);
        idx += sizeof(size);

        rleImgHdr = &dbase200Data[idx];

        switch(rleImgHdr->imgType){
            case IMGTYPE_RLE:
                decodeRleImg(&dbase200Data[idx], i);
                break;

            case IMGTYPE_ROWBGN_LEN:
                decodeRowBgnImg(&dbase200Data[idx], i);
                break;

        }

        idx += size;

        // align idx to a multiple of 8 if it isn't already
        idx = (idx + 7) & ~(unsigned)7;

        ++i;
    }

    free(dbase200Data);

}

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


static void decodeRowBgnImg(BYTE spriteLump[], unsigned numImage){
    rowBgnLenImgHdr_t *imgHdr = spriteLump;
    imgRowDefs_t *imgRowDefs;

    BYTE *decodedSpriteBuf;

    unsigned srcIdx;
    unsigned rowStartIdx;

    unsigned i, subSpriteIdx, numSubSprites;

    numSubSprites = imgHdr->numSubSprites + 10;

    //for(subSpriteIdx = 0; subSpriteIdx <= numSubSprites; ++subSpriteIdx){
    for(subSpriteIdx = 0; imgHdr->imgType == IMGTYPE_ROWBGN_LEN; ++subSpriteIdx){   // this iterator has proven to be more reliable than the one commented in the line above

        if((decodedSpriteBuf = calloc(imgHdr->width * imgHdr->height, 1)) == NULL){
            fprintf(stderr, "Couldn't allocate %u bytes for sprite %u_%u\n", imgHdr->width * imgHdr->height, numImage, subSpriteIdx + 1);
            exit(EXIT_FAILURE);
        }

        srcIdx = sizeof(*imgHdr) + imgHdr->height * sizeof(*imgRowDefs);
        imgRowDefs = spriteLump + sizeof(*imgHdr);
        rowStartIdx = 0;

        for(i = 0; i < imgHdr->height; ++i){
            memcpy(decodedSpriteBuf + rowStartIdx + imgRowDefs[i].startingOffset, spriteLump + srcIdx, imgRowDefs[i].pixelRun);
            srcIdx += imgRowDefs[i].pixelRun;
            rowStartIdx += imgHdr->width;
        }

        sprintf(fileNamePtr, "%.3u_%.2hu.tga", numImage, subSpriteIdx + 1);
        pic_handler(decodedSpriteBuf, imgHdr->width, imgHdr->height, NOT_FLIPPED);

        free(decodedSpriteBuf);

        spriteLump += imgHdr->currSubSpriteDataSize;
        imgHdr = spriteLump;
    }
}
