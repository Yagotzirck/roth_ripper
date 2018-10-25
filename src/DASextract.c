// FAT = File Allocation Table

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "pic_utils.h"
#include "tga_utils.h"

extern char path[];
extern char *fileNamePtr;

enum imgBasicTypes{
    PLAIN_DATA =            0x02,
    PLAIN_DATA_2 =          0x1A,
    PLAIN_DATA_3 =          0x18,
    PLAIN_DATA_4 =          0x04,
    PLAIN_DATA_5 =          0x1C,
    PLAIN_DATA_6 =          0x06,
    PLAIN_DATA_7 =          0x0A,
    PLAIN_DATA_8 =          0x0C,
    PLAIN_DATA_9 =          0x00,

    PLAIN_DATA_FLIPPED =    0x10,
    PLAIN_DATA_FLIPPED_2 =  0x12,
    PLAIN_DATA_FLIPPED_3 =  0x14,
    PLAIN_DATA_FLIPPED_4 =  0x30,

    COMPRESSED =            0x11,
    COMPRESSED_2 =          0x13,
    COMPRESSED_3 =          0x31,
    COMPRESSED_4 =          0x17,
    COMPRESSED_5 =          0x33,
    COMPRESSED_6 =          0x03
};

typedef struct DAShdr_s{
    char    DAS_id_str[4];          // "DASP" (no zero-termination)
    WORD    DAS_id_num;             // always 5
    WORD    size_FAT;               // the total size of the 2 file allocation tables combined together(I guess)
    DWORD   imgFATOffset;           // it seems to be always 0x44(immediately following this header)
    DWORD   paletteOffset;          // if it's zero, then this file is ADEMO.DAS
    DWORD   unk_0x10;
    DWORD   fileNamesBlockOffset;
    WORD    fileNamesBlockSize;
    WORD    unk_0x1C_size;          // size of data pointed by unk_0x1C
    DWORD   unk_0x1C;
    DWORD   unk_0x20;
    DWORD   unk_0x24;               // useless FAT;     size = imgFAT_numEntries * 4
    DWORD   unk_0x28;               // useless FAT 2
    DWORD   unk_0x28_size;               //
    DWORD   unk_0x30;
    WORD    imgFAT_numEntries;
    WORD    imgFAT_numEntries2;    // number of 2nd img FAT entries, contiguous to the 1st one
    DWORD   unk_0x38;
    WORD    unk_0x38_size;
    WORD    unk_0x40_size;
    DWORD   unk_0x40;
}DAShdr_t;


typedef struct imgFAT_s{
    DWORD   offset;
    WORD    unk1;
    BYTE    type;   // if it's 0x20 or 0x24 it's not pointing to image data, but some unknown shit to skip
    BYTE    unk2;
}imgFAT_t;


typedef struct lumpNameEntry_s{
    WORD entrySize;     // size of the whole entry(entrySize + index + lmpName's string length)
    WORD index;         // used for accessing the offsets in the FAT, associating them with the name specified in lmpName field
    char lmpName[];     // struct hack; the real length is entrySize - 4
}lumpNameEntry_t;

typedef struct imgBasicHdr_s{
    BYTE unk;
    BYTE imageType;
    WORD width;
    WORD height;
}imgBasicHdr_t;

typedef struct imgCompressed1Hdr_s{
    BYTE    unk;
    BYTE    imageType;
    WORD    width;
    WORD    height;
    WORD    spritesBlockSize;
    WORD    unk2;
    WORD    firstImgOffset;     // relative to the start of this header
    WORD    numSubImgs;         // the amount of compressed images following the first uncompressed frame
    WORD    unk3;         //
    WORD    unk4;
    /* this header type is then followed by an array of numSubImgs DWORDs which supposedly specify the offset
    ** of each compressed frame, relative to a point i'm not too sure about; however since the compressed
    ** frames' data is contiguous and there's a code that specifies the end of each frame, I'm not going to
    ** use the aforementioned offsets
    */
}imgCompressed1Hdr_t;

typedef struct imgCompressed2Hdr_s{
    BYTE    unk;
    BYTE    imageType;
    WORD    width;
    WORD    height;
    WORD    unk2;       // seems to be always 0
    WORD    unk3;       // as above
    WORD    unk4;
    WORD    imageType2; // if it's  not 0xFFFE, then this is an imgCompressed1Hdr type
    WORD    unk5;
}imgCompressed2Hdr_t;

