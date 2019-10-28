/*
 * Star Micronics
 *
 * CUPS Filter
 *
 * [ Linux ]
 * compile cmd: gcc -Wl,-rpath,/usr/lib -Wall -fPIC -O2 -o rastertostarm rastertostarm.c -lcupsimage -lcups
 * compile requires cups-devel-1.1.19-13.i386.rpm (version not neccessarily important?)
 * find cups-devel location here: http://rpmfind.net/linux/rpm2html/search.php?query=cups-devel&submit=Search+...&system=&arch=
 *
 * [ Mac OS X ]
 * compile cmd: gcc -Wall -fPIC -o rastertostarm rastertostarm.c -lcupsimage -lcups -arch i386 -arch x86_64
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
//#include <allreceipts.h>
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

#define MAX(a,b) ( ((a) > (b)) ? (a) : (b) )

#define FALSE 0
#define TRUE  (!FALSE)

// definitions for model number
#define SMT300i        3001
#define SMT400i        4001
#define SMS210i        2101
#define SMS220i        2201
#define SMS230i        2301
#define SML200         2000
#define SML204         2004
#define SML300         3000
#define SML304         3004
#define POP10          10
#define MCP20          20
#define MCP21          21
#define MCP30          30
#define MCP31          31


// definitions for printable width
#define SMT300i_MAX_WIDTH 72
#define SMT300i_STD_WIDTH 72
#define SMT400i_MAX_WIDTH 104
#define SMT400i_STD_WIDTH 104
#define SMS210i_MAX_WIDTH 48
#define SMS210i_STD_WIDTH 48
#define SMS220i_MAX_WIDTH 48
#define SMS220i_STD_WIDTH 48
#define SMS230i_MAX_WIDTH 48
#define SMS230i_STD_WIDTH 48
#define SML200_MAX_WIDTH 48
#define SML200_STD_WIDTH 48
#define SML300_MAX_WIDTH 72
#define SML300_STD_WIDTH 72
#define POP10_MAX_WIDTH 54
#define POP10_STD_WIDTH 48
#define MCP20_MAX_WIDTH 58
#define MCP20_STD_WIDTH 48
#define MCP30_MAX_WIDTH 72
#define MCP30_STD_WIDTH 72

#define FOCUS_LEFT      0
#define FOCUS_CENTER    1
#define FOCUS_RIGHT     2

#define DATACANCEL_NO_USE 0
#define DATACANCEL_DOC    1

#define BUZZER_NO_USE   0
#define BUZZER_DOC_TOP  1
#define BUZZER_DOC_BTM  2

#define MELODYSPEAKER_NO_USE 0
#define MELODYSPEAKER_DOC_TOP 1
#define MELODYSPEAKER_DOC_BTM 2

#define MELODYSPEAKER_AREA_1 0
#define MELODYSPEAKER_AREA_2 1

struct settings_
{
    int modelNumber;

    float pageWidth;
    float pageHeight;

    int pageType;
    int focusArea;

    int printSpeed;

    int pageCutType;
    int docCutType;

    int mediaType;
    int printDensity;

    int topMargin;

    int dataTreatmentRecoverFromError;

    int blackMarkDetection;

    int cashDrawerSetting;
    int cashDrawer1PulseWidth;

    int buzzer1Setting;
    int buzzer1OnTime;
    int buzzer1OffTime;
    int buzzer1Repeat;
    int buzzer2Setting;
    int buzzer2OnTime;
    int buzzer2OffTime;
    int buzzer2Repeat;
    
    int melodySpeakerSetting;
    int melodySpeakerSoundStorageArea;
    int melodySpeakerSoundNumber;
    int melodySpeakerSoundVolume;
    int melodySpeakerRepeat;

    int bytesPerScanLine;
    int bytesPerScanLineStd;
};

struct command
{
    int    length;
    char* command;
};

// define printer initialize command
// timing: jobSetup.0
// models: all
static const struct command printerInitializeCommand =
{2,(char[2]){0x1b,'@'}};

// NSB request Setting(USB Printer Class)
// timing: jobSetup.0.1
// models: all
/* unused
 static const struct command nsbRequestSettingCommand[2] =
   {{4,(char[4]){0x1b,0x1e,0x61,48}},  // Ignore  NSB and ASB req
    {4,(char[4]){0x1b,0x1e,0x61,50}}}; // Receive NSB and ASB req
 */

// define printable width setting command
// timing: jobSetup.0.5
// models: SM-L300(SM-L304),POP10,MCP30,MCP31,MCP20,MCP21
static const struct command printableWidthCommand [6] =         // SM-L300(SM-L304) / POP10   / MCP30,31    / MCP20,MCP21
{{4,(char[4]){0x1b,0x1e,'A',0x00}},                             // 72mm             / 48mm    / 72mm        / 48mm
 {4,(char[4]){0x1b,0x1e,'A',0x01}},                             // -                / 54mm    / -           / 54mm
 {4,(char[4]){0x1b,0x1e,'A',0x02}},                             // -                / -       / 48mm        /
 {4,(char[4]){0x1b,0x1e,'A',0x03}},                             // 50.8mm           / 50.8mm  / 50.8mm      / 50.8mm
 {4,(char[4]){0x1b,0x1e,'A',0x04}},                             // -                / -       / -           /
 {4,(char[4]){0x1b,0x1e,'A',0x05}}};                            // -                / -       / -           /
 
// define print density
// timing: jobSetup.0.8
// models: all
static const struct command printDensityCommand [4] =           //
{{4,(char[4]){0x1b,0x1e,'d','3' }},                             // Special
 {4,(char[4]){0x1b,0x1e,'d','2' }},                             // High
 {4,(char[4]){0x1b,0x1e,'d','0' }},                             // Medium
 {4,(char[4]){0x1b,0x1e,'d','1' }}};                            // Low

// for POP10,MCP20,MCP21,MCP30,MCP31 (adjust thermal printer format)
static const struct command printDensityCommand2 [7] =          // POP10,MCP20,MCP21 / MCP30,31
{{4,(char[4]){0x1b,0x1e,'d','6' }},                             // -                 / -3
 {4,(char[4]){0x1b,0x1e,'d','5' }},                             // -                 / -2
 {4,(char[4]){0x1b,0x1e,'d','4' }},                             // -                 / -1
 {4,(char[4]){0x1b,0x1e,'d','3' }},                             // Standard          / Standard
 {4,(char[4]){0x1b,0x1e,'d','2' }},                             // +1                / +1
 {4,(char[4]){0x1b,0x1e,'d','1' }},                             // +2                / +2
 {4,(char[4]){0x1b,0x1e,'d','0' }}};                            // +3                / +3

