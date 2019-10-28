/*
 * Star Micronics
 * 
 * CUPS Filter
 *
 * [ Linux ]
 * compile cmd: gcc -Wl,-rpath,/usr/lib -Wall -fPIC -O2 -o rastertostarlm rastertostarlm.c -lcupsimage -lcups
 * compile requires cups-devel-1.1.19-13.i386.rpm (version not neccessarily important?)
 * find cups-devel location here: http://rpmfind.net/linux/rpm2html/search.php?query=cups-devel&submit=Search+...&system=&arch=
 *
 * [ Mac OS X ]
 * compile cmd: gcc -Wall -fPIC -o rastertostarlm rastertostarlm.c -lcupsimage -lcups -arch i386 -arch x86_64
 */

/*
 * Copyright (C) 2004-2018 Star Micronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <cups/ppd.h>
#include <cups/raster.h>
#ifdef __APPLE__
#include <cups/backend.h>
#include <cups/sidechannel.h>
#endif
#include <stdlib.h>
#include <fcntl.h>

#ifdef RPMBUILD

#include <dlfcn.h>

typedef cups_raster_t * (*cupsRasterOpen_fndef)(int fd, cups_mode_t mode);
typedef unsigned (*cupsRasterReadHeader_fndef)(cups_raster_t *r, cups_page_header2_t *h);
typedef unsigned (*cupsRasterReadPixels_fndef)(cups_raster_t *r, unsigned char *p, unsigned len);
typedef void (*cupsRasterClose_fndef)(cups_raster_t *r);

static cupsRasterOpen_fndef cupsRasterOpen_fn;
static cupsRasterReadHeader_fndef cupsRasterReadHeader_fn;
static cupsRasterReadPixels_fndef cupsRasterReadPixels_fn;
static cupsRasterClose_fndef cupsRasterClose_fn;

#define CUPSRASTEROPEN (*cupsRasterOpen_fn)
#define CUPSRASTERREADHEADER (*cupsRasterReadHeader_fn)
#define CUPSRASTERREADPIXELS (*cupsRasterReadPixels_fn)
#define CUPSRASTERCLOSE (*cupsRasterClose_fn)

typedef void (*ppdClose_fndef)(ppd_file_t *ppd);
typedef ppd_choice_t * (*ppdFindChoice_fndef)(ppd_option_t *o, const char *option);
typedef ppd_choice_t * (*ppdFindMarkedChoice_fndef)(ppd_file_t *ppd, const char *keyword);
typedef ppd_option_t * (*ppdFindOption_fndef)(ppd_file_t *ppd, const char *keyword);
typedef void (*ppdMarkDefaults_fndef)(ppd_file_t *ppd);
typedef ppd_file_t * (*ppdOpenFile_fndef)(const char *filename);

typedef void (*cupsFreeOptions_fndef)(int num_options, cups_option_t *options);
typedef int (*cupsParseOptions_fndef)(const char *arg, int num_options, cups_option_t **options);
typedef int (*cupsMarkOptions_fndef)(ppd_file_t *ppd, int num_options, cups_option_t *options);

static ppdClose_fndef ppdClose_fn;
static ppdFindChoice_fndef ppdFindChoice_fn;
static ppdFindMarkedChoice_fndef ppdFindMarkedChoice_fn;
static ppdFindOption_fndef ppdFindOption_fn;
static ppdMarkDefaults_fndef ppdMarkDefaults_fn;
static ppdOpenFile_fndef ppdOpenFile_fn;

static cupsFreeOptions_fndef cupsFreeOptions_fn;
static cupsParseOptions_fndef cupsParseOptions_fn;
static cupsMarkOptions_fndef cupsMarkOptions_fn;

#define PPDCLOSE            (*ppdClose_fn)
#define PPDFINDCHOICE       (*ppdFindChoice_fn)
#define PPDFINDMARKEDCHOICE (*ppdFindMarkedChoice_fn)
#define PPDFINDOPTION       (*ppdFindOption_fn)
#define PPDMARKDEFAULTS     (*ppdMarkDefaults_fn)
#define PPDOPENFILE         (*ppdOpenFile_fn)

#define CUPSFREEOPTIONS     (*cupsFreeOptions_fn)
#define CUPSPARSEOPTIONS    (*cupsParseOptions_fn)
#define CUPSMARKOPTIONS     (*cupsMarkOptions_fn)


#else

#define CUPSRASTEROPEN cupsRasterOpen
#define CUPSRASTERREADHEADER cupsRasterReadHeader2
#define CUPSRASTERREADPIXELS cupsRasterReadPixels
#define CUPSRASTERCLOSE cupsRasterClose

#define PPDCLOSE ppdClose
#define PPDFINDCHOICE ppdFindChoice
#define PPDFINDMARKEDCHOICE ppdFindMarkedChoice
#define PPDFINDOPTION ppdFindOption
#define PPDMARKDEFAULTS ppdMarkDefaults
#define PPDOPENFILE ppdOpenFile

#define CUPSFREEOPTIONS cupsFreeOptions
#define CUPSPARSEOPTIONS cupsParseOptions
#define CUPSMARKOPTIONS cupsMarkOptions

#endif

#define MAX(a,b) ( ((a) > (b)) ? (a) : (b))

#define FALSE 0
#define TRUE  (!FALSE)

// definitions for model number
#define SP747           747
#define SP717           717
#define SP742           742
#define SP712           712
#define SP542           542
#define SP512           512
#define HSP7000S        70002
#define HSP7000V        70003

// definitions for resolution
#define RES170x72DPI    170    //SP500, SP700
#define RES85x72DPI     85
#define RES160x72DPI    160    //HSP7000
#define RES80x72DPI     80

// definitions for printable width
#define SP500_170DPI_MAX_WIDTH 420
#define SP500_170DPI_STD_WIDTH 420

#define SP500_85DPI_MAX_WIDTH  210
#define SP500_85DPI_STD_WIDTH  210

#define HSP7000_160DPI_MAX_WIDTH 540
#define HSP7000_160DPI_STD_WIDTH 540

#define HSP7000_80DPI_MAX_WIDTH 270
#define HSP7000_80DPI_STD_WIDTH 270

// definitions for buzzer action
#define BUZZER_NO_USE   0
#define BUZZER_DOC_TOP  1
#define BUZZER_DOC_BTM  2

// definitions for dataTreatmentRecoverFromError action
#define DATACANCEL_NO_USE 0
#define DATACANCEL_DOC    1

struct settings_
{
    int modelNumber;

    float pageWidth;
    float pageHeight;

    int pageType;
    int bidirectional;

    int pageCutType;
    int docCutType;

    int buzzer1Setting;
    int buzzer1OnTime;
    int buzzer1OffTime;
    int buzzer1Repeat;
    int buzzer2Setting;
    int buzzer2OnTime;
    int buzzer2OffTime;
    int buzzer2Repeat;

    int dataTreatmentRecoverFromError;

    int cashDrawerSetting;
    int cashDrawer1PulseWidth;

    int resolution;
    int resolutionX;
    int resolutionY;    

    int bytesPerScanLine;
    int bytesPerScanLineStd;
    int bytesPerHorizDot;
};

struct command
{
    int    length;
    char* command;
};

// Debug
void debugInfo(struct settings_ settings)
{
  char *logLevel = "WARNING";

  fprintf(stderr, "%s: ===== debug Info =====\n\n", logLevel);

  fprintf(stderr, "%s:   modelNumber                     = %d\n", logLevel, settings.modelNumber);
  fprintf(stderr, "%s:   pageWidth                       = %lf\n",logLevel, settings.pageWidth);
  fprintf(stderr, "%s:   pageHeight                      = %lf\n",logLevel, settings.pageHeight);
  fprintf(stderr, "%s:   pageType                        = %d\n", logLevel, settings.pageType);
  fprintf(stderr, "%s:   bidirectional                   = %d\n", logLevel, settings.bidirectional);
  fprintf(stderr, "%s:   pageCutType                     = %d\n", logLevel, settings.pageCutType);
  fprintf(stderr, "%s:   docCutType                      = %d\n", logLevel, settings.docCutType);
  fprintf(stderr, "%s:   buzzer1Setting                  = %d\n", logLevel, settings.buzzer1Setting);
  fprintf(stderr, "%s:   buzzer1OnTime                   = %d\n", logLevel, settings.buzzer1OnTime);
  fprintf(stderr, "%s:   buzzer1OffTime                  = %d\n", logLevel, settings.buzzer1OffTime);
  fprintf(stderr, "%s:   buzzer1Repeat                   = %d\n", logLevel, settings.buzzer1Repeat);
  fprintf(stderr, "%s:   buzzer2Setting                  = %d\n", logLevel, settings.buzzer2Setting);
  fprintf(stderr, "%s:   buzzer2OnTime                   = %d\n", logLevel, settings.buzzer2OnTime);
  fprintf(stderr, "%s:   buzzer2OffTime                  = %d\n", logLevel, settings.buzzer2OffTime);
  fprintf(stderr, "%s:   buzzer2Repeat                   = %d\n", logLevel, settings.buzzer2Repeat);
  fprintf(stderr, "%s:   dataTreatmentRecoverFromError   = %d\n", logLevel, settings.dataTreatmentRecoverFromError);
  fprintf(stderr, "%s:   cashDrawerSetting               = %d\n", logLevel, settings.cashDrawerSetting);
  fprintf(stderr, "%s:   cashDrawer1PulseWidth           = %d\n", logLevel, settings.cashDrawer1PulseWidth);
  fprintf(stderr, "%s:   resolution                      = %d\n", logLevel, settings.resolution);
  fprintf(stderr, "%s:   resolutionX                     = %d\n", logLevel, settings.resolutionX);
  fprintf(stderr, "%s:   resolutionY                     = %d\n", logLevel, settings.resolutionY);
  fprintf(stderr, "%s:   bytesPerScanLine                = %d\n", logLevel, settings.bytesPerScanLine);
  fprintf(stderr, "%s:   bytesPerScanLineStd             = %d\n", logLevel, settings.bytesPerScanLineStd);
  fprintf(stderr, "%s:   bytesPerHorizDot                = %d\n", logLevel, settings.bytesPerHorizDot);

  fprintf(stderr, "%s: ================================\n",       logLevel);

  return;
}

// define printer initialize command
// timing: jobSetup.0
// models: all
static const struct command printerInitializeCommand =
{2,(char[2]){0x1b,'@'}};

// NSB Disable
// timing: jobSetup0.1
// models: SP712, SP717, SP742, SP747, SP512, SP542
static const struct command invalidateNsb =
{4,(char[4]){0x1b,0x1e,0x61,48}};

// Station setting command
// timing: jobSetup.0.2
// models: HSP7000S, HSP7000V
static const struct command stationSettingCommand [3] =         // HSP7000S, HSP7000V
{{4,(char[4]){0x1b,'+','A','0'}},                               // Receipt
 {4,(char[4]){0x1b,'+','A','3'}},                               // Slip 
 {4,(char[4]){0x1b,'+','A','4'}}};                              // Validation

// define printable width setting command
// timing: jobSetup.0.3
// models: all
static const struct command printableWidthCommand [10] =        // SP700 / HSP7000S, HSP7000V
{{4,(char[4]){0x1b,0x1e,'A',0x00}},                             // 63mm  / 85mm
 {4,(char[4]){0x1b,0x1e,'A',0x01}},                             // 48mm  / 80mm
 {4,(char[4]){0x1b,0x1e,'A',0x02}},                             // 60mm  / 75mm
 {4,(char[4]){0x1b,0x1e,'A',0x03}},                             // 45mm  / 70mm
 {4,(char[4]){0x1b,0x1e,'A',0x04}},                             //       / 65mm
 {4,(char[4]){0x1b,0x1e,'A',0x05}},                             //       / 60mm
 {4,(char[4]){0x1b,0x1e,'A',0x06}},                             //       / 55mm
 {4,(char[4]){0x1b,0x1e,'A',0x07}},                             //       / 50mm
 {4,(char[4]){0x1b,0x1e,'A',0x08}},                             //       / 45mm
 {4,(char[4]){0x1b,0x1e,'A',0x09}}};                            //       / 40mm

// define bidirectional printing commands
// timing: jobSetup.1
// models: all
static const struct command bidiPrintingCommand [2] =
{{3,(char[3]){0x1b,'U','0'}},                              // 0 bidirectional
 {3,(char[3]){0x1b,'U','1'}}};                             // 1 unidirectional

// define buzzer 1 on time commands
// timing: jobSetup.2.1
// models: SP712/742/717/747
static const struct command buzzer1OnTimeCommand [8] =
{{6,(char[6]){0x1b,0x1d,0x19,0x11,'1',0x01}},                     //  0 20millis
 {6,(char[6]){0x1b,0x1d,0x19,0x11,'1',0x02}},                     //  1 40millis
 {6,(char[6]){0x1b,0x1d,0x19,0x11,'1',0x05}},                     //  2 100millis
 {6,(char[6]){0x1b,0x1d,0x19,0x11,'1',0x0A}},                     //  3 200millis
 {6,(char[6]){0x1b,0x1d,0x19,0x11,'1',0x19}},                     //  4 500millis
 {6,(char[6]){0x1b,0x1d,0x19,0x11,'1',0x32}},                     //  5 1000millis
 {6,(char[6]){0x1b,0x1d,0x19,0x11,'1',0x64}},                     //  6 2000millis
 {6,(char[6]){0x1b,0x1d,0x19,0x11,'1',(char)0xFA}}};              //  7 5000millis

// define buzzer 1 off time commands
// timing: jobSetup.2.2
// models: SP712/742/717/747
static const struct command buzzer1OffTimeCommand [8] =
{{1,(char[1]){0x01}},                                             //  0 20millis
 {1,(char[1]){0x02}},                                             //  1 40millis
 {1,(char[1]){0x05}},                                             //  2 100millis
 {1,(char[1]){0x0A}},                                             //  3 200millis
 {1,(char[1]){0x19}},                                             //  4 500millis
 {1,(char[1]){0x32}},                                             //  5 1000millis
 {1,(char[1]){0x64}},                                             //  6 2000millis
 {1,(char[1]){(char)0xFA}}};                                      //  7 5000millis

// define buzzer 1 fire commands
// timing: jobSetup.2.3 or endJob.3.1
// models: SP712/742/717/747
static const struct command buzzer1SettingCommand [7] =
{{7,(char[7]){0x1b,0x1d,0x19,0x12,'1',0x01,0x00}},                //  0 buzzer 1 fire (repeat = 1)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'1',0x02,0x00}},                //  1 buzzer 1 fire (repeat = 2)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'1',0x03,0x00}},                //  2 buzzer 1 fire (repeat = 3)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'1',0x05,0x00}},                //  3 buzzer 1 fire (repeat = 5)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'1',0x0A,0x00}},                //  4 buzzer 1 fire (repeat = 10)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'1',0x0F,0x00}},                //  5 buzzer 1 fire (repeat = 15)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'1',0x14,0x00}}};               //  6 buzzer 1 fire (repeat = 20)

// define buzzer 2 on time commands
// timing: jobSetup.2.4
// models: SP712/742/717/747
static const struct command buzzer2OnTimeCommand [8] =
{{6,(char[6]){0x1b,0x1d,0x19,0x11,'2',0x01}},                     //  0 20millis
 {6,(char[6]){0x1b,0x1d,0x19,0x11,'2',0x02}},                     //  1 40millis
 {6,(char[6]){0x1b,0x1d,0x19,0x11,'2',0x05}},                     //  2 100millis
 {6,(char[6]){0x1b,0x1d,0x19,0x11,'2',0x0A}},                     //  3 200millis
 {6,(char[6]){0x1b,0x1d,0x19,0x11,'2',0x19}},                     //  4 500millis
 {6,(char[6]){0x1b,0x1d,0x19,0x11,'2',0x32}},                     //  5 1000millis
 {6,(char[6]){0x1b,0x1d,0x19,0x11,'2',0x64}},                     //  6 2000millis
 {6,(char[6]){0x1b,0x1d,0x19,0x11,'2',(char)0xFA}}};              //  7 5000millis

// define buzzer 2 off time commands
// timing: jobSetup.2.5
// models: SP712/742/717/747
static const struct command buzzer2OffTimeCommand [8] =
{{1,(char[1]){0x01}},                                             //  0 20millis
 {1,(char[1]){0x02}},                                             //  1 40millis
 {1,(char[1]){0x05}},                                             //  2 100millis
 {1,(char[1]){0x0A}},                                             //  3 200millis
 {1,(char[1]){0x19}},                                             //  4 500millis
 {1,(char[1]){0x32}},                                             //  5 1000millis
 {1,(char[1]){0x64}},                                             //  6 2000millis
 {1,(char[1]){(char)0xFA}}};                                      //  7 5000millis

// define buzzer 2 fire commands
// timing: jobSetup.2.6 or endJob.3.2
// models: SP712/742/717/747
static const struct command buzzer2SettingCommand [7] =
{{7,(char[7]){0x1b,0x1d,0x19,0x12,'2',0x01,0x00}},                //  0 buzzer 2 fire (repeat = 1)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'2',0x02,0x00}},                //  1 buzzer 2 fire (repeat = 2)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'2',0x03,0x00}},                //  2 buzzer 2 fire (repeat = 3)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'2',0x05,0x00}},                //  3 buzzer 2 fire (repeat = 5)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'2',0x0A,0x00}},                //  4 buzzer 2 fire (repeat = 10)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'2',0x0F,0x00}},                //  5 buzzer 2 fire (repeat = 15)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'2',0x14,0x00}}};               //  6 buzzer 2 fire (repeat = 20)

// define cash drawer 1 pulse width commands
// timing: jobSetup.3
// models: all
static const struct command cashDrawer1PulseWidthCommand [13] =
{{4,(char[4]){0x1b,0x07,0x01,0x14}},                              //  0 10millis
 {4,(char[4]){0x1b,0x07,0x0a,0x14}},                              //  1 100millis
 {4,(char[4]){0x1b,0x07,0x14,0x14}},                              //  2 200millis
 {4,(char[4]){0x1b,0x07,0x1e,0x14}},                              //  3 300millis
 {4,(char[4]){0x1b,0x07,0x28,0x14}},                              //  4 400millis
 {4,(char[4]){0x1b,0x07,0x32,0x14}},                              //  5 500millis
 {4,(char[4]){0x1b,0x07,0x3c,0x14}},                              //  6 600millis
 {4,(char[4]){0x1b,0x07,0x46,0x14}},                              //  7 700millis
 {4,(char[4]){0x1b,0x07,0x50,0x14}},                              //  8 800millis
 {4,(char[4]){0x1b,0x07,0x5a,0x14}},                              //  9 900millis
 {4,(char[4]){0x1b,0x07,0x64,0x14}},                              // 10 1000millis
 {4,(char[4]){0x1b,0x07,0x6e,0x14}},                              // 11 1100millis
 {4,(char[4]){0x1b,0x07,0x78,0x14}}};                             // 12 1200millis

// define cash drawer open commands
// timing: jobSetup.4
// models: all
static const struct command cashDrawerSettingCommand [4] =
{{0,NULL},                                 //  0 do not open
 {1,(char[1]){0x07}},                      //  1 open drawer 1
 {1,(char[1]){0x1a}},                      //  2 open drawer 2
 {2,(char[2]){0x07,0x1a}}};                //  3 open drawers 1 and 2

// define data Treatment Recover From Error command
// timing: jobSetup.4.5
// models: SP712/742/717/747
static const struct command dataTreatmentRecoverFromErrorCommand[3] =
{{1,(char[1]){0x00}},                              // 0 Store Data
 {6,(char[6]){0x1b,0x1d,0x03,0x03,0x00,0x00}},     // 1 Clear Data Start
 {6,(char[6]){0x1b,0x1d,0x03,0x04,0x00,0x00}}};    // 2 Clear Data Fnish

// define begin ignore sequence command
// timing: jobSetup.5
// models: all
static const struct command beginIgnoreSeqCommand =
{2,(char[2]){0x1b,'B'}};

#define PAGETYPE_RECEIPT 0
#define PAGETYPE_TICKET  1

// define start page command
// timing: pageSetup.0
// models: all
static const struct command startPageCommand =
{0,NULL};

// define page cut type commands
// timing: pageSetup.1
// models: all
static const struct command pageCutTypeCommand [4] =
{{2,(char[2]){(char)0xff,0x00}},              // 0 no cut, no action
 {2,(char[2]){0x03,0x00}},                    // 1 partial cut
 {2,(char[2]){0x02,0x00}},                    // 2 full cut
 {2,(char[2]){ 't',0x00}}};                   // 3 tear bar

// define end page command
// timing: endPage.0
// models: all
static const struct command endPageCommand =
{3,(char[3]){0x0a,0x1b,'d'}};

// define release action command
// timing: endPage.1
// models: HSP7000S, HSP7000V
static const struct command releaseActionCommand =
{3, (char[3]){0x1b,0x0c,'4'}};

// define end job command
// timing: endJob.0
// models: all
static const struct command endJobCommand =
{1,(char[1]){(char)0xff}};

// define doc cut type commands
// timing: endJob.2
// models: all
static const struct command docCutTypeCommand [4] =
{{2,(char[2]){0x0a,0x12}},                                    // 0 no cut, no action
 {3,(char[3]){0x1b,'d',0x03}},               // 1 partial cut
 {3,(char[3]){0x1b,'d',0x02}},                // 2 full cut
 {3,(char[3]){0x1b,'d', 't'}}};               // 3 tear bar

void outputCommand(struct command output)
{
    int i = 0;

    for (; i < output.length; i++)
    {
        putchar(output.command[i]);
    }
}

void outputAsciiEncodedLength(int length)
{
    printf("%d",length);
}

void outputDummyData()
{
    int i = 0;

    for (; i < 128; i++)
    {
        putchar(0x00);
    }
}

void outputNullTerminator()
{
    putchar(0x00);
}

int getOptionChoiceIndex(const char * choiceName, ppd_file_t * ppd)
{
    ppd_choice_t * choice;
    ppd_option_t * option;

    choice = PPDFINDMARKEDCHOICE(ppd, choiceName);
    if (choice == NULL)
    {
        if ((option = PPDFINDOPTION(ppd, choiceName))          == NULL) return -1;
        if ((choice = PPDFINDCHOICE(option,option->defchoice)) == NULL) return -1;
    }

    fprintf(stderr, "DEBUG:getOptionChoiceIndex: %s = %d\n", choiceName, atoi(choice->choice));

    return atoi(choice->choice);
}

void getPageWidthPageHeight(ppd_file_t * ppd, struct settings_ * settings)
{
    ppd_choice_t * choice;
    ppd_option_t * option;

    char width[20];
    int widthIdx;

    char height[20];
    int heightIdx;

    char * pageSize;
    int idx;

    int state;

    choice = PPDFINDMARKEDCHOICE(ppd, "PageSize");
    if (choice == NULL)
    {
        option = PPDFINDOPTION(ppd, "PageSize");
        choice = PPDFINDCHOICE(option,option->defchoice);
    }

    widthIdx = 0;
    memset(width, 0x00, sizeof(width));

    heightIdx = 0;
    memset(height, 0x00, sizeof(height));

    pageSize = choice->choice;
    idx = 0;

    state = 0; // 0 = init, 1 = width, 2 = height, 3 = complete, 4 = fail

    while (pageSize[idx] != 0x00)
    {
        if (state == 0)
        {
            if (pageSize[idx] == 'X')
            {
                state = 1;

                idx++;
                continue;
            }
        }
        else if (state == 1)
        {
            if ((pageSize[idx] >= '0') && (pageSize[idx] <= '9'))
            {
                width[widthIdx++] = pageSize[idx];

                idx++;
                continue;
            }
            else if (pageSize[idx] == 'D')
            {
                width[widthIdx++] = '.';

                idx++;
                continue;
            }
            else if (pageSize[idx] == 'M')
            {
                idx++;
                continue;
            }
            else if (pageSize[idx] == 'Y')
            {
                state = 2;

                idx++;
                continue;
            }
        }
        else if (state == 2)
        {
            if ((pageSize[idx] >= '0') && (pageSize[idx] <= '9'))
            {
                height[heightIdx++] = pageSize[idx];

                idx++;
                continue;
            }
            else if (pageSize[idx] == 'D')
            {
                height[heightIdx++] = '.';

                idx++;
                continue;
            }
            else if (pageSize[idx] == 'M')
            {
                state = 3;
                break;
            }
        }

        state = 4;
        break;
    }

    if (state == 3)
    {
        settings->pageWidth = atof(width);
        settings->pageHeight = atof(height);
    }
    else
    {
        settings->pageWidth = 0;
        settings->pageHeight = 0;
    }
}

void initializeSettings(char * commandLineOptionSettings, struct settings_ * settings)
{
    ppd_file_t *    ppd         = NULL;
    cups_option_t * options     = NULL;
    int             numOptions  = 0;
    int             modelNumber = 0;

    ppd = PPDOPENFILE(getenv("PPD"));

    PPDMARKDEFAULTS(ppd);

    numOptions = CUPSPARSEOPTIONS(commandLineOptionSettings, 0, &options);
    if ((numOptions != 0) && (options != NULL))
    {
        CUPSMARKOPTIONS(ppd, numOptions, options);

        CUPSFREEOPTIONS(numOptions, options);
    }

    memset(settings, 0x00, sizeof(struct settings_));

    modelNumber = settings->modelNumber = ppd->model_number;

    switch(modelNumber)
    {
        case SP542:
        case SP512:
        case SP747:
        case SP742:
        case SP717:
        case SP712:
                settings->pageType              = getOptionChoiceIndex("PageType"             , ppd);

            break;

        case HSP7000S:
        case HSP7000V:
                settings->pageType              = PAGETYPE_TICKET;

            break;
    }

    settings->bidirectional         = getOptionChoiceIndex("BidiPrinting"         , ppd);

    switch(modelNumber)
    {
        case SP542:
        case SP512:
        case SP747:
        case SP742:
        case SP717:
        case SP712:
                settings->pageCutType           = getOptionChoiceIndex("PageCutType"          , ppd);
                settings->docCutType            = getOptionChoiceIndex("DocCutType"           , ppd);

            break;

        case HSP7000S:
        case HSP7000V:
                settings->pageCutType           = 0;
                settings->docCutType            = 0;

            break;
    }

    settings->cashDrawerSetting     = getOptionChoiceIndex("CashDrawerSetting"    , ppd);
    settings->cashDrawer1PulseWidth = getOptionChoiceIndex("CashDrawer1PulseWidth", ppd);
    settings->resolution            = getOptionChoiceIndex("Resolution"           , ppd);

    switch (modelNumber)
    {
        case SP542:
        case SP512:
                settings->buzzer1Setting  = 0;
                settings->buzzer1OnTime   = 0;
                settings->buzzer1OffTime  = 0;
                settings->buzzer1Repeat   = 0;
                settings->buzzer2Setting  = 0;
                settings->buzzer2OnTime   = 0;
                settings->buzzer2OffTime  = 0;
                settings->buzzer2Repeat   = 0;

            break;

        case SP747:
        case SP717:
        case SP742:
        case SP712: 
        case HSP7000S:
        case HSP7000V: 
                settings->buzzer1Setting  = getOptionChoiceIndex("Buzzer1Setting"  , ppd);
                settings->buzzer1OnTime   = getOptionChoiceIndex("Buzzer1OnTime"   , ppd);
                settings->buzzer1OffTime  = getOptionChoiceIndex("Buzzer1OffTime"  , ppd);
                settings->buzzer1Repeat   = getOptionChoiceIndex("Buzzer1Repeat"   , ppd);
                settings->buzzer2Setting  = getOptionChoiceIndex("Buzzer2Setting"  , ppd);
                settings->buzzer2OnTime   = getOptionChoiceIndex("Buzzer2OnTime"   , ppd);
                settings->buzzer2OffTime  = getOptionChoiceIndex("Buzzer2OffTime"  , ppd);
                settings->buzzer2Repeat   = getOptionChoiceIndex("Buzzer2Repeat"   , ppd);

            break;
    }

    settings->dataTreatmentRecoverFromError  = getOptionChoiceIndex("DataTreatmentRecoverFromError" , ppd);

    switch (modelNumber)
    {
        case SP747:
        case SP717:
        case SP742:
        case SP712:
        case SP542:
        case SP512: 
                if (settings->resolution == RES170x72DPI)
                {
                    settings->resolutionX         = 170;
                    settings->resolutionY         = 72;
                    settings->bytesPerScanLine    = SP500_170DPI_MAX_WIDTH;
                    settings->bytesPerScanLineStd = SP500_170DPI_STD_WIDTH;
                    settings->bytesPerHorizDot    = 1;
                }
                else if (settings->resolution == RES85x72DPI)
                {
                    settings->resolutionX         = 85;
                    settings->resolutionY         = 72;
                    settings->bytesPerScanLine    = SP500_85DPI_MAX_WIDTH;
                    settings->bytesPerScanLineStd = SP500_85DPI_STD_WIDTH;
                    settings->bytesPerHorizDot    = 1;
                }

            break;

        case HSP7000S:
        case HSP7000V:
                if (settings->resolution == RES160x72DPI)
                {
                    settings->resolutionX         = 160;
                    settings->resolutionY         = 72;
                    settings->bytesPerScanLine    = HSP7000_160DPI_MAX_WIDTH;
                    settings->bytesPerScanLineStd = HSP7000_160DPI_STD_WIDTH;
                    settings->bytesPerHorizDot    = 1;
                }
                else if (settings->resolution == RES80x72DPI)
                {
                    settings->resolutionX         = 80;
                    settings->resolutionY         = 72;
                    settings->bytesPerScanLine    = HSP7000_80DPI_MAX_WIDTH;
                    settings->bytesPerScanLineStd = HSP7000_80DPI_STD_WIDTH;
                    settings->bytesPerHorizDot    = 1;
                }

            break;
    }


    getPageWidthPageHeight(ppd, settings);

    PPDCLOSE(ppd);
}

void jobSetup(struct settings_ settings, char *argv[])
{
    outputCommand(printerInitializeCommand);


    if (settings.modelNumber == SP747)
      {
	if (settings.dataTreatmentRecoverFromError == DATACANCEL_DOC )
	  {
	    outputCommand(dataTreatmentRecoverFromErrorCommand[1]);
	  }
      }

    if (settings.modelNumber == SP717)
      {
	if (settings.dataTreatmentRecoverFromError == DATACANCEL_DOC )
	  {
	    outputCommand(dataTreatmentRecoverFromErrorCommand[1]);
	  }
      }

    if (settings.modelNumber == SP742)
      {
	if (settings.dataTreatmentRecoverFromError == DATACANCEL_DOC )
	  {
	    outputCommand(dataTreatmentRecoverFromErrorCommand[1]);
	  }
      }

    if (settings.modelNumber == SP712)
      {
	if (settings.dataTreatmentRecoverFromError == DATACANCEL_DOC )
	  {
	    outputCommand(dataTreatmentRecoverFromErrorCommand[1]);
	  }
      }

    #ifdef __APPLE__
    const char *deviceUri = cupsBackendDeviceURI(argv);
    if (deviceUri != NULL)
    {
        if ( strstr(deviceUri, "usb://") )
        {
            switch (settings.modelNumber)
            {
                case SP747:
                case SP717:
                case SP742:
                case SP712:
                case SP512:
                case SP542:
                case HSP7000S:
                case HSP7000V:
                        outputCommand(invalidateNsb);
                    break;
            }
        }
    }
    #endif

    switch (settings.modelNumber)
    {
        case HSP7000S:
                fprintf(stderr, "DEBUG: Slip \n");//test
                outputCommand(stationSettingCommand[1]);

            break;

        case HSP7000V:
                fprintf(stderr, "DEBUG: Validation \n");//test
                outputCommand(stationSettingCommand[2]);

            break;
    }

   switch (settings.modelNumber)
   {
        case SP747:
        case SP717:
        case SP742:
        case SP712:
                if      (settings.pageWidth == 63.0f) outputCommand(printableWidthCommand[0]);
                else if (settings.pageWidth == 48.0f) outputCommand(printableWidthCommand[1]);
                else if (settings.pageWidth == 60.0f) outputCommand(printableWidthCommand[2]);
                else if (settings.pageWidth == 45.0f) outputCommand(printableWidthCommand[3]);
                else outputCommand(printableWidthCommand[0]);

            break;

        case HSP7000S:
        case HSP7000V:
                if      (settings.pageWidth == 85.0f) outputCommand(printableWidthCommand[0]);
                else if (settings.pageWidth == 80.0f) outputCommand(printableWidthCommand[1]);
                else if (settings.pageWidth == 75.0f) outputCommand(printableWidthCommand[2]);
                else if (settings.pageWidth == 70.0f) outputCommand(printableWidthCommand[3]);
                else if (settings.pageWidth == 65.0f) outputCommand(printableWidthCommand[4]);
                else if (settings.pageWidth == 60.0f) outputCommand(printableWidthCommand[5]);
                else if (settings.pageWidth == 55.0f) outputCommand(printableWidthCommand[6]);
                else if (settings.pageWidth == 50.0f) outputCommand(printableWidthCommand[7]);
                else if (settings.pageWidth == 45.0f) outputCommand(printableWidthCommand[8]);
                else if (settings.pageWidth == 40.0f) outputCommand(printableWidthCommand[9]);
                else outputCommand(printableWidthCommand[0]);

            break;
    }

    switch (settings.modelNumber)
    {
        case SP747:
        case SP717:
        case SP742:
        case SP712:
        case HSP7000S:
        case HSP7000V:
                outputCommand(bidiPrintingCommand[settings.bidirectional]);

            break;
    }

    if(settings.buzzer1Setting == BUZZER_DOC_TOP)
    {
        outputCommand(buzzer1OnTimeCommand[settings.buzzer1OnTime]);

        outputCommand(buzzer1OffTimeCommand[settings.buzzer1OffTime]);

        outputCommand(buzzer1SettingCommand[settings.buzzer1Repeat]);
    }

    if(settings.buzzer2Setting == BUZZER_DOC_TOP)
    {
        outputCommand(buzzer2OnTimeCommand[settings.buzzer2OnTime]);

        outputCommand(buzzer2OffTimeCommand[settings.buzzer2OffTime]);

        outputCommand(buzzer2SettingCommand[settings.buzzer2Repeat]);
    }

    if(settings.cashDrawerSetting != 0)
    {
        outputCommand(cashDrawer1PulseWidthCommand[settings.cashDrawer1PulseWidth]);

        outputCommand(cashDrawerSettingCommand[settings.cashDrawerSetting]);
    }

    switch(settings.modelNumber)
    {
        case SP512:
        case SP542:
        case SP712:
        case SP742:
        case SP717:
        case SP747:
                 outputCommand(beginIgnoreSeqCommand);

            break;
    }
}

void pageSetup(struct settings_ settings, cups_page_header2_t header)
{
    switch(settings.modelNumber)
    {
        case SP512:
        case SP542:
        case SP712:
        case SP742:
        case SP717:
        case SP747:
                outputCommand(startPageCommand);

            break;
    }
    
    switch (settings.modelNumber)
    {
        case SP542:
        case SP512:
        case SP747:
        case SP742:
        case SP717:
        case SP712: 
                outputCommand(pageCutTypeCommand[settings.pageCutType]);

            break;
    }
}

void endPage(struct settings_ settings)
{
    switch(settings.modelNumber)
    {
        case SP542:
        case SP512:
        case SP747:
        case SP742:
        case SP717:
        case SP712: 
                outputCommand(endPageCommand);

            break;
    }

    switch(settings.modelNumber)
    {
        case HSP7000S:
        case HSP7000V:
                outputCommand(releaseActionCommand);

            break;
    }
}

void endJob(struct settings_ settings, char *argv[])
{
    switch(settings.modelNumber)
    {
        case SP542:
        case SP512:
        case SP747:
        case SP742:
        case SP717:
        case SP712:
                outputCommand(endJobCommand);

            break;
    }

    switch(settings.modelNumber)
    {
        case SP542:
        case SP512:
        case SP747:
        case SP742:
        case SP717:
        case SP712:
                outputCommand(docCutTypeCommand[settings.docCutType]);

            break;
    }

    switch(settings.modelNumber)
    {
        case HSP7000S:
        case HSP7000V:
                outputCommand(stationSettingCommand[0]);

            break;
    }

    if(settings.buzzer1Setting == BUZZER_DOC_BTM)
    {
        outputCommand(buzzer1OnTimeCommand[settings.buzzer1OnTime]);

        outputCommand(buzzer1OffTimeCommand[settings.buzzer1OffTime]);

        outputCommand(buzzer1SettingCommand[settings.buzzer1Repeat]);
    }

    if(settings.buzzer2Setting == BUZZER_DOC_BTM)
    {
        outputCommand(buzzer2OnTimeCommand[settings.buzzer2OnTime]);

        outputCommand(buzzer2OffTimeCommand[settings.buzzer2OffTime]);

        outputCommand(buzzer2SettingCommand[settings.buzzer2Repeat]);
    }

    if (settings.modelNumber == SP747)
      {
	if (settings.dataTreatmentRecoverFromError == DATACANCEL_DOC)
	  {
	    outputCommand(dataTreatmentRecoverFromErrorCommand[2]);
	  }
      }

    if (settings.modelNumber == SP717)
      {
	if (settings.dataTreatmentRecoverFromError == DATACANCEL_DOC)
	  {
	    outputCommand(dataTreatmentRecoverFromErrorCommand[2]);
	  }
      }

    if (settings.modelNumber == SP742)
      {
	if (settings.dataTreatmentRecoverFromError == DATACANCEL_DOC)
	  {
	    outputCommand(dataTreatmentRecoverFromErrorCommand[2]);
	  }
      }

    if (settings.modelNumber == SP712)
      {
	if (settings.dataTreatmentRecoverFromError == DATACANCEL_DOC)
	  {
	    outputCommand(dataTreatmentRecoverFromErrorCommand[2]);
	  }
      }

    #ifdef __APPLE__
    #else
    outputDummyData();
    #endif

}

#define GET_LIB_FN_OR_EXIT_FAILURE(fn_ptr,lib,fn_name)                                      \
{                                                                                           \
    fn_ptr = dlsym(lib, fn_name);                                                           \
    if ((dlerror()) != NULL)                                                                \
    {                                                                                       \
        fputs("ERROR: required fn not exported from dynamically loaded libary\n", stderr);  \
        if (libCupsImage != 0) dlclose(libCupsImage);                                       \
        if (libCups      != 0) dlclose(libCups);                                            \
        return EXIT_FAILURE;                                                                \
    }                                                                                       \
}

#ifdef RPMBUILD
#define CLEANUP                                     \
{                                                   \
    if (rasterData   != NULL) free(rasterData);     \
    if (bitImageData != NULL) free(bitImageData);   \
    CUPSRASTERCLOSE(ras);                           \
    if (fd != 0)                                    \
    {                                               \
        close(fd);                                  \
    }                                               \
    dlclose(libCupsImage);                          \
    dlclose(libCups);                               \
}
#else
#define CLEANUP                                     \
{                                                   \
    if (rasterData   != NULL) free(rasterData);     \
    if (bitImageData != NULL) free(bitImageData);   \
    CUPSRASTERCLOSE(ras);                           \
    if (fd != 0)                                    \
    {                                               \
        close(fd);                                  \
    }                                               \
}
#endif

int main(int argc, char *argv[])
{
    int                 fd                      = 0;        /* File descriptor providing CUPS raster data                                           */
    cups_raster_t *     ras                     = NULL;     /* Raster stream for printing                                                           */
    cups_page_header2_t header;                             /* CUPS Page header                                                                     */
    int                 page                    = 0;        /* Current page                                                                         */

    int                 y                       = 0;        /* Vertical position in page 0 <= y <= header.cupsHeight                                */
    int                 vertDot                 = 0;        /* Vertical position in 8-dot heigh scan line 0 <= vertDot <= 8                         */
    int                 i                       = 0;        /* Horizontal byte index in CUPS raster data 0 <= i < header.cupsBytesPerLine           */

    unsigned char *     rasterData              = NULL;     /* CUPS raster data [0 ... header.cupsBytesPerLine - 1]  -- 1 byte = 8 horizontal dots  */
    unsigned char *     bitImageData            = NULL;     /* Line mode bit image data [0 ... header.cupsWidth - 1] -- 1 byte = 8 vertical dots    */

    int                 scanLineBlank           = FALSE;    /* TRUE iff 1-dot heigh scan line in CUPS raster data is blank                          */
    int                 blankScanLines          = 0;        /* Count of blank 1-dot heigh scan lines 0 <= blankScanLines <= 8                       */
    int                 blankPasses             = 0;        /* Count of blank 8-dot heigh scan lines                                                */

    struct settings_    settings;                           /* Configuration settings                                                               */