/* imgCompressed2Hdr_s is followed by a series of subimages
** having the following header:
*/
typedef struct subImgCompressed2Hdr_s{
    WORD    subImgID;       // seems to be always 0x17
    WORD    unk;            // seems to be always zero
    WORD    bufWidth;
    WORD    bufHeight;
    WORD    numImgs;
    WORD    currImgIdx;
    DWORD   currImgSize;    // including this header
    WORD    unk2;
    WORD    width;
    WORD    unk4;
    WORD    height;
}subImgCompressed2Hdr_t;


typedef struct multiPlainImgsHdr_s{ // I don't know the meaning of the fields in this header, I'm declaring it just to show I'm simply skipping it
    BYTE unknown[32];
}multiPlainImgsHdr_t;


/** local functions declarations **/
static void decodeImgCompressed1(BYTE *imgData, unsigned numImage, char lmpName[]);
static void decodeImgCompressed2(BYTE *imgData, unsigned numImage, char lmpName[]);
static void decodeMultiPlainImgs(BYTE *imgData, unsigned numImage, char lmpName[]);
static void decode3DObjsTextures(BYTE *imgData, unsigned numImage, char lmpName[]);
static void nameFix(lumpNameEntry_t *lumpNameEntry);

void DASextract(FILE *in_fp){
    BYTE *DAS_Data;
    DAShdr_t *DAShdr;
    imgFAT_t *imgFAT;
    imgCompressed2Hdr_t *imgCompressed2Hdr;
    lumpNameEntry_t *lumpNameEntry;

    BYTE *imgDataPtr;
    imgBasicHdr_t *imgBasicHdr;

    unsigned DAS_DataSize;

    unsigned i = 1;


    // read the file into memory
    fseek(in_fp, 0, SEEK_END);
    DAS_DataSize = ftell(in_fp);

    if((DAS_Data = malloc(DAS_DataSize)) == NULL){
        fprintf(stderr, "Couldn't allocate %u bytes for the DAS file\n", DAS_DataSize);
        exit(EXIT_FAILURE);
    }

    rewind(in_fp);
    fread(DAS_Data, 1, DAS_DataSize, in_fp);

    // various initializations
    DAShdr = DAS_Data;

    imgFAT = DAS_Data + DAShdr->imgFATOffset;

    // ADEMO.DAS doesn't have a palette, so we'll take it from DEMO.DAS
    if(DAShdr->paletteOffset == 0)
        initPalette();
    else
        rgb6bppToTgaPal(DAS_Data + DAShdr->paletteOffset);

    // start extracting stuff
    for(lumpNameEntry = (lumpNameEntry_t *)(DAS_Data + DAShdr->fileNamesBlockOffset + 4);
        lumpNameEntry < DAS_Data + DAS_DataSize - 4;
        lumpNameEntry = (BYTE *)lumpNameEntry + lumpNameEntry->entrySize, ++i){

        nameFix(lumpNameEntry);

        // weird shit to skip related to some sort of subdirectory system I guess, not image data
        if(imgFAT[lumpNameEntry->index].type == 0x24 ||
           imgFAT[lumpNameEntry->index].type == 0x20   )
            continue;

        imgDataPtr = DAS_Data + imgFAT[lumpNameEntry->index].offset;

        imgBasicHdr = imgDataPtr;

        switch(imgBasicHdr->imageType){
            case 0x80:      // lumps of this type seem to be 3D objects, not graphics
                break;

            case PLAIN_DATA:
            case PLAIN_DATA_2:
            case PLAIN_DATA_3:
            case PLAIN_DATA_4:
            case PLAIN_DATA_5:
            case PLAIN_DATA_6:
            case PLAIN_DATA_7:
            case PLAIN_DATA_8:
            case PLAIN_DATA_9:
                sprintf(fileNamePtr, "%.4u_%s.tga", i, lumpNameEntry->lmpName);
                pic_handler(imgDataPtr + sizeof(imgBasicHdr_t), imgBasicHdr->width, imgBasicHdr->height, NOT_FLIPPED);
                break;

            case PLAIN_DATA_FLIPPED:
            case PLAIN_DATA_FLIPPED_2:
                if(imgBasicHdr->unk > 0xC0){
                    decodeMultiPlainImgs(imgDataPtr, i, lumpNameEntry->lmpName);    // sprites consisting of only 3 rotations
                    break;
                }

                if(imgBasicHdr->unk == 0x40){
                    decode3DObjsTextures(imgDataPtr, i, lumpNameEntry->lmpName);
                    break;
                }

            case PLAIN_DATA_FLIPPED_3:
            case PLAIN_DATA_FLIPPED_4:
                sprintf(fileNamePtr, "%.4u_%s.tga", i, lumpNameEntry->lmpName);
                pic_handler(imgDataPtr + sizeof(imgBasicHdr_t), imgBasicHdr->width, imgBasicHdr->height, FLIPPED);
                break;

            case COMPRESSED:
            case COMPRESSED_2:
            case COMPRESSED_3:
            case COMPRESSED_4:
            case COMPRESSED_5:
            case COMPRESSED_6:
                imgCompressed2Hdr = imgDataPtr;
                if(imgCompressed2Hdr->imageType2 != 0xFFFE)
                    decodeImgCompressed1(imgDataPtr, i, lumpNameEntry->lmpName);
                else
                    decodeImgCompressed2(imgDataPtr, i, lumpNameEntry->lmpName);
                break;


            default:
                fprintf(stderr, "\n\tImage #%u 's type is unknown (0x%x, offset = 0x%x, name = %s)\n", i, imgBasicHdr->imageType, imgFAT[lumpNameEntry->index].offset, lumpNameEntry->lmpName);
                //exit(EXIT_FAILURE);
        }
    }
    free(DAS_Data);
}