// define print speed commands
// timing: jobSetup.0.9
// models: SM-T300i,POP10,MCP30,MCP31
static const struct command printSpeedCommand [3] =       // SM-T300i                / POP10 / MCP30,31
{{4,(char[4]){0x1b,0x1e,'r','0'}},                        // High Speed(Low Quality) / High  / High
 {4,(char[4]){0x1b,0x1e,'r','1'}},                        // Low Speed(High Quality) / -     / Middle
 {4,(char[4]){0x1b,0x1e,'r','2'}}};                       // -                       / Low   / Low

 
// define cash drawer 1 pulse width commands
// timing: jobSetup.1
// models: POP10,MCP20,MCP21,MCP30,MCP31
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


// define topmargin commands
// timing: jobSetup.1
// models: MCP31
static const struct command topMarginCommand [10] =
{{4,(char[4]){0x1b,0x1e,'T',0x0b}},                               //  0 Disable(11mm)
 {4,(char[4]){0x1b,0x1e,'T',0x02}},                               //  1 2mm
 {4,(char[4]){0x1b,0x1e,'T',0x03}},                               //  2 3mm
 {4,(char[4]){0x1b,0x1e,'T',0x04}},                               //  3 4mm
 {4,(char[4]){0x1b,0x1e,'T',0x05}},                               //  4 5mm
 {4,(char[4]){0x1b,0x1e,'T',0x06}},                               //  5 6mm
 {4,(char[4]){0x1b,0x1e,'T',0x07}},                               //  6 7mm
 {4,(char[4]){0x1b,0x1e,'T',0x08}},                               //  7 8mm
 {4,(char[4]){0x1b,0x1e,'T',0x09}},                               //  8 9mm
 {4,(char[4]){0x1b,0x1e,'T',0x0a}}};                              //  9 10mm
 

// define cash drawer open commands
// timing: jobSetup.1.1
// models: POP10,MCP20,MCP21,MCP30,MCP31
static const struct command cashDrawerSettingCommand [4] =
{{0,NULL},                                 //  0 do not open
 {1,(char[1]){0x07}},                      //  1 open drawer 1
 {1,(char[1]){0x1a}},                      //  2 open drawer 2
 {2,(char[2]){0x07,0x1a}}};                //  3 open drawers 1 and 2
 
// define data Treatment Recover From Error command
// timing: jobSetup.1.7
// models: all
static const struct command dataTreatmentRecoverFromErrorCommand[3] =
{{1,(char[1]){0x00}},                              // 0 Store Data
 {6,(char[6]){0x1b,0x1d,0x03,0x03,0x00,0x00}},     // 1 Clear Data Start
 {6,(char[6]){0x1b,0x1d,0x03,0x04,0x00,0x00}}};    // 2 Clear Data Fnish

// define blackMarckDetection
// timing: jobSetup.0.9
static const struct command blackMarkDetectionCommand [3] =   // SM-T300i
{{5,(char[5]){0x1b,0x1e,'m','0',0x00}},                        // Disable
 {5,(char[5]){0x1b,0x1e,'m','1',0x00}},                        // Enable
 {5,(char[5]){0x1b,0x1e,'m','2',0x00}}};                       // Enable + Detection

static const struct command blackMarkFrontDetectionCommand [3] =   // SM-L200 front
{{5,(char[5]){0x1b,0x1e,'m','0',0x00}},                             // Disable
 {5,(char[5]){0x1b,0x1e,'m','1',0x00}},                             // Enable
 {5,(char[5]){0x1b,0x1e,'m','2',0x00}}};                            // Enable + Detection(SM-L200 front)

static const struct command blackMarkBackDetectionCommand [3] =   // SM-L200 back
{{5,(char[5]){0x1b,0x1e,'m','0',0x00}},                            // Disable
 {5,(char[5]){0x1b,0x1e,'m','1',0x02}},                            // Enable
 {5,(char[5]){0x1b,0x1e,'m','2',0x02}}};                           // Enable + Detection(SM-L200 back)

static const struct command blackMarkGapHoleDetectionCommand [3] =   // SM-T400i Gap/Hole
{{5,(char[5]){0x1b,0x1e,'m','0',0x00}},                               // Disable
 {5,(char[5]){0x1b,0x1e,'m','1',0x01}},                               // Enable
 {5,(char[5]){0x1b,0x1e,'m','2',0x01}}};                              // Enable + Detection(SM-T400i Gap/Hole)

static const struct command blackMarkThreeInchDetectionCommand [3] =   // SM-L300 3inch
{{9,(char[9]){0x1b,0x1e,'o',0x00,0x1b,0x1e,'m', '0', 0x02}},            // Disable
 {9,(char[9]){0x1b,0x1e,'o',0x00,0x1b,0x1e,'m', '1', 0x02}},            // Enable
 {9,(char[9]){0x1b,0x1e,'o',0x00,0x1b,0x1e,'m', '2', 0x02}}};           // Enable + Detection(SM-T400i Gap/Hole)


static const struct command blackMarkTwoInchDetectionCommand [3] =   // SM-L300 2inch
{{9,(char[9]){0x1b,0x1e,'o',0x01,0x1b,0x1e,'m', '0', 0x02}},            // Disable
 {9,(char[9]){0x1b,0x1e,'o',0x01,0x1b,0x1e,'m', '1', 0x02}},            // Enable
 {9,(char[9]){0x1b,0x1e,'o',0x01,0x1b,0x1e,'m', '2', 0x02}}};           // Enable + Detection(SM-T400i Gap/Hole)


static const struct command blackMarkCenterDetectionCommand [3] =   // SM-L300 Center
{{9,(char[9]){0x1b,0x1e,'o',0x02,0x1b,0x1e,'m', '0', 0x02}},            // Disable
 {9,(char[9]){0x1b,0x1e,'o',0x02,0x1b,0x1e,'m', '1', 0x02}},            // Enable
 {9,(char[9]){0x1b,0x1e,'o',0x02,0x1b,0x1e,'m', '2', 0x02}}};           // Enable + Detection(SM-T400i Gap/Hole)


static const struct command blackMarkLabelDetectionCommand [3] =   // SM-L300 Lable
{{9,(char[9]){0x1b,0x1e,'o',0x02,0x1b,0x1e,'m', '0', 0x01}},            // Disable
 {9,(char[9]){0x1b,0x1e,'o',0x02,0x1b,0x1e,'m', '1', 0x01}},            // Enable
 {9,(char[9]){0x1b,0x1e,'o',0x02,0x1b,0x1e,'m', '2', 0x01}}};           // Enable + Detection(SM-T400i Gap/Hole)