#ifdef RPMBUILD
    void * libCupsImage = NULL;                             /* Pointer to libCupsImage library                                                      */
    void * libCups      = NULL;                             /* Pointer to libCups library                                                           */

    libCups = dlopen ("libcups.so", RTLD_NOW | RTLD_GLOBAL);
    if (! libCups)
    {
        fputs("ERROR: libcups.so load failure\n", stderr);
        return EXIT_FAILURE;
    }

    libCupsImage = dlopen ("libcupsimage.so", RTLD_NOW | RTLD_GLOBAL);
    if (! libCupsImage)
    {
        fputs("ERROR: libcupsimage.so load failure\n", stderr);
        dlclose(libCups);
        return EXIT_FAILURE;
    }

    GET_LIB_FN_OR_EXIT_FAILURE(ppdClose_fn,             libCups,      "ppdClose"             );
    GET_LIB_FN_OR_EXIT_FAILURE(ppdFindChoice_fn,        libCups,      "ppdFindChoice"        );
    GET_LIB_FN_OR_EXIT_FAILURE(ppdFindMarkedChoice_fn,  libCups,      "ppdFindMarkedChoice"  );
    GET_LIB_FN_OR_EXIT_FAILURE(ppdFindOption_fn,        libCups,      "ppdFindOption"        );
    GET_LIB_FN_OR_EXIT_FAILURE(ppdMarkDefaults_fn,      libCups,      "ppdMarkDefaults"      );
    GET_LIB_FN_OR_EXIT_FAILURE(ppdOpenFile_fn,          libCups,      "ppdOpenFile"          );
    GET_LIB_FN_OR_EXIT_FAILURE(cupsFreeOptions_fn,      libCups,      "cupsFreeOptions"      );
    GET_LIB_FN_OR_EXIT_FAILURE(cupsParseOptions_fn,     libCups,      "cupsParseOptions"     );
    GET_LIB_FN_OR_EXIT_FAILURE(cupsMarkOptions_fn,      libCups,      "cupsMarkOptions"      );
    GET_LIB_FN_OR_EXIT_FAILURE(cupsRasterOpen_fn,       libCupsImage, "cupsRasterOpen"       );
    GET_LIB_FN_OR_EXIT_FAILURE(cupsRasterReadHeader_fn, libCupsImage, "cupsRasterReadHeader2" );
    GET_LIB_FN_OR_EXIT_FAILURE(cupsRasterReadPixels_fn, libCupsImage, "cupsRasterReadPixels" );
    GET_LIB_FN_OR_EXIT_FAILURE(cupsRasterClose_fn,      libCupsImage, "cupsRasterClose"      );