void extractPalette(FILE *in_fp, const char *DASname){
    DAShdr_t DAShdr;

    rgb_palette_t rgb_palette[256];

    FILE *out_fp;

    unsigned i;

    // acquire the header
    fread(&DAShdr, sizeof(DAShdr), 1, in_fp);

    //position to the palette's offset
    fseek(in_fp, DAShdr.paletteOffset, SEEK_SET);

    // acquire the palette
    fread(&rgb_palette, sizeof(rgb_palette[0]), sizeof(rgb_palette) / sizeof(rgb_palette[0]), in_fp);


    /* convert from range 0-63 to 0-255;
    ** the shift-OR optimization was taken from:
    ** http://www.shikadi.net/moddingwiki/VGA_Palette
    */
    for(i = 0; i < 256; ++i){
        rgb_palette[i].red = (rgb_palette[i].red << 2) | (rgb_palette[i].red >> 4);
        rgb_palette[i].green = (rgb_palette[i].green << 2) | (rgb_palette[i].green >> 4);
        rgb_palette[i].blue = (rgb_palette[i].blue << 2) | (rgb_palette[i].blue >> 4);
    }

    // save the palette
    sprintf(fileNamePtr, "%s palette.pal", DASname);
    if((out_fp = fopen(path, "wb")) == NULL){
        fprintf(stderr, "Couldn't create %s\n", path);
        exit(EXIT_FAILURE);
    }
    fwrite(&rgb_palette, sizeof(rgb_palette[0]), sizeof(rgb_palette) / sizeof(rgb_palette[0]), out_fp);
    fclose(out_fp);
}


/***** local functions definitions *****/

