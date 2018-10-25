#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"

extern char path[];
extern char *fileNamePtr;

typedef struct wavHeader_s{
    char chunkID[4];
    DWORD chunkSize;
    char format[4];

    char subChunk1ID[4];
    DWORD subChunk1Size;
    WORD audioFormat;
    WORD numChannels;
    DWORD sampleRate;
    DWORD byteRate;
    WORD blockAlign;
    WORD bitsPerSample;

    char subChunk2ID[4];
    DWORD subChunk2Size;
}wavHeader_t;

static wavHeader_t wavHdr = {
    {'R', 'I', 'F', 'F'},
    0, /* to be defined later */
    {'W', 'A', 'V', 'E'},

    {'f', 'm', 't', ' '},
    16,
    1, /* pcm */
    1, /* mono */
    22050,
    (22050 * 1 * 16) / 8, /* (SampleRate * NumChannels * BitsPerSample) / 8 */
    1 * 16 / 8, /* NumChannels * BitsPerSample / 8 */
    16,

    {'d', 'a', 't', 'a'},
    0 /* to be defined later */
};


typedef struct sndNameEntry_s{
    WORD index;         // used for accessing the offsets in the FAT, associating them with the name specified in lmpName field
    char sndName[];     // struct hack; the real length is given by zero-termination (after proper adjustment by the namesFix() function)
}sndNameEntry_t;

/** local functions declarations **/
static void namesFix(sndNameEntry_t *sndNameEntry);
static void init_deltaTable(SWORD deltaTable[]);

void SFXextract(FILE *in_fp){
    typedef struct SFXhdr_s{
        char    SFX_id_str[4];      // "0XFS" (no zero-termination)
        WORD    SFX_id_num;         // always 1?
        WORD    unk;
        DWORD   sndsFAT_offset;
        DWORD   unk_offset;
        DWORD   sndsFAT_size;       // expressed as bytes' size, not as the number of entries
        DWORD   sndNameFAT_offset;
        DWORD   sndNameFAT_size;    // expressed as bytes' size, not as the number of entries
    }SFXhdr_t;


    typedef struct sndsFAT_s{
        DWORD   offset;
        DWORD   size;
        WORD    index;
        WORD    type;   /* 0 = null entry/placeholder
                        ** 1 = 16 bit, 11025 Hz
                        ** 3 = 16 bit, 22050 Hz */
    }sndsFAT_t;


    BYTE            *SFX_Data;
    SFXhdr_t        *SFXhdr;
    sndsFAT_t       *sndsFAT;
    sndNameEntry_t  *sndNameEntry;

    FILE *wav_fp;

    unsigned SFX_DataSize;
    unsigned numSnds;

    unsigned i;

     // read the file into memory
    fseek(in_fp, 0, SEEK_END);
    SFX_DataSize = ftell(in_fp);

    if((SFX_Data = malloc(SFX_DataSize)) == NULL){
        fprintf(stderr, "Couldn't allocate %u bytes for the SFX file\n", SFX_DataSize);
        exit(EXIT_FAILURE);
    }

    rewind(in_fp);
    fread(SFX_Data, 1, SFX_DataSize, in_fp);

    // various initializations
    SFXhdr = SFX_Data;

    sndsFAT = SFX_Data + SFXhdr->sndsFAT_offset;
    numSnds = SFXhdr->sndsFAT_size / sizeof(sndsFAT_t);

    sndNameEntry = SFX_Data + SFXhdr->sndNameFAT_offset;

    namesFix(sndNameEntry);

    // start extracting stuff
    for(i = 0; i < numSnds; ++i, sndNameEntry = (BYTE *)sndNameEntry + sizeof(sndNameEntry->index) + strlen(sndNameEntry->sndName) + 1){

        sprintf(fileNamePtr, "%.3u_%s.wav", i + 1, sndNameEntry->sndName);

        switch(sndsFAT[i].type){
            case 0:     // null entry/leftover entry which was probably discarded from the final game release
                sprintf(fileNamePtr, "(NULL)%.3u_%s.wav", i + 1, sndNameEntry->sndName);

            case 1:
                wavHdr.sampleRate = 11025;
                break;

            case 3:
                wavHdr.sampleRate = 22050;
                break;

            /** there doesn't seem to be any other types other than the ones listed above; no need for a default case
            default:
                fprintf(stderr, "\n\t%s 's sound type is unknown(%hu)\n", fileNamePtr, sndsFAT[i].type);
                continue;
            **/
        }

        if((wav_fp = fopen(path, "wb")) == NULL){
            fprintf(stderr, "Couldn't create %s\n", path);
            exit(EXIT_FAILURE);
        }

        wavHdr.byteRate =   (wavHdr.sampleRate * wavHdr.numChannels * wavHdr.bitsPerSample) / 8;

        /* set chunkSize and subChunk2Size header fields */
        wavHdr.chunkSize = 36 + sndsFAT[i].size;
        wavHdr.subChunk2Size = sndsFAT[i].size;

        /* write the wav header */
        fwrite(&wavHdr, sizeof(wavHdr), 1, wav_fp);

        /* write the pcm data */
        fwrite(SFX_Data + sndsFAT[i].offset, 1, sndsFAT[i].size, wav_fp);

        fclose(wav_fp);

    }

    free(SFX_Data);
}


