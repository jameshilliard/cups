/*
 * Star Micronics
 *
 * CUPS Filter
 *
 * [ Linux ]
 * compile cmd: gcc -Wl,-rpath,/usr/lib -Wall -fPIC -O2 -o rastertostar rastertostar.c -lcupsimage -lcups
 * compile requires cups-devel-1.1.19-13.i386.rpm (version not neccessarily important?)
 * find cups-devel location here: http://rpmfind.net/linux/rpm2html/search.php?query=cups-devel&submit=Search+...&system=&arch=
 *
 * [ Mac OS X ]
 * compile cmd: gcc -Wall -fPIC -o rastertostar rastertostar.c -lcupsimage -lcups -arch i386 -arch x86_64
 */

/*
 * Copyright (C) 2004-2019 Star Micronics Co., Ltd.
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
#include <cups/backend.h>
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
#define HSP7000R        70001
#define TSP1000         1000
#define TUP992          992
#define TUP942          942
#define TSP800          800
#define TSP800II        8002
#define TSP828L         828
#define TSP700II        7002
#define TSP700          700
#define TSP654          654
#define TSP651          651
#define TSP643          643
#define TSP613          613
#define TUP542          542
#define TUP592          592
#define FVP10           10
#define TSP143          143   
#define TSP113          113   


// definitions for printable width
#define HSP7000R_MAX_WIDTH 72
#define HSP7000R_STD_WIDTH 72

#define TSP1000_MAX_WIDTH 80
#define TSP1000_STD_WIDTH 80

#define TUP900_MAX_WIDTH 104
#define TUP900_STD_WIDTH 104

#define TSP800_MAX_WIDTH 104
#define TSP800_STD_WIDTH 104

#define TSP800II_MAX_WIDTH 104
#define TSP800II_STD_WIDTH 104

#define TSP828L_MAX_WIDTH 104
#define TSP828L_STD_WIDTH 104

#define TSP700_MAX_WIDTH 80
#define TSP700_STD_WIDTH 72

#define TSP650_MAX_WIDTH 80
#define TSP650_STD_WIDTH 72

#define TSP600_MAX_WIDTH 72
#define TSP600_STD_WIDTH 72

#define TUP500_MAX_WIDTH 82
#define TUP500_STD_WIDTH 82

#define FVP10_MAX_WIDTH 72
#define FVP10_STD_WIDTH 72

#define TSP100_MAX_WIDTH 72
#define TSP100_STD_WIDTH 72

#define FOCUS_LEFT      0
#define FOCUS_CENTER    1
#define FOCUS_RIGHT     2

#define BUZZER_NO_USE   0
#define BUZZER_DOC_TOP  1
#define BUZZER_DOC_BTM  2

#define DATACANCEL_NO_USE 0
#define DATACANCEL_DOC    1

struct settings_
{
    int modelNumber;

    float pageWidth;
    float pageHeight;

    int pageType;
    int focusArea;

    int printSpeed;

    int topSearch;
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

    int labelDetect;    // TSP828L 
    int mediaType;  // TSP828L 0:Normal, 1:Label(Black mark)
    int printDensity;   // TSP828L 0:-3 -> 6:+3

    int snoutControl;     // TUP500
    int snout1Interval;   // TUP500
    int snout2Interval;   // TUP500

    int documentTopSound;    // FVP10
    int documentBottomSound; // FVP10

    int dataTreatmentRecoverFromError;

    int cashDrawerSetting;
    int cashDrawer1PulseWidth;

    int peripheralSetting;
    int peripheralActivationPulseWidth;

    int presenterAction;
    int presenterTimeout;

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
 static const struct command nsbRequestSettingCommand[2] =
   {{4,(char[4]){0x1b,0x1e,0x61,48}},  // Ignore  NSB and ASB req
    {4,(char[4]){0x1b,0x1e,0x61,50}}}; // Receive NSB and ASB req

// define station setting command
// timing: jobSetup.0.2
// models: HSP7000
static const struct command stationSettingCommand =
  {4, (char[4]){0x1b, '+', 'A', '0'}};

// define printable width setting command
// timing: jobSetup.0.5
// models: all
static const struct command printableWidthCommand [6] =         // TSP1000 / TSP100 / TSP700II / TSP650 / FVP10
{{4,(char[4]){0x1b,0x1e,'A',0x00}},                             // 80mm    / 72mm   / 72mm     / 72mm   / 72mm
 {4,(char[4]){0x1b,0x1e,'A',0x01}},                             // 72mm    / 51mm   / 52.5mm   / -      / 52.5mm
 {4,(char[4]){0x1b,0x1e,'A',0x02}},                             // 55mm    / -      / 80mm     / -      / -
 {4,(char[4]){0x1b,0x1e,'A',0x03}},                             // 52mm    / -      / 50.8mm   / 50.8mm / 50.8mm
 {4,(char[4]){0x1b,0x1e,'A',0x04}},                             // 47mm    / -      / 52mm     / -      / 52mm
 {4,(char[4]){0x1b,0x1e,'A',0x05}}};                            // 42mm    / -      / -        / -      / -

// define printable width setting command for TUP500             
static const struct command printableWidthCommandForTUP500 [4] = // TUP500
  {{4,(char[4]){0x1b,0x1e,'A',0x50}},                            // 80mm
   {4,(char[4]){0x1b,0x1e,'A',0x48}},                            // 72mm
   {4,(char[4]){0x1b,0x1e,'A',0x44}},                            // 68mm
   {4,(char[4]){0x1b,0x1e,'A',0x33}}};                           // 51mm

// define print media command
// timing: jobSetup.0.6
// models: TSP828L
static const struct command mediaTypeCommand [2] =          // TSP828L
{{4,(char[4]){0x1b,0x1e,'m',0x00}},                             // Normal
 {4,(char[4]){0x1b,0x1e,'m',0x01}}};                            // Label


// define label detect at power on command
// timing: jobSetup.0.7
// models: TSP828L
static const struct command labelDetectCommand [2] =            // TSP828L
{{1,(char[1]){0x00,            }},                              // Disable
 {4,(char[4]){0x1b,0x1e,'m',0x02}}};                            // Enable


// define print density
// timing: jobSetup.0.8
// models: TSP828L
static const struct command printDensityCommand [7] =           // TSP828L
{{4,(char[4]){0x1b,0x1e,'d','6' }},                             // -3
 {4,(char[4]){0x1b,0x1e,'d','5' }},                             // -2
 {4,(char[4]){0x1b,0x1e,'d','4' }},                             // -1
 {4,(char[4]){0x1b,0x1e,'d','3' }},                             // standard
 {4,(char[4]){0x1b,0x1e,'d','2' }},                             // +1
 {4,(char[4]){0x1b,0x1e,'d','1' }},                             // +2
 {4,(char[4]){0x1b,0x1e,'d','0' }}};                            // +3


// define cash drawer 1 pulse width commands
// timing: jobSetup.1
// models: TSP100, TSP600, TSP700, TSP800, TSP800II, FVP10
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

// define peripheral activation pulse width commands
// timing: jobSetup.1
// models: TSP1000
static const struct command peripheralActivationPulseWidthCommand [13] =
{{6,(char[6]){0x1b,0x1d,0x07,0x01,0x01,0x01}},                    //  0 20millis
 {6,(char[6]){0x1b,0x1d,0x07,0x01,0x05,0x01}},                    //  1 100millis
 {6,(char[6]){0x1b,0x1d,0x07,0x01,0x0a,0x01}},                    //  2 200millis
 {6,(char[6]){0x1b,0x1d,0x07,0x01,0x0f,0x01}},                    //  3 300millis
 {6,(char[6]){0x1b,0x1d,0x07,0x01,0x14,0x01}},                    //  4 400millis
 {6,(char[6]){0x1b,0x1d,0x07,0x01,0x19,0x01}},                    //  5 500millis
 {6,(char[6]){0x1b,0x1d,0x07,0x01,0x1e,0x01}},                    //  6 600millis
 {6,(char[6]){0x1b,0x1d,0x07,0x01,0x23,0x01}},                    //  7 700millis
 {6,(char[6]){0x1b,0x1d,0x07,0x01,0x28,0x01}},                    //  8 800millis
 {6,(char[6]){0x1b,0x1d,0x07,0x01,0x2d,0x01}},                    //  9 900millis
 {6,(char[6]){0x1b,0x1d,0x07,0x01,0x32,0x01}},                    // 10 1000millis
 {6,(char[6]){0x1b,0x1d,0x07,0x01,0x37,0x01}},                    // 11 1100millis
 {6,(char[6]){0x1b,0x1d,0x07,0x01,0x3c,0x01}}};                   // 12 1200millis

// define presenter action commands
// timing: jobSetup.1
// models: TUP900, TUP500
static const struct command presenterActionCommand [13] =
{{4,(char[4]){0x1b,0x16,'2',0x00}},                              //  0 loop - hold - retract
 {4,(char[4]){0x1b,0x16,'2',0x01}},                              //  1 loop - hold - eject
 {4,(char[4]){0x1b,0x16,'2',0x02}},                              //  2 no loop - hold - retract
 {4,(char[4]){0x1b,0x16,'2',0x03}},                              //  3 no loop - hold - eject
 {4,(char[4]){0x1b,0x16,'2',0x04}}};                             //  4 no loop - no hold - eject

// define presenter timeout commands
// timing: jobSetup.1
// models: TUP900, TUP500
static const struct command presenterTimeoutCommand [13] =
{{4,(char[4]){0x1b,0x16,'1',(char) 0}},                           //  0 do not timeout
 {4,(char[4]){0x1b,0x16,'1',(char) 20}},                          //  1 10 seconds
 {4,(char[4]){0x1b,0x16,'1',(char) 40}},                          //  2 20 seconds
 {4,(char[4]){0x1b,0x16,'1',(char) 60}},                          //  3 30 seconds
 {4,(char[4]){0x1b,0x16,'1',(char) 80}},                          //  4 40 seconds
 {4,(char[4]){0x1b,0x16,'1',(char) 100}},                         //  5 50 seconds
 {4,(char[4]){0x1b,0x16,'1',(char) 120}},                         //  6 60 seconds
 {4,(char[4]){0x1b,0x16,'1',(char) 140}},                         //  7 70 seconds
 {4,(char[4]){0x1b,0x16,'1',(char) 160}},                         //  8 80 seconds
 {4,(char[4]){0x1b,0x16,'1',(char) 180}},                         //  9 90 seconds
 {4,(char[4]){0x1b,0x16,'1',(char) 200}},                         // 10 100 seconds
 {4,(char[4]){0x1b,0x16,'1',(char) 220}},                         // 11 110 seconds
 {4,(char[4]){0x1b,0x16,'1',(char) 240}}};                        // 12 120 seconds

// define buzzer1 on time commands
// timing: jobSetup.1
// models: TSP650, TSP700II, HSP7000_Receipt, FVP10
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
// models: TSP650, TSP700II, HSP7000_Receipt, FVP10
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
// models: TSP650, TSP700II, HSP7000_Receipt, FVP10
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
// models: TSP650, TSP700II, HSP7000_Receipt, FVP10
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
// models: TSP650, TSP700II, HSP7000_Receipt, FVP10
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
// models: TSP650, TSP700II, HSP7000_Receipt, FVP10
static const struct command buzzer2SettingCommand [7] =
{{7,(char[7]){0x1b,0x1d,0x19,0x12,'2',0x01,0x00}},                //  0 buzzer 2 fire (repeat = 1)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'2',0x02,0x00}},                //  1 buzzer 2 fire (repeat = 2)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'2',0x03,0x00}},                //  2 buzzer 2 fire (repeat = 3)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'2',0x05,0x00}},                //  3 buzzer 2 fire (repeat = 5)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'2',0x0A,0x00}},                //  4 buzzer 2 fire (repeat = 10)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'2',0x0F,0x00}},                //  5 buzzer 2 fire (repeat = 15)
 {7,(char[7]){0x1b,0x1d,0x19,0x12,'2',0x14,0x00}}};               //  6 buzzer 2 fire (repeat = 20)

// define snout control command
// timing: jobSetup.1.5
// models: TUP592
static const struct command snoutControlCommand[4] =
{{7,(char[7]){0x1b,0x1d,0x1a,0x11,'0',0x00,0x00}},
 {7,(char[7]){0x1b,0x1d,0x1a,0x11,'1',0x00,0x00}},
 {7,(char[7]){0x1b,0x1d,0x1a,0x11,'2',0x00,0x00}},
 {7,(char[7]){0x1b,0x1d,0x1a,0x11,'3',0x00,0x00}}};

// define snout1(Green LED) interval setting command
// timing: jobSetup.1.5
// models: TUP592
static const struct command snout1IntervalCommand[4] =
{{7,(char[7]){0x1b,0x1d,0x1a,0x12,'1',0x00,0x00}},
 {7,(char[7]){0x1b,0x1d,0x1a,0x12,'1',0x04,0x04}},
 {7,(char[7]){0x1b,0x1d,0x1a,0x12,'1',0x0a,0x0a}},
 {7,(char[7]){0x1b,0x1d,0x1a,0x12,'1',0x14,0x14}}};

// define snout2(Red LED) interval setting command
// timing: jobSetup.1.5
// models: TUP592
static const struct command snout2IntervalCommand[4] =
{{7,(char[7]){0x1b,0x1d,0x1a,0x12,'2',0x00,0x00}},
 {7,(char[7]){0x1b,0x1d,0x1a,0x12,'2',0x04,0x04}},
 {7,(char[7]){0x1b,0x1d,0x1a,0x12,'2',0x0a,0x0a}},
 {7,(char[7]){0x1b,0x1d,0x1a,0x12,'2',0x14,0x14}}};

// define document top suond command
// timing: jobSetup.1.6
// models: FVP10
static const struct command documentTopSoundCommand[21] =
{{1,(char[1]){0x00}},                                                             // 0  No Sound
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x01,0x01,0x00,0x00,0x00,0x00,0x00}},  // 1  Sound1
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x02,0x01,0x00,0x00,0x00,0x00,0x00}},  // 2  Sound2
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x03,0x01,0x00,0x00,0x00,0x00,0x00}},  // 3  Sound3
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x04,0x01,0x00,0x00,0x00,0x00,0x00}},  // 4  Sound4 
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x05,0x01,0x00,0x00,0x00,0x00,0x00}},  // 5  Sound5
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x06,0x01,0x00,0x00,0x00,0x00,0x00}},  // 6  Sound6
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x07,0x01,0x00,0x00,0x00,0x00,0x00}},  // 7  Sound7
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x08,0x01,0x00,0x00,0x00,0x00,0x00}},  // 8  Sound8
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x09,0x01,0x00,0x00,0x00,0x00,0x00}},  // 9  Sound9
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x0A,0x01,0x00,0x00,0x00,0x00,0x00}},  // 10 Sound10
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x0B,0x01,0x00,0x00,0x00,0x00,0x00}},  // 11 Sound11
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x0C,0x01,0x00,0x00,0x00,0x00,0x00}},  // 12 Sound12
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x0D,0x01,0x00,0x00,0x00,0x00,0x00}},  // 13 Sound13
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x0E,0x01,0x00,0x00,0x00,0x00,0x00}},  // 14 Sound14
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x0F,0x01,0x00,0x00,0x00,0x00,0x00}},  // 15 Sound15
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x10,0x01,0x00,0x00,0x00,0x00,0x00}},  // 16 Sound16
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x11,0x01,0x00,0x00,0x00,0x00,0x00}},  // 17 Sound17
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x12,0x01,0x00,0x00,0x00,0x00,0x00}},  // 18 Sound18
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x13,0x01,0x00,0x00,0x00,0x00,0x00}},  // 19 Sound19
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x14,0x01,0x00,0x00,0x00,0x00,0x00}}}; // 20 Sound20


// define document bottom suond command
// timing: jobSetup.1.6
// models: FVP10
static const struct command documentBottomSoundCommand[21] =
{{1,(char[1]){0x00}},                                                             // 0  No Sound
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x01,0x01,0x00,0x00,0x00,0x00,0x00}},  // 1  Sound1
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x02,0x01,0x00,0x00,0x00,0x00,0x00}},  // 2  Sound2
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x03,0x01,0x00,0x00,0x00,0x00,0x00}},  // 3  Sound3
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x04,0x01,0x00,0x00,0x00,0x00,0x00}},  // 4  Sound4 
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x05,0x01,0x00,0x00,0x00,0x00,0x00}},  // 5  Sound5
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x06,0x01,0x00,0x00,0x00,0x00,0x00}},  // 6  Sound6
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x07,0x01,0x00,0x00,0x00,0x00,0x00}},  // 7  Sound7
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x08,0x01,0x00,0x00,0x00,0x00,0x00}},  // 8  Sound8
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x09,0x01,0x00,0x00,0x00,0x00,0x00}},  // 9  Sound9
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x0A,0x01,0x00,0x00,0x00,0x00,0x00}},  // 10 Sound10
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x0B,0x01,0x00,0x00,0x00,0x00,0x00}},  // 11 Sound11
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x0C,0x01,0x00,0x00,0x00,0x00,0x00}},  // 12 Sound12
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x0D,0x01,0x00,0x00,0x00,0x00,0x00}},  // 13 Sound13
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x0E,0x01,0x00,0x00,0x00,0x00,0x00}},  // 14 Sound14
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x0F,0x01,0x00,0x00,0x00,0x00,0x00}},  // 15 Sound15
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x10,0x01,0x00,0x00,0x00,0x00,0x00}},  // 16 Sound16
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x11,0x01,0x00,0x00,0x00,0x00,0x00}},  // 17 Sound17
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x12,0x01,0x00,0x00,0x00,0x00,0x00}},  // 18 Sound18
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x13,0x01,0x00,0x00,0x00,0x00,0x00}},  // 19 Sound19
 {13,(char[13]){0x1b,0x1d,'s','O',0x00,'1',0x14,0x01,0x00,0x00,0x00,0x00,0x00}}}; // 20 Sound20

// define data Treatment Recover From Error command
// timing: jobSetup.1.7
// models: TSP800II, FVP10, TSP654, TSP651, TSP700II
static const struct command dataTreatmentRecoverFromErrorCommand[3] =
{{1,(char[1]){0x00}},                              // 0 Store Data
 {6,(char[6]){0x1b,0x1d,0x03,0x03,0x00,0x00}},     // 1 Clear Data Start
 {6,(char[6]){0x1b,0x1d,0x03,0x04,0x00,0x00}}};    // 2 Clear Data Fnish


// define raster mode start command
// timing: jobSetup.2
// models: all
static const struct command rasterModeStartCommand =
{8,(char[8]){0x1b,'*','r','R',0x1b,'*','r','A'}};

// define print speed commands
// timing: jobSetup.3
// models: all
static const struct command printSpeedCommand [4] =               // Basic    / TUP500
{{6,(char[6]){0x1b,'*','r','Q','0',0x00}},                        // 0 High   / 0 Standard
 {6,(char[6]){0x1b,'*','r','Q','1',0x00}},                        // 1 Middle / 1 Middle
 {6,(char[6]){0x1b,'*','r','Q','2',0x00}},                        // 2 Low    / 2 Low
 {6,(char[6]){0x1b,'*','r','Q','3',0x00}}};                       //          / 3 High

// define set Raster Page Length Command
// timing: jobSetup.3
// models: TSP650, HSP7000_Receipt
static const struct command setRasterPageLengthCommand =
{6,(char[6]){0x1b,'*','r','P','0',0x00}};

// define cash drawer commands
// timing: jobSetup.4
// models: TSP100, TSP600, TSP700, TSP800, TSP800II, FVP10
static const struct command cashDrawerSettingCommand [4] =
{{6,(char[6]){0x1b,'*','r','D','0',0x00}},                        // 0 do not open drawers
 {6,(char[6]){0x1b,'*','r','D','1',0x00}},                        // 1 open drawer 1
 {6,(char[6]){0x1b,'*','r','D','2',0x00}},                        // 2 open drawer 2
 {6,(char[6]){0x1b,'*','r','D','3',0x00}}};                       // 3 open drawers 1 & 2

// define page type commands
// timing: jobSetup.5
// models: all
static const struct command pageTypeCommand [3] =
{{6,(char[6]){0x1b,'*','r','P','0',0x00}},                        // 0 receipt mode
 {4,(char[4]){0x1b,'*','r','P'         }},                        // 1 ticket mode
 {6,(char[6]){0x1b,'*','r','P','0',0x00}}};                       // 2 label mode
#define PAGETYPE_RECEIPT 0
#define PAGETYPE_TICKET  1
#define PAGETYPE_LABEL   2

// define top search commands
// timing: jobSetup.6
// models: all
static const struct command topSearchCommand [2] =
{{6,(char[6]){0x1b,'*','r','T','2',0x00}},                        // 0 disabled
 {6,(char[6]){0x1b,'*','r','T','1',0x00}}};                       // 1 enabled

// define page cut type commands
// timing: jobSetup.7
// models: TSP100,TSP600,TSP700,TSP800, TSP800II
static const struct command pageCutTypeCommand [4] =
{{6,(char[6]){0x1b,'*','r','F','1',0x00}},                        // 0 no cut         (TSP113,TSP143,TSP613,TSP643,TSP651,TSP654,TSP700,TSP800,TSP800II,FVP10)
 {7,(char[7]){0x1b,'*','r','F','1','3',0x00}},                    // 1 partial cut    (TSP143,TSP643,TSP654,TSP700,TSP800,TSP800II,FVP10)
 {6,(char[6]){0x1b,'*','r','F','9',0x00}},                        // 2 full cut       (TSP700,TSP800,TSP800II)
 {6,(char[6]){0x1b,'*','r','F','3',0x00}}};                       // 3 tear bar       (TSP113,TSP613,TSP651)

// define doc cut type commands
// timing: jobSetup.8
// models: all
static const struct command docCutTypeCommand [6] =
{{6, (char[ 6]){0x1b,'*','r','E','1',0x00}},                              // 0 no cut              (TSP113,TSP143,TSP613,TSP643,TSP651,TSP654,TSP700,TSP800,TSP800II,FVP10)
 {7, (char[ 7]){0x1b,'*','r','E','1','3',0x00}},                          // 1 partial cut         (TSP143,TSP643,TSP654,TSP700,TSP800,TSP800II,FVP10)
 {6, (char[ 6]){0x1b,'*','r','E','9',0x00}},                              // 2 full cut            (TSP700,TSP800,TSP800II)
 {6, (char[ 6]){0x1b,'*','r','E','3',0x00}},                              // 3 tear bar            (TSP113,TSP613,TSP651,TSP700,TSP800,TSP800II)
 {12,(char[12]){0x1b,'*','r','F','9',0x00,0x1b,'*','r','E','9',0x00}},    // 4 full cut all pages  (TUP900,TUP500)
 {12,(char[12]){0x1b,'*','r','F','1',0x00,0x1b,'*','r','E','9',0x00}}};   // 5 full cut last page  (TUP900,TUP500)

// define start page command
// timing: pageSetup.0
// models: all
static const struct command startPageCommand =
{1,(char[1]){0x00}};

// define end page command
// timing: endPage.0
// models: all
static const struct command endPageCommand =
{8,(char[8]){0x1b,'*','r','Y','1',0x00,0x1b,0x0c}};

// define end job command
// timing: endJob.0
// models: all
static const struct command endJobCommand =
{5,(char[5]){0x04,0x1b,'*','r','B'}};

void debugPrintSettings(struct settings_ * settings)
{
  fprintf(stderr, "DEBUG: pageType = %d\n"    , settings->pageType);
  fprintf(stderr, "DEBUG: focusArea = %d\n"   , settings->focusArea);
  fprintf(stderr, "DEBUG: printSpeed = %d\n"  , settings->printSpeed);
  fprintf(stderr, "DEBUG: topSearch = %d\n"   , settings->topSearch);
  fprintf(stderr, "DEBUG: pageCutType = %d\n" , settings->pageCutType);
  fprintf(stderr, "DEBUG: docCutType = %d\n"  , settings->docCutType);
  fprintf(stderr, "DEBUG: cashDrawerSetting = %d\n"  , settings->cashDrawerSetting);
  fprintf(stderr, "DEBUG: cashDrawer1PulseWidth = %d\n", settings->cashDrawer1PulseWidth);
  fprintf(stderr, "DEBUG: peripheralSetting = %d\n", settings->peripheralSetting);
  fprintf(stderr, "DEBUG: peripheralActionPulseWidth = %d\n", settings->peripheralActivationPulseWidth);
  fprintf(stderr, "DEBUG: presenterAction = %d\n", settings->presenterAction);
  fprintf(stderr, "DEBUG: presenterTimeout = %d\n", settings->presenterTimeout);
  fprintf(stderr, "DEBUG: mediaType = %d\n", settings->mediaType);
  fprintf(stderr, "DEBUG: labelDetect = %d\n", settings->labelDetect);
  fprintf(stderr, "DEBUG: printDensity = %d\n", settings->printDensity);
  fprintf(stderr, "DEBUG: snoutControl = %d\n", settings->snoutControl);
  fprintf(stderr, "DEBUG: snout1Interval = %d\n", settings->snout1Interval);
  fprintf(stderr, "DEBUG: snout2Interval = %d\n", settings->snout2Interval);
  fprintf(stderr, "DEBUG: bytesPerScanLine = %d\n", settings->bytesPerScanLine);
  fprintf(stderr, "DEBUG: buzzer1Setting = %d\n", settings->buzzer1Setting);
  fprintf(stderr, "DEBUG: buzzer1OnTime = %d\n", settings->buzzer1OnTime);
  fprintf(stderr, "DEBUG: buzzer1OffTime = %d\n", settings->buzzer1OffTime);
  fprintf(stderr, "DEBUG: buzzer1Repeat = %d\n", settings->buzzer1Repeat);
  fprintf(stderr, "DEBUG: buzzer2Setting = %d\n", settings->buzzer2Setting);
  fprintf(stderr, "DEBUG: buzzer2OnTime = %d\n", settings->buzzer2OnTime);
  fprintf(stderr, "DEBUG: buzzer2OffTime = %d\n", settings->buzzer2OffTime);
  fprintf(stderr, "DEBUG: buzzer2Repeat = %d\n", settings->buzzer2Repeat);
  fprintf(stderr, "DEBUG: documentTopSound = %d\n", settings->documentTopSound);
  fprintf(stderr, "DEBUG: documentBottomSound = %d\n", settings->documentBottomSound);
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

    settings->pageType                       = getOptionChoiceIndex("PageType"                      , ppd);
    settings->focusArea                      = getOptionChoiceIndex("FocusArea"                     , ppd);
    settings->printSpeed                     = getOptionChoiceIndex("PrintSpeed"                    , ppd);
    settings->topSearch                      = getOptionChoiceIndex("TopSearch"                     , ppd);
    settings->pageCutType                    = getOptionChoiceIndex("PageCutType"                   , ppd);
    settings->docCutType                     = getOptionChoiceIndex("DocCutType"                    , ppd);
    settings->cashDrawerSetting              = getOptionChoiceIndex("CashDrawerSetting"             , ppd);
    settings->cashDrawer1PulseWidth          = getOptionChoiceIndex("CashDrawer1PulseWidth"         , ppd);
    settings->peripheralSetting              = getOptionChoiceIndex("PeripheralSetting"             , ppd);
    settings->peripheralActivationPulseWidth = getOptionChoiceIndex("PeripheralActivationPulseWidth", ppd);
    settings->presenterAction                = getOptionChoiceIndex("PresenterAction"               , ppd);
    settings->presenterTimeout               = getOptionChoiceIndex("PresenterTimeout"              , ppd);
    settings->mediaType                      = getOptionChoiceIndex("MediaType"                     , ppd);
    settings->labelDetect                    = getOptionChoiceIndex("LabelDetect"                   , ppd);
    settings->printDensity                   = getOptionChoiceIndex("PrintDensity"                  , ppd);
    settings->snoutControl                   = getOptionChoiceIndex("SnoutControl"                  , ppd);
    settings->snout1Interval                 = getOptionChoiceIndex("Snout1Interval"                , ppd);
    settings->snout2Interval                 = getOptionChoiceIndex("Snout2Interval"                , ppd);
    settings->documentTopSound               = getOptionChoiceIndex("DocumentTopSound"              , ppd);
    settings->documentBottomSound            = getOptionChoiceIndex("DocumentBottomSound"           , ppd);
    settings->dataTreatmentRecoverFromError  = getOptionChoiceIndex("DataTreatmentRecoverFromError" , ppd);

    switch (modelNumber)
    {
        case HSP7000R:  settings->bytesPerScanLine    = HSP7000R_MAX_WIDTH;
                        settings->bytesPerScanLineStd = HSP7000R_STD_WIDTH; break;

        case TSP1000:   settings->bytesPerScanLine    = TSP1000_MAX_WIDTH;
                        settings->bytesPerScanLineStd = TSP1000_STD_WIDTH; break;

        case TUP992:
        case TUP942:    settings->bytesPerScanLine    = TUP900_MAX_WIDTH;
                        settings->bytesPerScanLineStd = TUP900_STD_WIDTH; break;

        case TSP800:
                        settings->bytesPerScanLine    = TSP800_MAX_WIDTH;
                        settings->bytesPerScanLineStd = TSP800_STD_WIDTH; break;

        case TSP800II:
                        settings->bytesPerScanLine    = TSP800II_MAX_WIDTH;
                        settings->bytesPerScanLineStd = TSP800II_STD_WIDTH; break;

        case TSP828L:   settings->bytesPerScanLine    = TSP828L_MAX_WIDTH;
                        settings->bytesPerScanLineStd = TSP828L_STD_WIDTH; break;

        case TSP700II:
        case TSP700:    settings->bytesPerScanLine    = TSP700_MAX_WIDTH;
                        settings->bytesPerScanLineStd = TSP700_STD_WIDTH; break;

        case TSP651:
        case TSP654:    settings->bytesPerScanLine    = TSP650_MAX_WIDTH;
                        settings->bytesPerScanLineStd = TSP650_STD_WIDTH; break;

        case TSP643:
        case TSP613:    settings->bytesPerScanLine    = TSP600_MAX_WIDTH;
                        settings->bytesPerScanLineStd = TSP600_STD_WIDTH; break;

        case TUP542:
        case TUP592:
                        settings->bytesPerScanLine    = TUP500_MAX_WIDTH;
                        settings->bytesPerScanLineStd = TUP500_STD_WIDTH; break;

        case FVP10:     settings->bytesPerScanLine    = FVP10_MAX_WIDTH;
                        settings->bytesPerScanLineStd = FVP10_STD_WIDTH; break;

        case TSP143:
        case TSP113:    settings->bytesPerScanLine    = TSP100_MAX_WIDTH;
                        settings->bytesPerScanLineStd = TSP100_STD_WIDTH; break;
    }

    switch (modelNumber)
    {
        case TSP1000:
        case TUP992:
        case TUP942:
        case TSP800:
        case TSP800II:
        case TSP828L:
        case TSP700:
        case TSP643:
        case TSP613:
        case TUP592:
        case TUP542:
        case TSP143:
        case TSP113:    settings->buzzer1Setting = 0;
                        settings->buzzer1OnTime  = 0;
                        settings->buzzer1OffTime = 0;
                        settings->buzzer1Repeat  = 0;
                        settings->buzzer2Setting = 0;
                        settings->buzzer2OnTime  = 0;
                        settings->buzzer2OffTime = 0;
                        settings->buzzer2Repeat  = 0;

            break;

        case TSP654:
        case TSP651:
        case HSP7000R:
        case FVP10:
        case TSP700II:
                        settings->buzzer1Setting = getOptionChoiceIndex("Buzzer1Setting" , ppd);
                        settings->buzzer1OnTime  = getOptionChoiceIndex("Buzzer1OnTime"  , ppd);
                        settings->buzzer1OffTime = getOptionChoiceIndex("Buzzer1OffTime" , ppd);
                        settings->buzzer1Repeat  = getOptionChoiceIndex("Buzzer1Repeat"  , ppd);
                        settings->buzzer2Setting = getOptionChoiceIndex("Buzzer2Setting" , ppd);
                        settings->buzzer2OnTime  = getOptionChoiceIndex("Buzzer2OnTime"  , ppd);
                        settings->buzzer2OffTime = getOptionChoiceIndex("Buzzer2OffTime" , ppd);
                        settings->buzzer2Repeat  = getOptionChoiceIndex("Buzzer2Repeat"  , ppd);

            break;
    }

    getPageWidthPageHeight(ppd, settings);

    PPDCLOSE(ppd);

    debugPrintSettings(settings);
}

void jobSetup(struct settings_ settings, char *argv[])
{
    outputCommand(printerInitializeCommand);

    if (settings.modelNumber == FVP10)
      {
	if (settings.dataTreatmentRecoverFromError == DATACANCEL_DOC )
	  {
	    outputCommand(dataTreatmentRecoverFromErrorCommand[1]);
	  }
      }

    if (settings.modelNumber == TSP800II)
      {
	if (settings.dataTreatmentRecoverFromError == DATACANCEL_DOC )
	  {
	    outputCommand(dataTreatmentRecoverFromErrorCommand[1]);
	  }
      }

    if (settings.modelNumber == TSP700II)
      {
	if (settings.dataTreatmentRecoverFromError == DATACANCEL_DOC )
	  {
	    outputCommand(dataTreatmentRecoverFromErrorCommand[1]);
	  }
      }

    if (settings.modelNumber == TSP654)
      {
	if (settings.dataTreatmentRecoverFromError == DATACANCEL_DOC )
	  {
	    outputCommand(dataTreatmentRecoverFromErrorCommand[1]);
	  }
      }

    if (settings.modelNumber == TSP651)
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
            switch(settings.modelNumber)
            {
                case TUP992:
                case TUP942:
                case TUP592:
                case TUP542:
                case TSP700II:
                case TSP800II:
                case TSP654:
                case TSP651:
                case HSP7000R:
                case FVP10:
                        outputCommand(nsbRequestSettingCommand[0]);

                    break;
                default:
                    break;
            }
        }
    }
    #endif

    if (settings.modelNumber == HSP7000R)
    {
        outputCommand(stationSettingCommand);
    }

    if (settings.modelNumber == TSP1000)
    {
        if      (settings.pageWidth == 80.0f) outputCommand(printableWidthCommand[0]);
        else if (settings.pageWidth == 72.0f) outputCommand(printableWidthCommand[1]);
        else if (settings.pageWidth == 55.0f) outputCommand(printableWidthCommand[2]);
        else if (settings.pageWidth == 52.0f) outputCommand(printableWidthCommand[3]);
        else if (settings.pageWidth == 47.0f) outputCommand(printableWidthCommand[4]);
        else if (settings.pageWidth == 42.0f) outputCommand(printableWidthCommand[5]);
        else outputCommand(printableWidthCommand[0]);
    }

    if (settings.modelNumber == TSP113)
    {
        if      (settings.pageWidth == 72.0f) outputCommand(printableWidthCommand[0]);
        else if (settings.pageWidth == 50.8f) outputCommand(printableWidthCommand[1]);
        else outputCommand(printableWidthCommand[0]);
    }

    if (settings.modelNumber == TSP143)
    {
        if      (settings.pageWidth == 72.0f) outputCommand(printableWidthCommand[0]);
        else if (settings.pageWidth == 50.8f) outputCommand(printableWidthCommand[1]);
        else outputCommand(printableWidthCommand[0]);
    }

    if (settings.modelNumber == TSP651)
    {
        if      (settings.pageWidth == 72.0f) outputCommand(printableWidthCommand[0]);
        else if (settings.pageWidth == 50.8f) outputCommand(printableWidthCommand[1]);
        else outputCommand(printableWidthCommand[0]);
    }

    if (settings.modelNumber == TSP654)
    {
        if      (settings.pageWidth == 72.0f) outputCommand(printableWidthCommand[0]);
        else if (settings.pageWidth == 50.8f) outputCommand(printableWidthCommand[1]);
        else outputCommand(printableWidthCommand[0]);
    }

    if (settings.modelNumber == HSP7000R)
    {
        if      (settings.pageWidth == 72.0f) outputCommand(printableWidthCommand[0]);
        else if (settings.pageWidth == 50.8f) outputCommand(printableWidthCommand[1]);
        else outputCommand(printableWidthCommand[0]);
    }

    if (settings.modelNumber == TSP700II)
    {
        if      (settings.pageWidth == 72.0f) outputCommand(printableWidthCommand[0]);
        else if (settings.pageWidth == 52.5f) outputCommand(printableWidthCommand[1]);
        else if (settings.pageWidth == 80.0f) outputCommand(printableWidthCommand[2]);
        else if (settings.pageWidth == 50.8f) outputCommand(printableWidthCommand[3]);
        else if (settings.pageWidth == 52.0f) outputCommand(printableWidthCommand[4]);
        else outputCommand(printableWidthCommand[0]);
    }

    if (settings.modelNumber == TUP542 || settings.modelNumber == TUP592)
    {
        if      (settings.pageWidth == 80.0f) outputCommand(printableWidthCommandForTUP500[0]);
        else if (settings.pageWidth == 72.0f) outputCommand(printableWidthCommandForTUP500[1]);
        else if (settings.pageWidth == 68.0f) outputCommand(printableWidthCommandForTUP500[2]);
        else if (settings.pageWidth == 51.0f) outputCommand(printableWidthCommandForTUP500[3]);
        else outputCommand(printableWidthCommandForTUP500[0]);
    }

    if (settings.modelNumber == FVP10)
    {
        if      (settings.pageWidth == 72.0f) outputCommand(printableWidthCommand[0]);
        else if (settings.pageWidth == 52.5f) outputCommand(printableWidthCommand[1]);
        else if (settings.pageWidth == 80.0f) outputCommand(printableWidthCommand[2]);
        else if (settings.pageWidth == 50.8f) outputCommand(printableWidthCommand[3]);
        else if (settings.pageWidth == 52.0f) outputCommand(printableWidthCommand[4]);
        else outputCommand(printableWidthCommand[0]);
    }

    if (settings.modelNumber == TSP828L)
    {
        if( settings.mediaType == 1 )
        {
            outputCommand(labelDetectCommand[settings.labelDetect]);
        }
        else
        {
            outputCommand(mediaTypeCommand[settings.mediaType]);
        }

        outputCommand(printDensityCommand[settings.printDensity]);
    }

    if (settings.modelNumber == TSP651)
    {
        outputCommand(printDensityCommand[settings.printDensity]);
    }

    if (settings.modelNumber == TSP654)
    {
        outputCommand(printDensityCommand[settings.printDensity]);
    }

    if (settings.modelNumber == HSP7000R)
    {
        outputCommand(printDensityCommand[settings.printDensity]);
    }

    if (settings.modelNumber == FVP10)
    {
        outputCommand(printDensityCommand[settings.printDensity]);
    }


    if (settings.modelNumber == TSP1000)
    {
        if (settings.peripheralSetting != 0)
        {
            outputCommand(peripheralActivationPulseWidthCommand[settings.peripheralActivationPulseWidth]);
        }
    }

    if (settings.modelNumber == TSP651)
    {
        if (settings.buzzer1Setting == BUZZER_DOC_TOP)
        {
            outputCommand(buzzer1OnTimeCommand[settings.buzzer1OnTime]);
            outputCommand(buzzer1OffTimeCommand[settings.buzzer1OffTime]);
            outputCommand(buzzer1SettingCommand[settings.buzzer1Repeat]);
        }
    }

    if (settings.modelNumber == TSP651)
    {
        if (settings.buzzer2Setting == BUZZER_DOC_TOP)
        {
            outputCommand(buzzer2OnTimeCommand[settings.buzzer2OnTime]);
            outputCommand(buzzer2OffTimeCommand[settings.buzzer2OffTime]);
            outputCommand(buzzer2SettingCommand[settings.buzzer2Repeat]);
        }
    }

    if (settings.modelNumber == TSP654)
    {
        if (settings.buzzer1Setting == BUZZER_DOC_TOP)
        {
            outputCommand(buzzer1OnTimeCommand[settings.buzzer1OnTime]);
            outputCommand(buzzer1OffTimeCommand[settings.buzzer1OffTime]);
            outputCommand(buzzer1SettingCommand[settings.buzzer1Repeat]);
        }
    }

    if (settings.modelNumber == TSP654)
    {
        if (settings.buzzer2Setting == BUZZER_DOC_TOP)
        {
            outputCommand(buzzer2OnTimeCommand[settings.buzzer2OnTime]);
            outputCommand(buzzer2OffTimeCommand[settings.buzzer2OffTime]);
            outputCommand(buzzer2SettingCommand[settings.buzzer2Repeat]);
        }
    }

    if (settings.modelNumber == HSP7000R)
    {
        if (settings.buzzer1Setting == BUZZER_DOC_TOP)
        {
            outputCommand(buzzer1OnTimeCommand[settings.buzzer1OnTime]);
            outputCommand(buzzer1OffTimeCommand[settings.buzzer1OffTime]);
            outputCommand(buzzer1SettingCommand[settings.buzzer1Repeat]);
        }
    }

    if (settings.modelNumber == HSP7000R)
    {
        if (settings.buzzer2Setting == BUZZER_DOC_TOP)
        {
            outputCommand(buzzer2OnTimeCommand[settings.buzzer2OnTime]);
            outputCommand(buzzer2OffTimeCommand[settings.buzzer2OffTime]);
            outputCommand(buzzer2SettingCommand[settings.buzzer2Repeat]);
        }
    }

    if (settings.modelNumber == FVP10)
    {
        if (settings.buzzer1Setting == BUZZER_DOC_TOP)
        {
            outputCommand(buzzer1OnTimeCommand[settings.buzzer1OnTime]);
            outputCommand(buzzer1OffTimeCommand[settings.buzzer1OffTime]);
            outputCommand(buzzer1SettingCommand[settings.buzzer1Repeat]);
        }
    }

    if (settings.modelNumber == FVP10)
    {
        if (settings.buzzer2Setting == BUZZER_DOC_TOP)
        {
            outputCommand(buzzer2OnTimeCommand[settings.buzzer2OnTime]);
            outputCommand(buzzer2OffTimeCommand[settings.buzzer2OffTime]);
            outputCommand(buzzer2SettingCommand[settings.buzzer2Repeat]);
        }
    }

    if (settings.modelNumber == TSP700II)
    {
        if (settings.buzzer1Setting == BUZZER_DOC_TOP)
        {
            outputCommand(buzzer1OnTimeCommand[settings.buzzer1OnTime]);
            outputCommand(buzzer1OffTimeCommand[settings.buzzer1OffTime]);
            outputCommand(buzzer1SettingCommand[settings.buzzer1Repeat]);
        }
    }

    if (settings.modelNumber == TSP700II)
    {
        if (settings.buzzer2Setting == BUZZER_DOC_TOP)
        {
            outputCommand(buzzer2OnTimeCommand[settings.buzzer2OnTime]);
            outputCommand(buzzer2OffTimeCommand[settings.buzzer2OffTime]);
            outputCommand(buzzer2SettingCommand[settings.buzzer2Repeat]);
        }
    }

    if (settings.cashDrawer1PulseWidth > -1)
    {
        outputCommand(cashDrawer1PulseWidthCommand[settings.cashDrawer1PulseWidth]);
    }

    if (settings.presenterAction > -1)
    {
        outputCommand(presenterActionCommand[settings.presenterAction]);
    }

    if (settings.presenterTimeout > -1)
    {
        outputCommand(presenterTimeoutCommand[settings.presenterTimeout]);
    }

    if (settings.snoutControl > -1)
    {
        fprintf(stderr, "DEBUG: \n\n\n\n snout\n"); 
        fprintf(stderr, "DEBUG: %d\n", settings.snoutControl); 
        fprintf(stderr, "DEBUG: %d\n", settings.snout1Interval); 
        fprintf(stderr, "DEBUG: %d\n", settings.snout2Interval); 

        outputCommand(snoutControlCommand[settings.snoutControl]);
        outputCommand(snout1IntervalCommand[settings.snout1Interval]);
        outputCommand(snout2IntervalCommand[settings.snout2Interval]);
    }

    if (settings.documentTopSound > -1)
    {
        outputCommand(documentTopSoundCommand[settings.documentTopSound]);
    }

    outputCommand(rasterModeStartCommand);

    outputCommand(printSpeedCommand[settings.printSpeed]);
    if (settings.modelNumber == TSP651)
    {
        outputCommand(setRasterPageLengthCommand);
    }

    if (settings.modelNumber == TSP654)
    {
        outputCommand(setRasterPageLengthCommand);
    }

    if (settings.modelNumber == HSP7000R)
    {
        outputCommand(setRasterPageLengthCommand);
    }

    if (settings.cashDrawerSetting > -1)
    {
        outputCommand(cashDrawerSettingCommand[settings.cashDrawerSetting]);
    }

    outputCommand(pageTypeCommand[PAGETYPE_RECEIPT]);

    if (settings.topSearch > -1)
    {
        outputCommand(topSearchCommand[settings.topSearch]);
    }

    if (settings.pageCutType > -1)
    {
        outputCommand(pageCutTypeCommand[settings.pageCutType]);
    }

    outputCommand(docCutTypeCommand[settings.docCutType]);
}

void pageSetup(struct settings_ settings, cups_page_header2_t header)
{
    outputCommand(startPageCommand);
}

void endPage(struct settings_ settings)
{
    outputCommand(endPageCommand);
}

void endJob(struct settings_ settings, char *argv[])
{
    outputCommand(endJobCommand);

    if (settings.modelNumber == TSP651)
    {
        if (settings.buzzer1Setting == BUZZER_DOC_BTM)
        {
            outputCommand(buzzer1OnTimeCommand[settings.buzzer1OnTime]);
            outputCommand(buzzer1OffTimeCommand[settings.buzzer1OffTime]);
            outputCommand(buzzer1SettingCommand[settings.buzzer1Repeat]);
        }
    }

    if (settings.modelNumber == TSP651)
    {
        if (settings.buzzer2Setting == BUZZER_DOC_BTM)
        {
            outputCommand(buzzer2OnTimeCommand[settings.buzzer2OnTime]);
            outputCommand(buzzer2OffTimeCommand[settings.buzzer2OffTime]);
            outputCommand(buzzer2SettingCommand[settings.buzzer2Repeat]);
        }
    }

    if (settings.modelNumber == TSP654)
    {
        if (settings.buzzer1Setting == BUZZER_DOC_BTM)
        {
            outputCommand(buzzer1OnTimeCommand[settings.buzzer1OnTime]);
            outputCommand(buzzer1OffTimeCommand[settings.buzzer1OffTime]);
            outputCommand(buzzer1SettingCommand[settings.buzzer1Repeat]);
        }
    }

    if (settings.modelNumber == TSP654)
    {
        if (settings.buzzer2Setting == BUZZER_DOC_BTM)
        {
            outputCommand(buzzer2OnTimeCommand[settings.buzzer2OnTime]);
            outputCommand(buzzer2OffTimeCommand[settings.buzzer2OffTime]);
            outputCommand(buzzer2SettingCommand[settings.buzzer2Repeat]);
        }
    }

    if (settings.modelNumber == HSP7000R)
    {
        if (settings.buzzer1Setting == BUZZER_DOC_BTM)
        {
            outputCommand(buzzer1OnTimeCommand[settings.buzzer1OnTime]);
            outputCommand(buzzer1OffTimeCommand[settings.buzzer1OffTime]);
            outputCommand(buzzer1SettingCommand[settings.buzzer1Repeat]);
        }
    }

    if (settings.modelNumber == HSP7000R)
    {
        if (settings.buzzer2Setting == BUZZER_DOC_BTM)
        {
            outputCommand(buzzer2OnTimeCommand[settings.buzzer2OnTime]);
            outputCommand(buzzer2OffTimeCommand[settings.buzzer2OffTime]);
            outputCommand(buzzer2SettingCommand[settings.buzzer2Repeat]);
        }
    }

    if (settings.modelNumber == FVP10)
    {
        if (settings.buzzer1Setting == BUZZER_DOC_BTM)
        {
            outputCommand(buzzer1OnTimeCommand[settings.buzzer1OnTime]);
            outputCommand(buzzer1OffTimeCommand[settings.buzzer1OffTime]);
            outputCommand(buzzer1SettingCommand[settings.buzzer1Repeat]);
        }
    }

    if (settings.modelNumber == FVP10)
    {
        if (settings.buzzer2Setting == BUZZER_DOC_BTM)
        {
            outputCommand(buzzer2OnTimeCommand[settings.buzzer2OnTime]);
            outputCommand(buzzer2OffTimeCommand[settings.buzzer2OffTime]);
            outputCommand(buzzer2SettingCommand[settings.buzzer2Repeat]);
        }
    }

    if (settings.modelNumber == TSP700II)
    {
        if (settings.buzzer1Setting == BUZZER_DOC_BTM)
        {
            outputCommand(buzzer1OnTimeCommand[settings.buzzer1OnTime]);
            outputCommand(buzzer1OffTimeCommand[settings.buzzer1OffTime]);
            outputCommand(buzzer1SettingCommand[settings.buzzer1Repeat]);
        }
    }

    if (settings.modelNumber == TSP700II)
    {
        if (settings.buzzer2Setting == BUZZER_DOC_BTM)
        {
            outputCommand(buzzer2OnTimeCommand[settings.buzzer2OnTime]);
            outputCommand(buzzer2OffTimeCommand[settings.buzzer2OffTime]);
            outputCommand(buzzer2SettingCommand[settings.buzzer2Repeat]);
        }
    }

    if (settings.documentBottomSound > -1)
    {
        outputCommand(documentBottomSoundCommand[settings.documentBottomSound]);
    }

    if (settings.modelNumber == FVP10)
      {
	if (settings.dataTreatmentRecoverFromError == DATACANCEL_DOC)
	  {
	    outputCommand(dataTreatmentRecoverFromErrorCommand[2]);
	  }
      }

    if (settings.modelNumber == TSP800II)
      {
	if (settings.dataTreatmentRecoverFromError == DATACANCEL_DOC)
	  {
	    outputCommand(dataTreatmentRecoverFromErrorCommand[2]);
	  }
      }

    if (settings.modelNumber == TSP700II)
      {
	if (settings.dataTreatmentRecoverFromError == DATACANCEL_DOC)
	  {
	    outputCommand(dataTreatmentRecoverFromErrorCommand[2]);
	  }
      }

    if (settings.modelNumber == TSP654)
      {
	if (settings.dataTreatmentRecoverFromError == DATACANCEL_DOC)
	  {
	    outputCommand(dataTreatmentRecoverFromErrorCommand[2]);
	  }
      }

    if (settings.modelNumber == TSP651)
      {
	if (settings.dataTreatmentRecoverFromError == DATACANCEL_DOC)
	  {
	    outputCommand(dataTreatmentRecoverFromErrorCommand[2]);
	  }
      }


    #ifdef __APPLE__
    #else
    switch(settings.modelNumber)
    {
        case TUP992:
        case TUP942:
        case TUP592:
        case TUP542:
        case TSP700II:
        case TSP800II:
        case TSP654:
        case TSP651:
        case HSP7000R:
        case FVP10:
             outputDummyData();
            break;
        default:
            break;
    }

    #endif

}

void jobSetupForPromotion(struct settings_ settings, char *argv[])
{
    if (settings.dataTreatmentRecoverFromError == DATACANCEL_DOC )
    {
        outputCommand(dataTreatmentRecoverFromErrorCommand[1]);
    }
    
    outputCommand(rasterModeStartCommand);

    outputCommand(setRasterPageLengthCommand);
    
    outputCommand(docCutTypeCommand[settings.docCutType]);
}

void endJobForPromotion(struct settings_ settings, char *argv[])
{
    outputCommand(endJobCommand);
    
    if (settings.dataTreatmentRecoverFromError == DATACANCEL_DOC)
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
    if (originalRasterDataPtr != NULL) free(originalRasterDataPtr);     \
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
    unsigned char *     rawRasterPixels         = NULL;     /* [AllReceipts] CUPS Raster Data (Generate NSImage)                                    */

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
    int cupsBytesPerLine = 0;
    int pxBufferedReceiptHeight = 0;

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

        pageSetup(settings, header);

        page++;
        fprintf(stderr, "PAGE: %d %d\n", page, header.NumCopies);

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
                    leftByteDiff = ((header.cupsBytesPerLine - (unsigned int)settings.bytesPerScanLine) / 2);
                    break;
                case FOCUS_RIGHT:
                    leftByteDiff = ((int)header.cupsBytesPerLine - settings.bytesPerScanLine);
                    break;
            }
        }

