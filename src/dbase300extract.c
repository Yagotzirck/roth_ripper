#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dbase300extract.h"
#include "makedir.h"
#include "types.h"
#include "pic_utils.h"
#include "tga_utils.h"

#define PALETTE_SIZE 768


extern char path[];
extern char *fileNamePtr;

enum    fileType_e{
    FILETYPE_GDV    =   0x29111994,
    FILETYPE_HMP    =   0x4D494D48, // full header is "HMIMIDIP013195" (zero-terminated)
    FILETYPE_MIDI   =   0x6468544D, // "MThd" (no zero-termination)

    FILETYPE_IMG1   =   0x01,
    FILETYPE_IMG3   =   0x03,
    FILETYPE_IMG7   =   0x07
};



/*** local functions declarations ***/
static void saveToFile(const BYTE *data, DWORD size);
static void decodeRleImg(BYTE spriteLump[], unsigned numImage);

void dbase300extract(FILE *in_fp){
    BYTE *dbase300Data;

    unsigned dbase300DataSize;
    unsigned i = 1;
    unsigned idx = 8;
    DWORD       size;   // located at the begin of each file entry, before the entry data

    DWORD   fileType;   /* we'll check only the first 4 bytes to determine the file type; although
                        ** some entries have a string-like header longer than 4 bytes,
                        ** checking only the first 4 bytes should be enough to differentiate them
                        */


    // read the file into memory
    fseek(in_fp, 0, SEEK_END);

    dbase300DataSize = ftell(in_fp);

    if((dbase300Data = malloc(dbase300DataSize)) == NULL){
        fprintf(stderr, "Couldn't allocate %u bytes for DBASE300\n", dbase300DataSize);
        exit(EXIT_FAILURE);
    }
    rewind(in_fp);

    fread(dbase300Data, 1, dbase300DataSize, in_fp);


    // start extracting stuff
    while(idx < dbase300DataSize){
        size = *((DWORD*)&dbase300Data[idx]);
        idx += sizeof(size);

        fileType = *((DWORD*)&dbase300Data[idx]);

        switch(fileType){
            case FILETYPE_GDV:
                sprintf(fileNamePtr, "%.3u.gdv", i);
                saveToFile(&dbase300Data[idx], size);
                break;

            case FILETYPE_HMP:
                sprintf(fileNamePtr, "%.3u.hmp", i);
                saveToFile(&dbase300Data[idx], size);

                /* In some game versions, each HMP file is immediately followed by a MIDI file
                ** (not aligned to a multiple of 8, and preceded by a DWORD field indicating the MIDI file size)
                */
                if(*(DWORD *)(dbase300Data + idx + size + sizeof(size)) == FILETYPE_MIDI){
                    idx += size;
                    size = *((DWORD*)&dbase300Data[idx]);
                    idx += sizeof(size);
                    sprintf(fileNamePtr, "%.3u.midi", i);
                    saveToFile(&dbase300Data[idx], size);
                }
                break;

            case FILETYPE_MIDI:
                sprintf(fileNamePtr, "%.3u.midi", i);
                saveToFile(&dbase300Data[idx], size);
                break;

            case FILETYPE_IMG1:
            case FILETYPE_IMG3:
            case FILETYPE_IMG7:
                decodeRleImg(&dbase300Data[idx], i);
                break;

            default:
                fprintf(stderr, "\n\tEntry #%u is an unknown file type(0x%x), offset = 0x%x\n", i, fileType, idx);
        }

        idx += size;
        // align idx to a multiple of 8 if it isn't already
        idx = (idx + 7) & ~(DWORD)7;

        ++i;
    }

    free(dbase300Data);
}


/*** local functions definitions ***/
static void saveToFile(const BYTE *data, DWORD size){
    FILE *out_fp;

    if((out_fp = fopen(path, "wb")) == NULL){
        fprintf(stderr, "Couldn't create %s\n", path);
        exit(EXIT_FAILURE);
    }

    fwrite(data, 1, size, out_fp);
    fclose(out_fp);
}

static void decodeRleImg(BYTE spriteLump[], unsigned numImage){
    /* header for image type 1 (images including palette, this time it's 8bpp instead of
    the usual 6bpp format used for every other image/sprite)
    */
    typedef struct imgType1_hdr_s{
        DWORD imgType;  // 1
        WORD width;
        WORD height;
        rgb_palette_t rgb_palette[256];
    }imgType1_hdr_t;

    typedef struct imgType3_hdr_s{    // header for image type 3
        DWORD imgType;  // 3
        WORD width;
        WORD height;
    }imgType3_hdr_t;

    typedef struct imgType7_hdr_s{    // header for image type 7
        DWORD imgType;  // 7
        WORD width;
        WORD height;

        WORD unk;
        WORD width2;    // identical to width

        WORD unk2;
        WORD height2;   // identical to height
    }imgType7_hdr_t;


    BYTE *decodedSpriteBuf;

    imgType1_hdr_t *imgType1_hdr;
    imgType3_hdr_t *imgType3_hdr;
    imgType7_hdr_t *imgType7_hdr;


    //unsigned srcIdx = sizeof(*rleImgHdr);
    unsigned srcIdx;
    unsigned destIdx = 0;

    //unsigned decodedSpriteSize = rleImgHdr->width * rleImgHdr->height;
    unsigned decodedSpriteSize;

    DWORD width, height;

    // initialize stuff
    switch(*(DWORD *)spriteLump){
        case 1:
            imgType1_hdr = spriteLump;
            width =         imgType1_hdr->width;
            height =        imgType1_hdr->height;
            rgb8bppToTgaPal(imgType1_hdr->rgb_palette);
            srcIdx = sizeof(imgType1_hdr_t);
            break;

        case 3:
            initPalette();
            imgType3_hdr = spriteLump;
            width =     imgType3_hdr->width;
            height =    imgType3_hdr->height;
            srcIdx =    sizeof(imgType3_hdr_t);
            break;

        case 7:
            initPalette();
            imgType7_hdr = spriteLump;
            width =     imgType7_hdr->width;
            height =     imgType7_hdr->height;
            srcIdx =    sizeof(imgType7_hdr_t);
    }

    decodedSpriteSize = width * height;

    if((decodedSpriteBuf = malloc(decodedSpriteSize)) == NULL){
        fprintf(stderr, "Couldn't allocate %u bytes for sprite # %u\n", decodedSpriteSize, numImage);
        exit(EXIT_FAILURE);
    }

    // start decoding
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
    pic_handler(decodedSpriteBuf, width, height, NOT_FLIPPED);

    free(decodedSpriteBuf);
}
