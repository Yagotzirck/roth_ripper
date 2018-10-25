#include <stdio.h>
#include <string.h>

#include "makedir.h"
#include "types.h"
#include "pic_utils.h"

#include "dbase200extract.h"
#include "dbase300extract.h"
#include "DASextract.h"
#include "soundextract.h"
#include "icons_backdrop_extract.h"

extern char path[FILENAME_MAX];
extern char *currSubDirPtr;
extern char *fileNamePtr;


static void initPath(void);
static void initSubDir(const char *fileName);

int main(void){
    typedef struct filesHandlers_s{
        char *fileName;
        void (*ripFunc)(FILE *);
    }filesHandlers_t;

    typedef struct paletteNames_files_s{
        char *palName;
        char *DASfileName;
    }paletteNames_files_t;


    FILE *in_fp;

    unsigned i, filesCount;

    // array of structures associating each file to its ripper function
    filesHandlers_t filesHandlers[] = {
        {"ICONS.ALL",       ICONSextract},
        {"BACKDROP.RAW",    BACKDROPextract},
        {"FXSCRIPT.SFX",    SFXextract},
        {"DBASE200.DAT",    dbase200extract},
        {"DBASE300.DAT",    dbase300extract},
        {"DBASE500.DAT",    dbase500extract},
        {"DEMO.DAS",        DASextract},
        {"DEMO1.DAS",       DASextract},
        {"DEMO2.DAS",       DASextract},
        {"DEMO3.DAS",       DASextract},
        {"DEMO4.DAS",       DASextract},
        {"ADEMO.DAS",       DASextract}
    };

    // array of structures for the palette extraction
    paletteNames_files_t paletteNames_files[] = {
        {"DEMO",    "DEMO.DAS" },
        {"DEMO1",   "DEMO1.DAS"},
        {"DEMO2",   "DEMO2.DAS"},
        {"DEMO3",   "DEMO3.DAS"},
        {"DEMO4",   "DEMO4.DAS"}
    };

    filesCount = sizeof(filesHandlers) / sizeof(filesHandlers[0]);

    puts("\t\tRealms of the Haunting ripper v0.1 by Yagotzirck\n");

    initPath();

    for(i = 0; i < filesCount; ++i){
        if((in_fp = fopen(filesHandlers[i].fileName, "rb")) == NULL){
            fprintf(stderr, "Couldn't open %s. Skipping...\n", filesHandlers[i].fileName);
            continue;
        }

        initSubDir(filesHandlers[i].fileName);

        printf("Extracting %s...", filesHandlers[i].fileName);
        filesHandlers[i].ripFunc(in_fp);
        puts("done");

        fclose(in_fp);
    }

    /**** extract the palettes *****/

    putchar('\n');

    // create the "palettes" subdirectory
    strcpy(currSubDirPtr, "palettes/");
    makeDir(path);

    // initialize the filename pointer to point at the end of the subdirectory string
    fileNamePtr = currSubDirPtr + strlen(currSubDirPtr);

    // start extracting palettes
    filesCount = sizeof(paletteNames_files) / sizeof(paletteNames_files[0]);

    for(i = 0; i < filesCount; ++i){
        if((in_fp = fopen(paletteNames_files[i].DASfileName, "rb")) == NULL)
            continue;

        printf("Extracting %s's palette...", paletteNames_files[i].DASfileName);
        extractPalette(in_fp, paletteNames_files[i].palName);
        puts("done");
        fclose(in_fp);
    }


    // The rip is done
    puts("\nExtraction process complete.");

    return 0;
}

/** initialize the destination path **/
static void initPath(void){
    sprintf(path, "ROTH_Resources/");
    currSubDirPtr = path + strlen(path);
    makeDir(path);
}


static void initSubDir(const char *fileName){
    char *dotFinder = currSubDirPtr;

    // put the filename string right after the base directory string
    strcpy(currSubDirPtr, fileName);

    // replace the filename's extension dot with a '/', then put a string termination character after it
    while(*dotFinder != '.')
        ++dotFinder;
    *dotFinder = '/';
    *(dotFinder + 1) = '\0';

    // create the subdirectory
    makeDir(path);

    // initialize the filename pointer to point at the end of the subdirectory string
    fileNamePtr = dotFinder + 1;
}