#ifdef STAR_CLOUD_SERVICES
        cupsBytesPerLine = header.cupsBytesPerLine;

        setPaperWidth(header.cupsBytesPerLine * 8);

        rawRasterPixels = (unsigned char *) calloc(header.cupsHeight * header.cupsBytesPerLine, sizeof(unsigned char));
        if (rawRasterPixels == NULL)
        {
            CLEANUP;

            return EXIT_FAILURE;
        }
        int rawRasterPixelSize = 0;
#endif

        for (y = 0; y < header.cupsHeight; y++)
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

            if (scanLineBlank == FALSE || settings.pageType == PAGETYPE_TICKET)
            {
                if (isMicroReceipt)
                {
                    numBlankScanLines = 0;
                }
                else
                {
                    if (numBlankScanLines > 0)
                    {
                        printf("\x01b*rY%d", numBlankScanLines);
                        putchar(0x00);

                        numBlankScanLines = 0;
                    }

                    if (scanLineBlank == FALSE)
                    {
                        putchar('b');
                        putchar((char) ((lastBlackPixel > 0)?(lastBlackPixel % 256):1));
                        putchar((char) (lastBlackPixel / 256));

                        for (i = 0; i < lastBlackPixel; i++)
                        {
                            putchar((char) *rasterData);
                            rasterData++;
                        }
                    }
                }
            }
            rasterData = originalRasterDataPtr;
        }

#ifdef STAR_CLOUD_SERVICES
        int pxSkipBytes = (settings.pageType == PAGETYPE_TICKET) ? 0 : (cupsBytesPerLine * numBlankScanLines);
        appendBytes(rawRasterPixels, rawRasterPixelSize - pxSkipBytes);
        free(rawRasterPixels);
        rawRasterPixels = NULL;

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
        // Cancel ESC FF ?? command
        putchar(0x00);

        int result = doAllReceiptsTasks(cupsBytesPerLine * 8, pxBufferedReceiptHeight);

        endPage(settings);
    }
    
#endif

    endJob(settings, argv);
    
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

            jobSetupForPromotion(settings,argv);
            doPromoPrntTasks(ImageFileList[i]);
            endJobForPromotion(settings, argv);
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