static void decodeImgCompressed1(BYTE *imgData, unsigned numImage, char lmpName[]){
    imgCompressed1Hdr_t *imgCompressed1Hdr;
    imgBasicHdr_t *imgBasicHdr;
    BYTE *firstImgRawData;
    BYTE *firstImgRawDataPtr;

    BYTE *imgEncodedData;

    BYTE *safeBuf;

    unsigned frameSize;

    unsigned i = 1;
    unsigned numImgs;

    WORD code0x80Word;
    BYTE code;

    // various initializations
    imgCompressed1Hdr = imgData;
    imgBasicHdr = imgData + imgCompressed1Hdr->firstImgOffset;
    firstImgRawData = (BYTE*)imgBasicHdr + sizeof(imgBasicHdr_t);
    imgEncodedData = firstImgRawData + imgCompressed1Hdr->width * imgCompressed1Hdr->height;

    /*** since the "shrink_tga()" function in tga_utils.c might alter the original buffer, and
    **** in this encoding format each frame depends on the previous one (interframe compression),
    **** we'll pass a safe buffer to pic_handler() so that there won't be any risk of
    **** modifying the original frame
    ***/
    frameSize = imgBasicHdr->width * imgBasicHdr->height;
    if((safeBuf = malloc(frameSize)) == NULL){
        fprintf(stderr, "\n\tCouldn't allocate %u bytes for %s 's safe buffer\n", frameSize, lmpName);
        exit(EXIT_FAILURE);
    }

    memcpy(safeBuf, firstImgRawData, frameSize);


    // save the first frame as-is, since it's plain data
    sprintf(fileNamePtr, "%.4u_%s_%.2u.tga", numImage, lmpName, i);
    pic_handler(safeBuf, imgBasicHdr->width, imgBasicHdr->height, imgBasicHdr->imageType == 1 ? NOT_FLIPPED : FLIPPED);

    // the other frames must be decoded
    ++i;
    numImgs = imgCompressed1Hdr->numSubImgs + 1;
    while(i <= numImgs){
        firstImgRawDataPtr = firstImgRawData;


        for(;;){
            code = *imgEncodedData++;

            if(code == 0){  // RLE
                if(*imgEncodedData == 0)    // end of the current sequence
                    goto endSequence;

                code = *imgEncodedData++;
                memset(firstImgRawDataPtr, *imgEncodedData++, code);
                firstImgRawDataPtr += code;

            }

            else
            if(code > 0x80){    // move (code & 0x7F) bytes forward
                code &= 0x7F;
                firstImgRawDataPtr += code;
            }

            else
            if(code < 0x80){    // copy (code) bytes from the current point in the encoded sequence to the decoded buffer
                memcpy(firstImgRawDataPtr, imgEncodedData, code);
                imgEncodedData += code;
                firstImgRawDataPtr += code;
            }

            else{   // code holds the value 0x80
                code0x80Word = *(WORD *)imgEncodedData;     // read the next 2 bytes as a WORD variable
                imgEncodedData += 2;

                if(code0x80Word == 0)   // end of the current frame
                    break;


                if((code0x80Word & 0x8000)){    // if the MSB bit is set...
                    code0x80Word &= 0x3FFF;     // ... then set the top 2 bits to zero
                    if(*imgEncodedData == 0){   // if the byte following the WORD variable in the encoded data is zero...
                        memset(firstImgRawDataPtr, 0, code0x80Word);    // ...then write (code0x80Word) zeroes in the decoded buffer
                        imgEncodedData++;
                    }
                }

                firstImgRawDataPtr += code0x80Word;
            }

        }

        // save the freshly decoded frame

        sprintf(fileNamePtr, "%.4u_%s_%.2u.tga", numImage, lmpName, i);
        memcpy(safeBuf, firstImgRawData, frameSize);
        pic_handler(safeBuf, imgBasicHdr->width, imgBasicHdr->height, imgBasicHdr->imageType == 1 ? NOT_FLIPPED : FLIPPED);

        // move to the next frame
        ++i;
    }

    endSequence:
    free(safeBuf);
}


static void decodeImgCompressed2(BYTE *imgData, unsigned numImage, char lmpName[]){
    imgCompressed2Hdr_t *imgCompressed2Hdr;
    subImgCompressed2Hdr_t *subImgCompressed2Hdr;

    BYTE *encodedBufPtr;

    BYTE *decodedBuf, *decodedBufPtr, *decodedBufEnd;

    unsigned i;
    unsigned decodedBufSize;

    WORD numImgs;


    // various initializations
    imgCompressed2Hdr = imgData;
    subImgCompressed2Hdr = imgData + sizeof(imgCompressed2Hdr_t);
    i = 1;

    numImgs = subImgCompressed2Hdr->numImgs;

    // start decoding
    do{
        decodedBufSize = subImgCompressed2Hdr->width * subImgCompressed2Hdr->height;

        if((decodedBuf = malloc(decodedBufSize)) == NULL){
            fprintf(stderr, "\nCouldn't allocate %u bytes for a sprite's decoded buf size\n"
                            "\t(name: %s)\n", decodedBufSize, lmpName);
                exit(EXIT_FAILURE);
        }

        decodedBufPtr = decodedBuf;

        decodedBufEnd = decodedBuf + decodedBufSize;

        encodedBufPtr = (BYTE *)subImgCompressed2Hdr + sizeof(subImgCompressed2Hdr_t);

        do{
            // a byte with the top 4 bits set indicates a RLE byte; the low 4 bits indicate how many times to repeat the next byte
            if(*encodedBufPtr > 0xF0){
                BYTE pixelCount = *encodedBufPtr++ & 0x0F;
                memset(decodedBufPtr, *encodedBufPtr++, pixelCount);
                decodedBufPtr += pixelCount;
            }
            else
                *decodedBufPtr++ = *encodedBufPtr++;
        }while(decodedBufPtr < decodedBufEnd);

        // save the freshly decoded frame
        sprintf(fileNamePtr, "%.4u_%s_%.2u.tga", numImage, lmpName, i);
        pic_handler(decodedBuf, subImgCompressed2Hdr->width, subImgCompressed2Hdr->height, FLIPPED);

        // move to the next frame
        ++i;
        subImgCompressed2Hdr = (BYTE *)subImgCompressed2Hdr + subImgCompressed2Hdr->currImgSize;

        free(decodedBuf);

    }while(numImgs == subImgCompressed2Hdr->numImgs);
}