// define buzzer1 on time commands
// timing: jobSetup.1
// models: POP10,MCP20,MCP21,MCP30,MCP31
static const struct command buzzer1OnTimeCommand [8] =
  {{6,(char[6]){0x1b,0x1d,0x19,0x11,'1',0x01}},                   //  1 20 millis
   {6,(char[6]){0x1b,0x1d,0x19,0x11,'1',0x02}},                   //  2 40 millis
   {6,(char[6]){0x1b,0x1d,0x19,0x11,'1',0x05}},                   //  3 100 millis
   {6,(char[6]){0x1b,0x1d,0x19,0x11,'1',0x0A}},                   //  4 200 millis
   {6,(char[6]){0x1b,0x1d,0x19,0x11,'1',0x19}},                   //  5 500 millis
   {6,(char[6]){0x1b,0x1d,0x19,0x11,'1',0x32}},                   //  6 1000 millis
   {6,(char[6]){0x1b,0x1d,0x19,0x11,'1',0x64}},                   //  7 2000 millis
   {6,(char[6]){0x1b,0x1d,0x19,0x11,'1',(char)0xFA}}};            //  8 5000 millis

// define buzzer1 off time commands
// timing: jobSetup.1
// models: MCP20,MCP21,MCP30,MCP31
static const struct command buzzer1OffTimeCommand [8] =
  {{1,(char[1]){0x01}},                   //  1 20 millis
   {1,(char[1]){0x02}},                   //  2 40 millis
   {1,(char[1]){0x05}},                   //  3 100 millis
   {1,(char[1]){0x0A}},                   //  4 200 millis
   {1,(char[1]){0x19}},                   //  5 500 millis
   {1,(char[1]){0x32}},                   //  6 1000 millis
   {1,(char[1]){0x64}},                   //  7 2000 millis
   {1,(char[1]){(char)0xFA}}};            //  8 5000 millis