#endif

    if (argc < 6 || argc > 7)
    {
        fputs("ERROR: rastertostar job-id user title copies options [file]\n", stderr);

        #ifdef RPMBUILD
            dlclose(libCupsImage);
            dlclose(libCups);
        #endif

        return EXIT_FAILURE;
    }

    if (argc == 7)
    {
        if ((fd = open(argv[6], O_RDONLY)) == -1)
        {
            perror("ERROR: Unable to open raster file - ");
            sleep(1);
            
            #ifdef RPMBUILD
                dlclose(libCupsImage);
                dlclose(libCups);
            #endif

            return EXIT_FAILURE;
        }
    }
    else
    {
        fd = 0;
    }

    initializeSettings(argv[5], &settings);

    jobSetup(settings,argv);

    ras = CUPSRASTEROPEN(fd, CUPS_RASTER_READ);

    page = 0;

    while (CUPSRASTERREADHEADER(ras, &header))
    {
        if ((header.cupsHeight == 0) || (header.cupsBytesPerLine == 0))
        {
            break;
        }

        if (bitImageData == NULL)
        {
            bitImageData = malloc(MAX(header.cupsWidth * (unsigned int)settings.bytesPerHorizDot, (unsigned long)settings.bytesPerScanLine));
            if (bitImageData == NULL)
            {
                CLEANUP;
                return EXIT_FAILURE;
            }
            memset(bitImageData, 0x00, settings.bytesPerScanLine);
        }

        if (rasterData == NULL)
        {
            rasterData = malloc(header.cupsBytesPerLine);
            if (rasterData == NULL)
            {
                CLEANUP;
                return EXIT_FAILURE;
            }
        }

        pageSetup(settings, header);

        page++;
        fprintf(stderr, "PAGE: %d %d\n", page, header.NumCopies);

        for (vertDot = 8, y = 0; y < header.cupsHeight; y++)
        {
            if ((y & 127) == 0)
            {
                fprintf(stderr, "INFO: Printing page %d, %d%% complete...\n", page, (100 * (unsigned int)y / header.cupsHeight));            }

            if (CUPSRASTERREADPIXELS(ras, rasterData, header.cupsBytesPerLine) < 1)
            {
                break;
            }

            vertDot--;

            scanLineBlank = TRUE;
            for (i = 0; i < header.cupsBytesPerLine; i++)
            {
                scanLineBlank &= (rasterData[i] == 0);

                switch (header.cupsWidth - ((unsigned int)i * 8))
                {
                    default: bitImageData[(i * 8) + 7] |= ((rasterData[i] & 0x01) >> 0) << vertDot;
                    case 6:  bitImageData[(i * 8) + 6] |= ((rasterData[i] & 0x02) >> 1) << vertDot;
                    case 5:  bitImageData[(i * 8) + 5] |= ((rasterData[i] & 0x04) >> 2) << vertDot;
                    case 4:  bitImageData[(i * 8) + 4] |= ((rasterData[i] & 0x08) >> 3) << vertDot;
                    case 3:  bitImageData[(i * 8) + 3] |= ((rasterData[i] & 0x10) >> 4) << vertDot;
                    case 2:  bitImageData[(i * 8) + 2] |= ((rasterData[i] & 0x20) >> 5) << vertDot;
                    case 1:  bitImageData[(i * 8) + 1] |= ((rasterData[i] & 0x40) >> 6) << vertDot;
                    case 0:  bitImageData[(i * 8) + 0] |= ((rasterData[i] & 0x80) >> 7) << vertDot;
                }
            }

            if (scanLineBlank == TRUE)
            {
                blankScanLines++;
            }

            if (vertDot == 0)
            {
                vertDot = 8;

                if (blankScanLines == 8)
                {
                    blankPasses++;
                }
                else
                {
                    while (blankPasses > 0)
                    {
                        putchar(0x1b);
                        putchar('J');
                        putchar(0x08);

                        blankPasses--;
                    }

                    switch(settings.modelNumber)
                    {
                        case SP512:
                        case SP542:
                        case SP712:
                        case SP742:
                        case SP717:
                        case SP747:
                                if (settings.resolutionX == 170)
                                {
                                    putchar(0x1b);
                                    putchar('L');
                                    putchar(settings.bytesPerScanLine % 256); // 420 % 256 = 164
                                    putchar(settings.bytesPerScanLine / 256); // 420 / 256 = 1

                                    for (i = 0; i < settings.bytesPerScanLine; i++)
                                    {
                                        putchar(bitImageData[i]);
                                    }
                                }
                                else if (settings.resolutionX == 85)
                                {
                                    putchar(0x1b);
                                    putchar('K');
                                    putchar(settings.bytesPerScanLine % 256); // 210 % 256 = 210
                                    putchar(settings.bytesPerScanLine / 256); // 210 / 256 = 0

                                    for (i = 0; i < settings.bytesPerScanLine; i++)
                                    {
                                        putchar(bitImageData[i]);
                                    }
                                }

                            break;

                        case HSP7000S:
                        case HSP7000V:
                                if (settings.resolutionX == 160)
                                {
                                    putchar(0x1b);
                                    putchar('L');
                                    putchar(settings.bytesPerScanLine % 256); // 540 % 256 = 28
                                    putchar(settings.bytesPerScanLine / 256); // 540 / 256 = 2

                                    for (i = 0; i < settings.bytesPerScanLine; i++)
                                    {
                                        putchar(bitImageData[i]);
                                    }
                                }
                                else if (settings.resolutionX == 80)
                                {
                                    putchar(0x1b);
                                    putchar('K');
                                    putchar(settings.bytesPerScanLine % 256); // 270 % 256 = 14
                                    putchar(settings.bytesPerScanLine / 256); // 270 / 256 = 1

                                    for (i = 0; i < settings.bytesPerScanLine; i++)
                                    {
                                        putchar(bitImageData[i]);
                                    }
                                }

                            break;
                    }

                    putchar(0x1b);
                    putchar('J');
                    putchar(0x08);
                }

                blankScanLines = 0;

                memset(bitImageData, 0x00, settings.bytesPerScanLine);
            }
        }

        if (vertDot != 8)
        {
            if (blankScanLines + vertDot == 8)
            {
                 blankPasses++;
            }
            else
            {
                while (blankPasses > 0)
                {
                    putchar(0x1b);
                    putchar('J');
                    putchar(0x08);

                    blankPasses--;
                }

                switch (settings.modelNumber)
                {
                    case SP512:
                    case SP542:
                    case SP712:
                    case SP742:
                    case SP717:
                    case SP747:
                            if (settings.resolutionX == 170)
                            {
                                putchar(0x1b);
                                putchar('L');
                                putchar(settings.bytesPerScanLine % 256); // 420 % 256 = 164
                                putchar(settings.bytesPerScanLine / 256); // 420 / 256 = 1 

                                for (i = 0; i < settings.bytesPerScanLine; i++)
                                {
                                    putchar(bitImageData[i]);
                                }
                            }
                            else if (settings.resolutionX == 85)
                            {
                                putchar(0x1b);
                                putchar('K');
                                putchar(settings.bytesPerScanLine % 256); // 210 % 256 = 210
                                putchar(settings.bytesPerScanLine / 256); // 210 / 256 = 0

                                for (i = 0; i < settings.bytesPerScanLine; i++)
                                {
                                    putchar(bitImageData[i]);
                                }
                            }

                        break;

                    case HSP7000S:
                    case HSP7000V:
                            if (settings.resolutionX == 160)
                            {
                                putchar(0x1b);
                                putchar('L');
                                putchar(settings.bytesPerScanLine % 256); // 540 % 256 = 28
                                putchar(settings.bytesPerScanLine / 256); // 540 / 256 = 2

                                for (i = 0; i < settings.bytesPerScanLine; i++)
                                {
                                    putchar(bitImageData[i]);
                                }
                            }
                            else if (settings.resolutionX == 80)
                            {
                                putchar(0x1b);
                                putchar('K');
                                putchar(settings.bytesPerScanLine % 256); // 270 % 256 = 14
                                putchar(settings.bytesPerScanLine / 256); // 270 / 256 = 1

                                for (i = 0; i < settings.bytesPerScanLine; i++)
                                {
                                    putchar(bitImageData[i]);
                                }
                            }

                        break;
                }

                putchar(0x1b);
                putchar('J');
                putchar(0x08);

                memset(bitImageData, 0x00, settings.bytesPerScanLine);
            }

            blankScanLines = 0;

            vertDot = 8;
        }

        if (settings.pageType == PAGETYPE_TICKET)
        {
            while (blankPasses--)
            {
                putchar(0x1b);
                putchar('J');
                putchar(0x08);
            }
        }

        endPage(settings);
    }

    endJob(settings, argv);

    CLEANUP;

    if (page == 0)
    {
        fputs("ERROR: No pages found!\n", stderr);
    }
    else
    {
        fputs("INFO: Ready to print.\n", stderr);
    }
    
    return (page == 0)?EXIT_FAILURE:EXIT_SUCCESS;
}

// end of rastertostarlm.c