/************************************************************************************
** DBASE500.DAT DPCM speech files decoder + extractor function.                     *
** The necessary info to achieve this was taken from here,                          *
** with some minor adjustments(just one state variable instead of 2):               *
** https://wiki.multimedia.cx/index.php?title=Gremlin_Digital_Video#Audio_Format    *
************************************************************************************/
void dbase500extract(FILE *in_fp){
    FILE *wav_fp;

    BYTE *dbase500Data;
    wavHeader_t *dbase500WavSndHdr;
    SWORD *sndDecodedBuf;

    SWORD deltaTable[256];

    unsigned dbase500DataSize;
    unsigned i = 1;
    unsigned idx = 8;
    unsigned decodedBufIdx;

    unsigned    curr_DPCM_dataEnd;

    DWORD       decodedSubChunk2Size;

    SWORD   DPCM_state;


    // read the file into memory
    fseek(in_fp, 0, SEEK_END);

    dbase500DataSize = ftell(in_fp);

    if((dbase500Data = malloc(dbase500DataSize)) == NULL){
        fprintf(stderr, "Couldn't allocate %u bytes for DBASE500\n", dbase500DataSize);
        exit(EXIT_FAILURE);
    }

    rewind(in_fp);
    fread(dbase500Data, 1, dbase500DataSize, in_fp);

    // initialize the DPCM delta table
    init_deltaTable(deltaTable);

    // initialize the wav samplerate-related fields to 22050 Hz
    wavHdr.sampleRate = 22050;
    wavHdr.byteRate =   (wavHdr.sampleRate * wavHdr.numChannels * wavHdr.bitsPerSample) / 8;


    // start decoding
    do{
        dbase500WavSndHdr = &dbase500Data[idx];
        idx += sizeof(wavHeader_t);


        /** 0x2A = custom DPCM format; though all DBASE500's .wavs seem to be of type 0x2A,
        *** so checking for the type is kinda superflous
        **/
        // if(dbase500WavSndHdr->audioFormat == 0x2A){

        sprintf(fileNamePtr, "%.4u.wav", i);
        if((wav_fp = fopen(path, "wb")) == NULL){
            fprintf(stderr, "Couldn't create %s\n", path);
            exit(EXIT_FAILURE);
        }


        DPCM_state = 0;
        decodedBufIdx = 0;
        decodedSubChunk2Size = dbase500WavSndHdr->subChunk2Size * 2;

        if((sndDecodedBuf = malloc(decodedSubChunk2Size)) == NULL){
            fprintf(stderr, "\n\tCouldn't allocate %u bytes for entry #%u's DPCM decoding buffer\n", decodedSubChunk2Size, i);
            exit(EXIT_FAILURE);
        }

        // decode the current .wav entry
        for(curr_DPCM_dataEnd = idx + dbase500WavSndHdr->subChunk2Size;
            idx < curr_DPCM_dataEnd;
            ++idx, ++decodedBufIdx)
        {
            DPCM_state += deltaTable[dbase500Data[idx]];
            sndDecodedBuf[decodedBufIdx] = DPCM_state;

        }

        /* set chunkSize and subChunk2Size header fields */
        wavHdr.chunkSize = 36 + decodedSubChunk2Size;
        wavHdr.subChunk2Size = decodedSubChunk2Size;

        /* write the wav header */
        fwrite(&wavHdr, sizeof(wavHdr), 1, wav_fp);

        /* write the pcm data */
        fwrite(sndDecodedBuf, 2, dbase500WavSndHdr->subChunk2Size, wav_fp);

        free(sndDecodedBuf);
        fclose(wav_fp);
        /** } // this was supposed to be the end of the "if(dbase500WavSndHdr->audioFormat == 0x2A)" block

        else{
            fprintf(stderr, "\n\tUnknown wav audio format(0x%x)\n", dbase500WavSndHdr->audioFormat);
            exit(EXIT_FAILURE);
        }
        **/

        // align idx to a multiple of 8 if it isn't already
        idx = (idx + 7) & ~(unsigned)7;

        ++i;
    }while(idx < dbase500DataSize);

    free(dbase500Data);
}


/*** local functions definitions ***/

static void init_deltaTable(SWORD deltaTable[]){
    SWORD delta = 0;
    SWORD code = 64;
    SWORD step = 45;

    unsigned i;

    deltaTable[0] = 0;

    for(i = 1; i <= 253;){
        delta += code >> 5;
        code += step;
        step += 2;
        deltaTable[i++] = delta;
        deltaTable[i++] = -delta;
    }

    /* table's last entry is left uncovered by the for loop above,
    ** so we initialize it here
    */
    deltaTable[i] = delta + (code >> 5);
}


/* some entry names include a comment, separated from the name itself by
** a 0x00 byte; we'll replace it with a minus( '-' ) character.
** there's also some other filename-unfriendly characters which need to be replaced as well
*/
static void namesFix(sndNameEntry_t *sndNameEntry){
    unsigned i;
    unsigned nameLength;
    char *findNextName;

    do{
        nameLength = 0;
        findNextName = sndNameEntry->sndName;

        do{
            ++nameLength;
            ++findNextName;
        }   while( *findNextName != '\0' ||
                  (*(WORD*)(findNextName + 1) != sndNameEntry->index + 1 &&
                  *(WORD*)(findNextName + 1) != 0xFFFF ) );


        for(i = 0; i < nameLength; ++i)
            if( sndNameEntry->sndName[i] == '\0' ||
                sndNameEntry->sndName[i] == '\\' ||
                sndNameEntry->sndName[i] == '/'  ||
                sndNameEntry->sndName[i] == ':'  ||
                sndNameEntry->sndName[i] == '?' )
                        sndNameEntry->sndName[i] = '-';

        sndNameEntry = (BYTE *)sndNameEntry + sizeof(sndNameEntry->index) + nameLength + 1;

    } while(sndNameEntry->index != 0xFFFF);
}