static void decodeMultiPlainImgs(BYTE *imgData, unsigned numImage, char lmpName[]){
    multiPlainImgsHdr_t *multiPlainImgsHdr;
    imgBasicHdr_t *imgBasicHdr;

    BYTE *imgRawData;

    unsigned i;

    unsigned alignment, lowPtr4Bits;

    BYTE imgReference;

    // initializations
    imgBasicHdr = imgData;
    multiPlainImgsHdr = imgData;
    imgBasicHdr = imgData + sizeof(multiPlainImgsHdr_t);
    imgRawData = imgData + sizeof(multiPlainImgsHdr_t) + sizeof(imgBasicHdr_t);

    alignment = (unsigned)imgBasicHdr & 0xF;

    imgReference = imgBasicHdr->unk;

    for(i = 1; imgReference == imgBasicHdr->unk; ++i){
        sprintf(fileNamePtr, "%.4u_%s_%.2u.tga", numImage, lmpName, i);
        pic_handler(imgRawData, imgBasicHdr->width, imgBasicHdr->height, FLIPPED);

        // move to the next frame, aligning the low 4 bits to the same 4 bits of the first image if necessary
        imgRawData += imgBasicHdr->width * imgBasicHdr->height;

        lowPtr4Bits = (unsigned)imgRawData & 0xF;
        if(lowPtr4Bits > alignment)
            imgRawData = imgRawData + (alignment + 0x10 - lowPtr4Bits);
        else
            imgRawData = imgRawData + (alignment - lowPtr4Bits);

        imgBasicHdr = imgRawData;
        imgRawData += sizeof(imgBasicHdr_t);
    }
}


static void decode3DObjsTextures(BYTE *imgData, unsigned numImage, char lmpName[]){
    imgBasicHdr_t *imgBasicHdr;

    BYTE *imgRawData;

    WORD *imgHdrFinder;

    unsigned i, numImgs = 0;

    unsigned alignment, lowPtr4Bits;

    BYTE imgReference;


    // initializations
    imgHdrFinder = imgData + sizeof(imgBasicHdr_t);


    // skip the array of words containing unknown values until the padding zeroes are reached
    do{
        ++imgHdrFinder;
        ++numImgs;
    }while(*imgHdrFinder != 0);

    --numImgs;

    // position into the subimage header

    imgRawData = imgHdrFinder;
    do
    ++imgRawData;
    while(*imgRawData == 0);

    --imgRawData;
    imgBasicHdr = imgRawData;
    imgRawData += sizeof(imgBasicHdr_t);

    alignment = (unsigned)imgBasicHdr & 0xF;
    imgReference = imgBasicHdr->imageType;

    for(i = 1; i <= numImgs && imgReference == imgBasicHdr->imageType; ++i){
        sprintf(fileNamePtr, "%.4u_%s_%.2u.tga", numImage, lmpName, i);
        pic_handler(imgRawData, imgBasicHdr->width, imgBasicHdr->height, FLIPPED);

        // move to the next frame, aligning the low 4 bits to the same 4 bits of the first image if necessary
        imgRawData += imgBasicHdr->width * imgBasicHdr->height;

        lowPtr4Bits = (unsigned)imgRawData & 0xF;
        if(lowPtr4Bits > alignment)
            imgRawData = imgRawData + (alignment + 0x10 - lowPtr4Bits);
        else
            imgRawData = imgRawData + (alignment - lowPtr4Bits);

        imgBasicHdr = imgRawData;
        imgRawData += sizeof(imgBasicHdr_t);
    }
}


/* some entry names include a comment, separated from the name itself by
** a 0x00 byte; we'll replace it with a minus( '-' ) character.
** there's also some other filename-unfriendly characters which need to be replaced as well
*/
static void nameFix(lumpNameEntry_t *lumpNameEntry){
    unsigned i;
    unsigned nameLength = lumpNameEntry->entrySize - 4 - 1;     // entry size minus the other 2 WORD fields minus the last string-terminating '\0' value

    for(i = 0; i < nameLength; ++i)
        if( (lumpNameEntry->lmpName[i] == 0x00 && lumpNameEntry->lmpName[i+1] != 0x00) ||
            lumpNameEntry->lmpName[i] == '\\' ||
            lumpNameEntry->lmpName[i] == '/' ||
            lumpNameEntry->lmpName[i] == ':' ||
            lumpNameEntry->lmpName[i] == '?' )
                    lumpNameEntry->lmpName[i] = '-';
}