// define buzzer1 fire commands
// timing: jobSetup.1
// models: MCP20,MCP21,MCP30,MCP31
static const struct command buzzer1SettingCommand [7] =
{{7,(char[7]){0x1b,0x1d,0x19,0x12,'1',0x01,0x00}},                //  0 buzzer 1 fire (repeat = 1)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'1',0x02,0x00}},                //  1 buzzer 1 fire (repeat = 2)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'1',0x03,0x00}},                //  2 buzzer 1 fire (repeat = 3)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'1',0x05,0x00}},                //  3 buzzer 1 fire (repeat = 5)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'1',0x0A,0x00}},                //  4 buzzer 1 fire (repeat = 10)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'1',0x0F,0x00}},                //  5 buzzer 1 fire (repeat = 15)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'1',0x14,0x00}}};               //  6 buzzer 1 fire (repeat = 20)

// define buzzer2 on time commands
// timing: jobSetup.1
// models: MCP20,MCP21,MCP30,MCP31
static const struct command buzzer2OnTimeCommand [8] =
  {{6,(char[6]){0x1b,0x1d,0x19,0x11,'2',0x01}},                   //  1 20 millis
   {6,(char[6]){0x1b,0x1d,0x19,0x11,'2',0x02}},                   //  2 40 millis
   {6,(char[6]){0x1b,0x1d,0x19,0x11,'2',0x05}},                   //  3 100 millis
   {6,(char[6]){0x1b,0x1d,0x19,0x11,'2',0x0A}},                   //  4 200 millis
   {6,(char[6]){0x1b,0x1d,0x19,0x11,'2',0x19}},                   //  5 500 millis
   {6,(char[6]){0x1b,0x1d,0x19,0x11,'2',0x32}},                   //  6 1000 millis
   {6,(char[6]){0x1b,0x1d,0x19,0x11,'2',0x64}},                   //  7 2000 millis
   {6,(char[6]){0x1b,0x1d,0x19,0x11,'2',(char)0xFA}}};            //  8 5000 millis

// define buzzer2 off time commands
// timing: jobSetup.1
// models: MCP20,MCP21,MCP30,MCP31
static const struct command buzzer2OffTimeCommand [8] =
  {{1,(char[1]){0x01}},                   //  1 20 millis
   {1,(char[1]){0x02}},                   //  2 40 millis
   {1,(char[1]){0x05}},                   //  3 100 millis
   {1,(char[1]){0x0A}},                   //  4 200 millis
   {1,(char[1]){0x19}},                   //  5 500 millis
   {1,(char[1]){0x32}},                   //  6 1000 millis
   {1,(char[1]){0x64}},                   //  7 2000 millis
   {1,(char[1]){(char)0xFA}}};            //  8 5000 milli

// define buzzer2 fire commands
// timing: jobSetup.1
// models: MCP20,MCP21,MCP30,MCP31
static const struct command buzzer2SettingCommand [7] =
{{7,(char[7]){0x1b,0x1d,0x19,0x12,'2',0x01,0x00}},                //  0 buzzer 2 fire (repeat = 1)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'2',0x02,0x00}},                //  1 buzzer 2 fire (repeat = 2)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'2',0x03,0x00}},                //  2 buzzer 2 fire (repeat = 3)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'2',0x05,0x00}},                //  3 buzzer 2 fire (repeat = 5)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'2',0x0A,0x00}},                //  4 buzzer 2 fire (repeat = 10)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'2',0x0F,0x00}},                //  5 buzzer 2 fire (repeat = 15)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'2',0x14,0x00}}};               //  6 buzzer 2 fire (repeat = 20)


// define melodySpeaker setting commands
static const struct command melodySpeakerSettingCommand [19] =
{{14,(char[14]){0x1b,0x1d,0x19,0x11,0x02,0x09,0x01,0x1b,0x1d,0x19,0x12,0x02,0x01,0x00}}, //00_00
 {14,(char[14]){0x1b,0x1d,0x19,0x11,0x02,0x08,0x02,0x1b,0x1d,0x19,0x12,0x02,0x01,0x00}}, //01_01
 {14,(char[14]){0x1b,0x1d,0x19,0x11,0x02,0x07,0x03,0x1b,0x1d,0x19,0x12,0x02,0x01,0x00}}, //02_03
 {14,(char[14]){0x1b,0x1d,0x19,0x11,0x02,0x06,0x04,0x1b,0x1d,0x19,0x12,0x02,0x01,0x00}}, //03_07
 {14,(char[14]){0x1b,0x1d,0x19,0x11,0x02,0x05,0x05,0x1b,0x1d,0x19,0x12,0x02,0x01,0x00}}, //04_0F
 {14,(char[14]){0x1b,0x1d,0x19,0x11,0x02,0x04,0x01,0x1b,0x1d,0x19,0x12,0x02,0x02,0x00}}, //05_10
 {14,(char[14]){0x1b,0x1d,0x19,0x11,0x02,0x04,0x06,0x1b,0x1d,0x19,0x12,0x02,0x01,0x00}}, //06_1F
 {14,(char[14]){0x1b,0x1d,0x19,0x11,0x02,0x03,0x02,0x1b,0x1d,0x19,0x12,0x02,0x02,0x00}}, //07_31
 {14,(char[14]){0x1b,0x1d,0x19,0x11,0x02,0x03,0x03,0x1b,0x1d,0x19,0x12,0x02,0x02,0x00}}, //08_38
 {14,(char[14]){0x1b,0x1d,0x19,0x11,0x02,0x03,0x07,0x1b,0x1d,0x19,0x12,0x02,0x01,0x00}}, //09_3F
 {14,(char[14]){0x1b,0x1d,0x19,0x11,0x02,0x02,0x04,0x1b,0x1d,0x19,0x12,0x02,0x02,0x00}}, //0A_79
 {14,(char[14]){0x1b,0x1d,0x19,0x11,0x02,0x02,0x08,0x1b,0x1d,0x19,0x12,0x02,0x01,0x00}}, //0B_7F
 {14,(char[14]){0x1b,0x1d,0x19,0x11,0x02,0x01,0x01,0x1b,0x1d,0x19,0x12,0x02,0x05,0x00}}, //0C_AA
 {14,(char[14]){0x1b,0x1d,0x19,0x11,0x02,0x01,0x03,0x1b,0x1d,0x19,0x12,0x02,0x03,0x00}}, //0D_EE
 {14,(char[14]){0x1b,0x1d,0x19,0x11,0x02,0x01,0x04,0x1b,0x1d,0x19,0x12,0x02,0x02,0x00}}, //0E_F7
 {14,(char[14]){0x1b,0x1d,0x19,0x11,0x02,0x01,0x05,0x1b,0x1d,0x19,0x12,0x02,0x02,0x00}}, //0F_FB
 {14,(char[14]){0x1b,0x1d,0x19,0x11,0x02,0x01,0x06,0x1b,0x1d,0x19,0x12,0x02,0x02,0x00}}, //FD
 {14,(char[14]){0x1b,0x1d,0x19,0x11,0x02,0x01,0x07,0x1b,0x1d,0x19,0x12,0x02,0x02,0x00}}, //FE
 {14,(char[14]){0x1b,0x1d,0x19,0x11,0x02,0x01,0x09,0x1b,0x1d,0x19,0x12,0x02,0x01,0x00}}  //FF
};


#define PAGETYPE_RECEIPT             0
#define PAGETYPE_TICKET              1
#define PAGETYPE_BLACKMARK            2
#define PAGETYPE_BLACKMARK_FRONT      3
#define PAGETYPE_BLACKMARK_BACK       4
#define PAGETYPE_BLACKMARK_GAP_HOLE   5
#define PAGETYPE_BLACKMARK_THREE_INCH 6
#define PAGETYPE_BLACKMARK_TWO_INCH   7
#define PAGETYPE_BLACKMARK_CENTER     8
#define PAGETYPE_LABEL              9

// define page cut type commands
// timing: jobSetup.7
// models: 
static const struct command pageCutTypeCommand[5] =
{{2,(char[2]){0x0a,0x00}},                    // no cut, no action
 {3,(char[3]){0x1b,'d','t'}},                 // Tearbar  
 {2,(char[2]){0x0c,0x00}},                    // BlackMark
 {3,(char[3]){0x1b,'d','3'}},                 // Partial cut
 {3,(char[3]){0x1b,'d','2'}}};                // Full cut

// for POP10,MCP20,MCP21,MCP30,MCP31 (adjust thermal printer format)
static const struct command pageCutTypeCommand2[3] =
{{2,(char[2]){0x0a,0x00}},                    // no cut, no action
 {3,(char[3]){0x1b,'d','3'}},                 // Partial cut
 {3,(char[3]){0x1b,'d','2'}}};                // Full cut

// define doc cut type commands
// timing: jobSetup.8
// models: 
static const struct command docCutTypeCommand[5] =
{{2,(char[2]){0x0a,0x00}},                    // no cut, no action
 {3,(char[3]){0x1b,'d','t'}},                 // Tearbar
 {2,(char[2]){0x0c,0x00}},                    // BlackMark
 {3,(char[3]){0x1b,'d','3'}},                 // Partial cut
 {3,(char[3]){0x1b,'d','2'}}};                // Full cut

// for POP10,MCP20,MCP21,MCP30,MCP31 (adjust thermal printer format)
static const struct command docCutTypeCommand2[5] =
{{2,(char[2]){0x0a,0x00}},                    // no cut, no action
 {3,(char[3]){0x1b,'d','3'}},                 // Partial cut
 {3,(char[3]){0x1b,'d','2'}}};                // Full cut



// define start page command
// timing: pageSetup.0
// models: all
static const struct command startPageCommand =
{0,NULL};

// define end page command
// timing: endPage.0
// models: all
/* unused
static const struct command endPageCommand =
{3,(char[3]){0x0a,0x1b,'d'}};
 */

// define end job command
// timing: endJob.0
// models: all
/* unused
static const struct command endJobCommand =
{1,(char[1]){0xff}};
 */


void debugPrintSettings(struct settings_ * settings)
{
  fprintf(stderr, "DEBUG: pageType     = %d\n", settings->pageType);
  fprintf(stderr, "DEBUG: focusArea    = %d\n", settings->focusArea);
  fprintf(stderr, "DEBUG: printSpeed   = %d\n", settings->printSpeed);
  fprintf(stderr, "DEBUG: mediaType    = %d\n", settings->mediaType);
  fprintf(stderr, "DEBUG: printDensity = %d\n", settings->printDensity);
  fprintf(stderr, "DEBUG: pageCutType  = %d\n", settings->pageCutType);
  fprintf(stderr, "DEBUG: docCutType   = %d\n", settings->docCutType);
  fprintf(stderr, "DEBUG: cashDrawerSetting = %d\n"  , settings->cashDrawerSetting);
  fprintf(stderr, "DEBUG: cashDrawer1PulseWidth = %d\n", settings->cashDrawer1PulseWidth);
  fprintf(stderr, "DEBUG: topMargin = %d\n", settings->topMargin);
  fprintf(stderr, "DEBUG: blackMarkDetection   = %d\n", settings->blackMarkDetection);
  fprintf(stderr, "DEBUG: bytesPerScanLine = %d\n", settings->bytesPerScanLine);
  fprintf(stderr, "DEBUG: buzzer1Setting = %d\n", settings->buzzer1Setting);
  fprintf(stderr, "DEBUG: buzzer1OnTime = %d\n", settings->buzzer1OnTime);
  fprintf(stderr, "DEBUG: buzzer1OffTime = %d\n", settings->buzzer1OffTime);
  fprintf(stderr, "DEBUG: buzzer1Repeat = %d\n", settings->buzzer1Repeat);
  fprintf(stderr, "DEBUG: buzzer2Setting = %d\n", settings->buzzer2Setting);
  fprintf(stderr, "DEBUG: buzzer2OnTime = %d\n", settings->buzzer2OnTime);
  fprintf(stderr, "DEBUG: buzzer2OffTime = %d\n", settings->buzzer2OffTime);
  fprintf(stderr, "DEBUG: buzzer2Repeat = %d\n", settings->buzzer2Repeat);
  fprintf(stderr, "DEBUG: melodySpeakerSetting = %d\n", settings->melodySpeakerSetting);
  fprintf(stderr, "DEBUG: melodySpeakerSoundStorageArea = %d\n", settings->melodySpeakerSoundStorageArea);
  fprintf(stderr, "DEBUG: melodySpeakerSoundNumber = %d\n", settings->melodySpeakerSoundNumber);
  fprintf(stderr, "DEBUG: melodySpeakerSoundVolume = %d\n", settings->melodySpeakerSoundVolume);
  fprintf(stderr, "DEBUG: melodySpeakerRepeat = %d\n", settings->melodySpeakerRepeat);
  fprintf(stderr, "DEBUG: dataTreatmentRecoverFromError = %d\n", settings->dataTreatmentRecoverFromError);

}

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

struct command getPrintDensityCommandWithIndex(struct settings_ * settings, int index)
{
    switch (settings->modelNumber)
    {
        default:
            return printDensityCommand[index];

        case POP10:
        case MCP20:
        case MCP21:
        case MCP30:
        case MCP31:
            return printDensityCommand2[index];
    }
}

struct command getPrintDensityCommand(struct settings_ * settings)
{
    return getPrintDensityCommandWithIndex(settings, settings->printDensity);
}

struct command getPageCutTypeCommandWithIndex(struct settings_ * settings, int index)
{
    switch (settings->modelNumber)
    {
        default:
            return pageCutTypeCommand[index];

        case POP10:
        case MCP20:
        case MCP21:
        case MCP30:
        case MCP31:
            return pageCutTypeCommand2[index];
    }
}

struct command getPageCutTypeCommand(struct settings_ * settings)
{
    return getPageCutTypeCommandWithIndex(settings, settings->pageCutType);
}

struct command getDocCutTypeCommandWithIndex(struct settings_ * settings, int index)
{
    switch (settings->modelNumber)
    {
        default:
            return docCutTypeCommand[index];

        case POP10:
        case MCP20:
        case MCP21:
        case MCP30:
        case MCP31:
            return docCutTypeCommand2[index];
    }
}

struct command getDocCutTypeCommand(struct settings_ * settings)
{
    return getDocCutTypeCommandWithIndex(settings, settings->docCutType);
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

    settings->pageType                       = getOptionChoiceIndex("PageType"                      , ppd);
    settings->focusArea                      = getOptionChoiceIndex("FocusArea"                     , ppd);
    settings->printSpeed                     = getOptionChoiceIndex("PrintSpeed"                    , ppd);
    settings->mediaType                      = getOptionChoiceIndex("MediaType"                     , ppd);
    settings->printDensity                   = getOptionChoiceIndex("PrintDensity"                  , ppd);
    settings->dataTreatmentRecoverFromError  = getOptionChoiceIndex("DataTreatmentRecoverFromError" , ppd);
    settings->pageCutType                    = getOptionChoiceIndex("PageCutType"                   , ppd);
    settings->docCutType                     = getOptionChoiceIndex("DocCutType"                    , ppd);
    settings->blackMarkDetection             = getOptionChoiceIndex("BlackMarkDetection"            , ppd);
    settings->topMargin                      = getOptionChoiceIndex("TopMargin"                     , ppd);
    settings->cashDrawerSetting              = getOptionChoiceIndex("CashDrawerSetting"             , ppd);
    settings->cashDrawer1PulseWidth          = getOptionChoiceIndex("CashDrawer1PulseWidth"         , ppd);
    settings->buzzer1Setting                 = getOptionChoiceIndex("Buzzer1Setting"                , ppd);
    settings->buzzer1OnTime                  = getOptionChoiceIndex("Buzzer1OnTime"                 , ppd);
    settings->buzzer1OffTime                 = getOptionChoiceIndex("Buzzer1OffTime"                , ppd);
    settings->buzzer1Repeat                  = getOptionChoiceIndex("Buzzer1Repeat"                 , ppd);
    settings->buzzer2Setting                 = getOptionChoiceIndex("Buzzer2Setting"                , ppd);
    settings->buzzer2OnTime                  = getOptionChoiceIndex("Buzzer2OnTime"                 , ppd);
    settings->buzzer2OffTime                 = getOptionChoiceIndex("Buzzer2OffTime"                , ppd);
    settings->buzzer2Repeat                  = getOptionChoiceIndex("Buzzer2Repeat"                 , ppd);
    settings->melodySpeakerSetting           = getOptionChoiceIndex("MelodySpeakerSetting"          , ppd);
    settings->melodySpeakerSoundStorageArea  = getOptionChoiceIndex("MelodySpeakerSoundStorageArea", ppd);
    settings->melodySpeakerSoundNumber       = getOptionChoiceIndex("MelodySpeakerSoundNumber"      , ppd);
    settings->melodySpeakerSoundVolume       = getOptionChoiceIndex("MelodySpeakerSoundVolume"      , ppd);
    settings->melodySpeakerRepeat            = getOptionChoiceIndex("MelodySpeakerRepeat"           , ppd);

    switch (modelNumber)
    {
        default:
        case SMT300i:   settings->bytesPerScanLine    = SMT300i_MAX_WIDTH;
                        settings->bytesPerScanLineStd = SMT300i_STD_WIDTH;  break;
        case SMT400i:   settings->bytesPerScanLine    = SMT400i_MAX_WIDTH;
                        settings->bytesPerScanLineStd = SMT400i_STD_WIDTH;  break;
        case SMS210i:   settings->bytesPerScanLine    = SMS210i_MAX_WIDTH;
                        settings->bytesPerScanLineStd = SMS210i_STD_WIDTH;  break;
        case SMS220i:   settings->bytesPerScanLine    = SMS220i_MAX_WIDTH;
                        settings->bytesPerScanLineStd = SMS220i_STD_WIDTH;  break;
        case SMS230i:   settings->bytesPerScanLine    = SMS230i_MAX_WIDTH;
                        settings->bytesPerScanLineStd = SMS230i_STD_WIDTH;  break;
        case SML200:
        case SML204:
                        settings->bytesPerScanLine    = SML200_MAX_WIDTH;
                        settings->bytesPerScanLineStd = SML200_STD_WIDTH;   break;
        case SML300:
        case SML304:
                        settings->bytesPerScanLine    = SML300_MAX_WIDTH;
                        settings->bytesPerScanLineStd = SML300_STD_WIDTH;   break;
        case POP10:     settings->bytesPerScanLine    = POP10_MAX_WIDTH;
                        settings->bytesPerScanLineStd = POP10_STD_WIDTH;    break;
        case MCP20:
        case MCP21:     settings->bytesPerScanLine    = MCP20_MAX_WIDTH;
                        settings->bytesPerScanLineStd = MCP20_STD_WIDTH;    break;
        case MCP30:
        case MCP31:     settings->bytesPerScanLine    = MCP30_MAX_WIDTH;
                        settings->bytesPerScanLineStd = MCP30_STD_WIDTH;    break;
    }

    getPageWidthPageHeight(ppd, settings);

    PPDCLOSE(ppd);

    debugPrintSettings(settings);
}

void jobSetup(struct settings_ settings, char *argv[])
{
    outputCommand(printerInitializeCommand);

    if (settings.dataTreatmentRecoverFromError == DATACANCEL_DOC )
    {
        outputCommand(dataTreatmentRecoverFromErrorCommand[1]);
    }

    if (settings.modelNumber == SML300 || settings.modelNumber == SML304)
    {
        if      (settings.pageWidth == 72.0f) outputCommand(printableWidthCommand[0]);
        else if (settings.pageWidth == 50.8f) outputCommand(printableWidthCommand[3]);
        else outputCommand(printableWidthCommand[0]);
    }

    if (settings.modelNumber == POP10 || settings.modelNumber == MCP20 || settings.modelNumber == MCP21)
    {
        if      (settings.pageWidth == 48.0f) outputCommand(printableWidthCommand[0]);
        else if (settings.pageWidth == 54.0f) outputCommand(printableWidthCommand[1]);
        else if (settings.pageWidth == 50.8f) outputCommand(printableWidthCommand[3]);
        else outputCommand(printableWidthCommand[0]);
    }

    if (settings.modelNumber == MCP30 || settings.modelNumber == MCP31)
    {
        if      (settings.pageWidth == 72.0f) outputCommand(printableWidthCommand[0]);
        else if (settings.pageWidth == 48.0f) outputCommand(printableWidthCommand[2]);
        else if (settings.pageWidth == 50.8f) outputCommand(printableWidthCommand[3]);
        else outputCommand(printableWidthCommand[0]);
    }

    outputCommand(getPrintDensityCommand(&settings));

    if (settings.modelNumber == SMS210i || settings.modelNumber == SMS220i || settings.modelNumber == SMS230i || settings.modelNumber == SMT300i || settings.modelNumber == SMT400i ||
        settings.modelNumber == POP10   || settings.modelNumber == MCP30   || settings.modelNumber == MCP31 )
    {
        outputCommand(printSpeedCommand[settings.printSpeed]);
    }

    if (settings.modelNumber == MCP31)
    {
        outputCommand(topMarginCommand[settings.topMargin]); 
    }

    
    if(settings.pageType == PAGETYPE_BLACKMARK)                                         //Black Mark
    {
        outputCommand(blackMarkDetectionCommand[settings.blackMarkDetection]);          //ESC RS m1 or ESC RS m2
    }
    else if(settings.pageType == PAGETYPE_BLACKMARK_FRONT)                              //Black Mark(Front) for SM-L200
    {
        outputCommand(blackMarkFrontDetectionCommand[settings.blackMarkDetection]);     //ESC RS m10 or ESC RS m20
    }
    else if(settings.pageType == PAGETYPE_BLACKMARK_BACK)                               //Black Mark(Back) for SM-L200
    {
        outputCommand(blackMarkBackDetectionCommand[settings.blackMarkDetection]);      //ESC RS m11 or ESC RS m21
    }
    else if(settings.pageType == PAGETYPE_BLACKMARK_GAP_HOLE)                           //Black Mark(Gap/Hole) for SM-T400i
    {
        outputCommand(blackMarkGapHoleDetectionCommand[settings.blackMarkDetection]);   //ESC RS m12 or ESC RS m22
    }
    else if(settings.pageType == PAGETYPE_BLACKMARK_THREE_INCH)                         //Black Mark(3inch) for SM-L300
    {
        outputCommand(blackMarkThreeInchDetectionCommand[settings.blackMarkDetection]);
    }
    else if(settings.pageType == PAGETYPE_BLACKMARK_TWO_INCH)                           //Black Mark(2inch) for SM-L300
    {
        outputCommand(blackMarkTwoInchDetectionCommand[settings.blackMarkDetection]);
    }
    else if(settings.pageType == PAGETYPE_BLACKMARK_CENTER)                             //Black Mark(center) for SM-L300
    {
        outputCommand(blackMarkCenterDetectionCommand[settings.blackMarkDetection]);
    }
    else if(settings.pageType == PAGETYPE_LABEL)                                        //Label for SM-L300
    {
        outputCommand(blackMarkLabelDetectionCommand[settings.blackMarkDetection]);
    }
    else
    {
         outputCommand(blackMarkDetectionCommand[0]);                                   //ESC RS m0
    }

    if ( settings.modelNumber == MCP30 || settings.modelNumber == MCP31 || settings.modelNumber == MCP20 || settings.modelNumber == MCP21)
    {
        if (settings.buzzer1Setting == BUZZER_DOC_TOP)
        {
            outputCommand(buzzer1OnTimeCommand[settings.buzzer1OnTime]);
            outputCommand(buzzer1OffTimeCommand[settings.buzzer1OffTime]);
            outputCommand(buzzer1SettingCommand[settings.buzzer1Repeat]);
        }

        if (settings.buzzer2Setting == BUZZER_DOC_TOP)
        {
            outputCommand(buzzer2OnTimeCommand[settings.buzzer2OnTime]);
            outputCommand(buzzer2OffTimeCommand[settings.buzzer2OffTime]);
            outputCommand(buzzer2SettingCommand[settings.buzzer2Repeat]);
        }
    }

    if (settings.modelNumber == POP10 || settings.modelNumber == MCP30 || settings.modelNumber == MCP31 || settings.modelNumber == MCP20 || settings.modelNumber == MCP21)
    {
        outputCommand(cashDrawer1PulseWidthCommand[settings.cashDrawer1PulseWidth]);

        outputCommand(cashDrawerSettingCommand[settings.cashDrawerSetting]);
    }
    
    if ( settings.modelNumber == MCP30 || settings.modelNumber == MCP31)
    {
        if(settings.melodySpeakerSetting == MELODYSPEAKER_DOC_TOP)
        {
            int i=0;
            for(i=0; i<settings.melodySpeakerRepeat + 1; i++)
            {
                outputCommand(melodySpeakerSettingCommand[16]); //FD
                
                if (settings.melodySpeakerSoundStorageArea == MELODYSPEAKER_AREA_1)
                {
                    outputCommand(melodySpeakerSettingCommand[4]); //0F
                }
                else // MELODYSPEAKER_AREA_2
                {
                    outputCommand(melodySpeakerSettingCommand[5]); //10
                }
                
                outputCommand(melodySpeakerSettingCommand[settings.melodySpeakerSoundNumber]);
                outputCommand(melodySpeakerSettingCommand[settings.melodySpeakerSoundVolume]);
            }
        }
    }

}

void pageSetup(struct settings_ settings, cups_page_header2_t header, int page)
{
    outputCommand(startPageCommand);

    if (page >= 1)
    {
        if (settings.pageType == PAGETYPE_BLACKMARK)
        {
            if (settings.pageCutType == 0)  // No Cut
            {
                outputCommand(getPageCutTypeCommandWithIndex(&settings, PAGETYPE_BLACKMARK));
            }
            else
            {
                outputCommand(getPageCutTypeCommand(&settings));
            }
        }
        else
        {
            outputCommand(getPageCutTypeCommand(&settings));
        }
    }

}

void endPage(struct settings_ settings)
{

}

void endJob(struct settings_ settings)
{
    if((settings.pageType == PAGETYPE_BLACKMARK)            ||
       (settings.pageType == PAGETYPE_BLACKMARK_FRONT)      ||
       (settings.pageType == PAGETYPE_BLACKMARK_BACK)       ||
       (settings.pageType == PAGETYPE_BLACKMARK_GAP_HOLE)   ||
       (settings.pageType == PAGETYPE_BLACKMARK_THREE_INCH) ||
       (settings.pageType == PAGETYPE_BLACKMARK_TWO_INCH)   ||
       (settings.pageType == PAGETYPE_BLACKMARK_CENTER)     ||
       (settings.pageType == PAGETYPE_LABEL))
    {
        if(settings.docCutType == 0)//No Cut
        {
            outputCommand(getDocCutTypeCommandWithIndex(&settings, PAGETYPE_BLACKMARK));
        }
        else
        {
            outputCommand(getDocCutTypeCommand(&settings));
        }
    }
    else
    {
        outputCommand(getDocCutTypeCommand(&settings));
    }

    if ( settings.modelNumber == MCP30 || settings.modelNumber == MCP31 || settings.modelNumber == MCP20 || settings.modelNumber == MCP21)
    {
        if (settings.buzzer1Setting == BUZZER_DOC_BTM)
        {
            outputCommand(buzzer1OnTimeCommand[settings.buzzer1OnTime]);
            outputCommand(buzzer1OffTimeCommand[settings.buzzer1OffTime]);
            outputCommand(buzzer1SettingCommand[settings.buzzer1Repeat]);
        }

        if (settings.buzzer2Setting == BUZZER_DOC_BTM)
        {
            outputCommand(buzzer2OnTimeCommand[settings.buzzer2OnTime]);
            outputCommand(buzzer2OffTimeCommand[settings.buzzer2OffTime]);
            outputCommand(buzzer2SettingCommand[settings.buzzer2Repeat]);
        }
    }
    
    if ( settings.modelNumber == MCP30 || settings.modelNumber == MCP31)
    {
        if(settings.melodySpeakerSetting == MELODYSPEAKER_DOC_BTM)
        {
            int i=0;
            for(i=0; i<settings.melodySpeakerRepeat + 1; i++)
            {
                outputCommand(melodySpeakerSettingCommand[16]); //FD
                
                if (settings.melodySpeakerSoundStorageArea == MELODYSPEAKER_AREA_1)
                {
                    outputCommand(melodySpeakerSettingCommand[4]); //0F
                }
                else // MELODYSPEAKER_AREA_2
                {
                    outputCommand(melodySpeakerSettingCommand[5]); //10
                }
                
                outputCommand(melodySpeakerSettingCommand[settings.melodySpeakerSoundNumber]);
                outputCommand(melodySpeakerSettingCommand[settings.melodySpeakerSoundVolume]);
            }
        }
    }

    if (settings.dataTreatmentRecoverFromError == DATACANCEL_DOC )
    {
        outputCommand(dataTreatmentRecoverFromErrorCommand[2]);
    }

}

void jobSetupForPromotion(struct settings_ settings, char *argv[])
{
    outputCommand(printerInitializeCommand);
    
    if (settings.dataTreatmentRecoverFromError == DATACANCEL_DOC )
    {
        outputCommand(dataTreatmentRecoverFromErrorCommand[1]);
    }
}

void endJobForPromotion(struct settings_ settings)
{
    if((settings.pageType == PAGETYPE_BLACKMARK)            ||
       (settings.pageType == PAGETYPE_BLACKMARK_FRONT)      ||
       (settings.pageType == PAGETYPE_BLACKMARK_BACK)       ||
       (settings.pageType == PAGETYPE_BLACKMARK_GAP_HOLE)   ||
       (settings.pageType == PAGETYPE_BLACKMARK_THREE_INCH) ||
       (settings.pageType == PAGETYPE_BLACKMARK_TWO_INCH)   ||
       (settings.pageType == PAGETYPE_BLACKMARK_CENTER)     ||
       (settings.pageType == PAGETYPE_LABEL))
    {
        if(settings.docCutType == 0)//No Cut
        {
            outputCommand(getDocCutTypeCommandWithIndex(&settings, PAGETYPE_BLACKMARK));
        }
        else
        {
            outputCommand(getDocCutTypeCommand(&settings));
        }
    }
    else
    {
        outputCommand(getDocCutTypeCommand(&settings));
    }
    
    if (settings.dataTreatmentRecoverFromError == DATACANCEL_DOC )
    {
        outputCommand(dataTreatmentRecoverFromErrorCommand[2]);
    }
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
#define CLEANUP                                                         \
{                                                                       \
    if (originalRasterDataPtr   != NULL) free(originalRasterDataPtr);   \
    CUPSRASTERCLOSE(ras);                                               \
    if (fd != 0)                                                        \
    {                                                                   \
        close(fd);                                                      \
    }                                                                   \
    dlclose(libCupsImage);                                              \
    dlclose(libCups);                                                   \
}
#else
#define CLEANUP                                                         \
{                                                                       \
    if (originalRasterDataPtr   != NULL) free(originalRasterDataPtr);   \
    if (rawRasterPixels != NULL)         free(rawRasterPixels);         \
    CUPSRASTERCLOSE(ras);                                               \
    if (fd != 0)                                                        \
    {                                                                   \
        close(fd);                                                      \
    }                                                                   \
}
#endif

int main(int argc, char *argv[])
{
    int                 fd                      = 0;        /* File descriptor providing CUPS raster data                                           */
    cups_raster_t *     ras                     = NULL;     /* Raster stream for printing                                                           */
    cups_page_header2_t header;                             /* CUPS Page header                                                                     */
    int                 page                    = 0;        /* Current page                                                                         */

    int                 y                       = 0;        /* Vertical position in page 0 <= y <= header.cupsHeight                                */
    int                 i                       = 0;

    unsigned char *     rasterData              = NULL;     /* Pointer to raster data buffer                                                        */
    unsigned char *     originalRasterDataPtr   = NULL;     /* Copy of original pointer for freeing buffer                                          */
    unsigned char *     rawRasterPixels         = NULL;     /* [AllReceipts] CUPS Raster Data                                                       */
    int                 leftByteDiff            = 0;        /* Bytes on left to discard                                                             */
    int                 scanLineBlank           = 0;        /* Set to TRUE is the entire scan line is blank (no black pixels)                       */
    int                 lastBlackPixel          = 0;        /* Position of the last byte containing one or more black pixels in the scan line       */
    int                 numBlankScanLines       = 0;        /* Number of scanlines that were entirely black                                         */
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

    int isMicroReceipt = 0;

#ifdef STAR_CLOUD_SERVICES
    unsigned int cupsBytesPerLine = 0;
    unsigned int pxBufferedReceiptHeight = 0;

    isMicroReceipt = isMicroReceiptEnable();
#endif

    while (CUPSRASTERREADHEADER(ras, &header))
    {
        if ((header.cupsHeight == 0) || (header.cupsBytesPerLine == 0))
        {
            break;
        }

        if (rasterData == NULL)
        {
            rasterData = malloc(header.cupsBytesPerLine);
            if (rasterData == NULL)
            {
                CLEANUP;
                return EXIT_FAILURE;

            }
            originalRasterDataPtr = rasterData;  // used to later free the memory
        }

        pageSetup(settings, header, page);

        page++;

        numBlankScanLines = 0;

        if (header.cupsBytesPerLine <= settings.bytesPerScanLine)
        {
            settings.bytesPerScanLine = (int)header.cupsBytesPerLine;
            leftByteDiff = 0;
        }
        else
        {
            settings.bytesPerScanLine = settings.bytesPerScanLineStd;

            switch (settings.focusArea)
            {
                default:
                case FOCUS_LEFT:
                        leftByteDiff = 0;
                    break;
                case FOCUS_CENTER:
                        leftByteDiff = (((int)header.cupsBytesPerLine - settings.bytesPerScanLine) / 2);
                    break;
                case FOCUS_RIGHT:
                        leftByteDiff = ((int)header.cupsBytesPerLine - settings.bytesPerScanLine);
                    break;
            }
        }

#ifdef STAR_CLOUD_SERVICES
        cupsBytesPerLine = header.cupsBytesPerLine;

        setPaperWidth(header.cupsBytesPerLine * 8);

        setEmulation(StarPRNTMode);

        if (rawRasterPixels != NULL)
        {
            free(rawRasterPixels);
            rawRasterPixels = NULL;
        }

         rawRasterPixels = (uint8_t *) calloc(header.cupsHeight * header.cupsBytesPerLine, sizeof(uint8_t));
         if (rawRasterPixels == NULL)
         {
             CLEANUP;
             return EXIT_FAILURE;
         }
         int rawRasterPixelSize = 0;
#endif

        for (y = 0; y < header.cupsHeight; y ++)
        {
            if ((y & 127) == 0)
            {
                fprintf(stderr, "INFO: Printing page %d, %d%% complete...\n", page, (100 * (unsigned int)y / header.cupsHeight));
            }

            if (CUPSRASTERREADPIXELS(ras, rasterData, header.cupsBytesPerLine) < 1)
            {
                break;
            }
            
#ifdef STAR_CLOUD_SERVICES
            // Get CUPS Raster Data (1 line)
            memcpy(rawRasterPixels + rawRasterPixelSize, rasterData, header.cupsBytesPerLine);
            rawRasterPixelSize += header.cupsBytesPerLine;
#endif

            rasterData += leftByteDiff;

            for (i = settings.bytesPerScanLine - 1; i >= 0; i--)
            {
                if (((char) *(rasterData + i)) != ((char) 0x00))
                {
                    break;
                }
            }

            if (i == -1)
            {
                scanLineBlank = TRUE;
                numBlankScanLines++;
            }
            else
            {
                lastBlackPixel = i + 1;
                scanLineBlank = FALSE;
            }

            if (scanLineBlank == FALSE)
            {
                if (isMicroReceipt)
                {
                    numBlankScanLines = 0;
                }
                else
                {
                    int count = numBlankScanLines / 255;
                    while (count > 0)
                    {
                        putchar(0x1b);
                        putchar('I');
                        putchar(0xFF);
                        
                        count--;
                    }
                    if (numBlankScanLines > 0)
                    {
                        putchar(0x1b);
                        putchar('I');
                        putchar(numBlankScanLines % 255);
                        numBlankScanLines = 0;
                    }

                    putchar(0x1b); //ESC
                    putchar(0x1d); //GS
                    putchar('S');  //S
                    putchar(0x01); //m
                    putchar((char) ((lastBlackPixel > 0)?(lastBlackPixel % 256):1)); //xL
                    putchar(0x00); //xH
                    putchar(0x01); //yL
                    putchar(0x00); //yH
                    putchar(0x00); //n1
                    for (i = 0; i < lastBlackPixel; i++)
                    {
                        putchar((char) *rasterData);
                        rasterData++;
                    }
                }
            }
            rasterData = originalRasterDataPtr;
        }

        // Start for Ticket
        if (settings.pageType == PAGETYPE_TICKET)
        {
            if (isMicroReceipt)
            {
                numBlankScanLines = 0;
            }
            else
            {
                int count = numBlankScanLines / 255;
                while (count > 0)
                {
                    putchar(0x1b);
                    putchar('I');
                    putchar(0xFF);

                    count--;
                }
                if (numBlankScanLines > 0)
                {
                     putchar(0x1b);
                     putchar('I');
                     putchar(numBlankScanLines % 255);
                }
            }
        }
        //End for Ticket
         
#ifdef STAR_CLOUD_SERVICES
        int pxSkipBytes = (settings.pageType == PAGETYPE_TICKET) ? 0 : (header.cupsBytesPerLine * numBlankScanLines);
        appendBytes(rawRasterPixels, rawRasterPixelSize - pxSkipBytes);

        pxBufferedReceiptHeight += (settings.pageType == PAGETYPE_TICKET) ? header.cupsHeight : header.cupsHeight - numBlankScanLines;

        if (settings.pageCutType != 0)
        {
            int result = doAllReceiptsTasks(cupsBytesPerLine * 8, pxBufferedReceiptHeight);

            pxBufferedReceiptHeight = 0;
        }
#endif

        endPage(settings);
    }

#ifdef STAR_CLOUD_SERVICES
    if (settings.pageCutType == 0)
    {
        int result = doAllReceiptsTasks(cupsBytesPerLine * 8, pxBufferedReceiptHeight);
    }
#endif

    endJob(settings);
    
#ifdef STAR_CLOUD_SERVICES
    
    char **ImageFileList;
    
    int count = getPromotionImageNum();
    
    if(count > 0)
    {
        ImageFileList = (char**)malloc(sizeof(char*) * count);
        
        for(int i=0; i<count; i++)
        {
            ImageFileList[i] = (char*)malloc(sizeof(char*) * 256);
        }
        
        getPromotionImagesName(ImageFileList);
        
        for(int i=0; i<count; i++)
        {
            //fprintf(stderr, "INFO: filter Image  = %s\n", ImageFileList[i]);
            //sleep(1);
            
            jobSetupForPromotion(settings,argv); //データキャンセルだけ必要
            doPromoPrntTasks(ImageFileList[i]);
            endJobForPromotion(settings); //外部機器を余計に動作させないため
        }
        
        for(int i=0; i<count; i++)
        {
            //free(ImageFileList[i]);
        }
        free(ImageFileList);
    }
    
#endif

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

// end of rastertostar.c
