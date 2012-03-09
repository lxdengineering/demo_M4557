// esi_M4557 demo
//
// TODO: Move "top" button up a bit in bitmap; And move its
//       touch-screen hot-zone up to match (increase in x direction).

#include <plib.h>       // PIC32 Peripheral library
#include <stdint.h>
#include <string.h>

#include "product_config.h"
#include "p32_utils.h"  // Our misc utils for pic32 (delays, etc)
#include "st7565.h"
#include "gfx.h"
#include "tsc2046.h"

// Defines for the different things we'll control the level of
#define DEMO_LIGHT 0
#define DEMO_FAN 1
#define DEMO_HEAT 2
#define DEMO_CONTRAST 3
// An array of saved levels for the above items. Each item ranges from 0..15.
// Indexes of this array are related to above defines. 
static int savedLevel[4] = { 0, 0, 0, 7 };

// Private prototypes
void demoLevel();


// Note: For the ESI module, we are going to treat the display as a 128x64
//       display (rather than 64x128), with pixel (0,0) at the bottom left.
//       The x-axis therefore goes from bottom to top; the y-axis goes
//       from left to right.  This is to better map with "standard"
//       128x64 display controllers (ST7565).


// A 1KByte bitmap buffer for the 128x64 pixel display
static uint8_t gfxBuf[1024];

// Touchscreen cal values, obtained from touches at LCD x,y locations
// that were 10 pixels in from bottom,left and top,right (10,10) and (117,53).
static int16_t cal_x0 = 0x220;         // Maps to LCD  x=10
static int16_t cal_dx = 0xf80-0x220;   // Maps to LCD dx=108
static int16_t cal_y0 = 0xD78;         // Maps to LCD  y=10
static int16_t cal_dy = 0x3C8-0xD78;   // Maps to LCD dy=44

// Dimmer switch base bitmap
extern const unsigned char esiDemoLevelSet[];
extern const unsigned char esiDemoTop[];


// PWM translation table.
//
// TODO: Fix this up.  This is a table that is roughly logarithmic, and
//       reasonable results for small LED on starter kit. (we start to
//       see the LED at around 1%PWM).
//       This table is "out-of-100000" (the value used in call to
//       set up timer2 for use by OC: OpenTimer2(..., 100000) -> 400Hz if pbClk = 40MHz.
//       May have to cal a table for each device we PWM control, e.g. big LED, little
//       LED, fan, whatever.
//       
uint32_t pwmTableLED[] = {  // TODO: log calcs for this table. We start to see the LED at ~1%pwm.
// for x in range(15,-1,-1): print .65 ** x * 100000
//    0,//156,
//    240,
//    369,
//    568,
//    875,
//    1346,
//    2071,
//    3186,
//    4902,
//    7541,
//    11602,
//    17850,
//    27462,
//    42250,
//    65000,
//    100000 // Matches the 100000 in OpenTimer2() call: 100% for 400Hz
//
// for x in range(15,-1,-1): print "   %f," % (.65 ** x * 2500)  # 16khZ
    0,//4,
    6,
    9,
    14,
    21,
    33,
    52,
    80,
    123,
    189,
    290,
    446,
    687,
    1056,
    1625,
    2500
};

// For the fan, a linear table, from around 75%->100% DC
// Note the fan PWM is NOISY at 400Hz & 4KHz.
// Turns out, fast pwm (>20KHz) is a good thing. Ref...
//    http://www.analog.com/library/analogDialogue/archives/38-02/fan_speed.html
//
uint32_t pwmTableFan[] = {
// max=2500; min=max*0.75; step=(max-min)/16.0;  # 75->100% about right for 80mm fan
// for x in range(1,17):  print "   %d," % round(x * step + min)
//    0,//1914,
//   1953,
//   1992,
//   2031,
//   2070,
//   2109,
//   2148,
//   2188,
//   2227,
//   2266,
//   2305,
//   2344,
//   2383,
//   2422,
//   2461,
//   2500,
// max=2500; min=max*0.25; step=(max-min)/16.0;  # 25->100% for (very) small fan
// for x in range(1,17):  print "   %d," % round(x * step + min)
    0,//742,
   859,
   977,
   1094,
   1211,
   1328,
   1445,
   1563,
   1680,
   1797,
   1914,
   2031,
   2148,
   2266,
   2383,
   2500
};



void calPoint(int16_t x_in, int16_t y_in,
              int16_t *x_out, int16_t *y_out)
{
    int16_t x, y;
    char dbgStr[64];
    
    gfxFCircle(x_in, y_in, 5, 1);          // Draw small circle at 10,10
    lcdWriteBuffer(gfxBuf);
    while(touchGetXY(&x,&y) == false);     // Wait for a touchscreen touch
    sprintf(dbgStr, "touchGetXY(x,y):(%04x,%04x)\n", x, y);
    DBPUTS(dbgStr);
    touchWaitForRelease();                 // Wait for touch release
    gfxFCircle(x_in, y_in, 5, 0);          // Clear circle
    *x_out = x;
    *y_out = y;
}


//
// Touch screen calibration - Show 2 cal points, and prompt
// user to touch at those locations.
//
void touchCalibration()
{
    int16_t x,y;
	char s[64];

    // Prompt user to touch the lower, left cal point
	gfxFill(0);
    gfxString(0,4,"Touch cal point 1..");
	lcdWriteBuffer(gfxBuf);

    // Get the lower, left cal point
    calPoint(10, 10, &cal_x0, &cal_y0);

    // Prompt user to touch the upper, right cal point
    gfxFill(0);
    gfxString(0,4,"Touch cal point 2..");
	lcdWriteBuffer(gfxBuf);

    // Get the upper, right cal point
    calPoint(127-10, 63-10, &x, &y);

    // Calculate the x & y ranges (dx and dy)
    cal_dx = x - cal_x0;
    cal_dy = y - cal_y0;

    // Display the 2 cal points (raw x,y coords)
    gfxFill(0);
    sprintf(s, "Cal p1:(%04x,%04x)", cal_x0, cal_y0);
    gfxString(0,2,s);
    sprintf(s, "Cal p2:(%04x,%04x)", x, y);
    gfxString(0,3,s);
    lcdWriteBuffer(gfxBuf);
}

// esiGetXY() - A wrapper for the touch-screen's touchGetXY() routine,
//              that translates its raw x,y locations into locations
//              calibrated and range adjusted for the ESI unit.
//              x,y ranges from (0,0) to (127,63).
//
bool esiGetXY(int16_t *x, int16_t *y)
{
    int32_t tmp;
    int16_t rawX, rawY;

    if(touchGetXY(&rawX, &rawY))   // Get the raw x,y values
    {
        tmp = rawX - cal_x0;
        tmp *= 108;
        tmp /= cal_dx;
        tmp += 10;
        if(tmp < 0) tmp = 0;
        if(tmp > 127) tmp = 127;
        *x = (int16_t)tmp;

        tmp = rawY - cal_y0;
        tmp *= 44;
        tmp /= cal_dy;
        tmp += 10;
        if(tmp < 0) tmp = 0;
        if(tmp > 63) tmp = 63;
        *y = (int16_t)tmp;

        return true;
    }
    else
    {
        *x = -1;
        *y = -1;
        return false;
    }
}


// testCal - Enters a forever loop that accepts user's touches, and
//           displays a cross-hair at that touch point (and also shows
//           the coordinates, in "LCD units").
void testCal()
{
	//unsigned char c;
    int16_t x,y;
	char dbgStr[64];

    while(1)
    {
        if(esiGetXY(&x,&y))
        {
            sprintf(dbgStr, "(x,y)=(%d,%d)", x, y);
            gfxFill(0);
            gfxString(20,3,dbgStr);  // Show x,y coord of touch point.
            gfxLine(x-20,y,x+20,y,1);  // Draw a cross-hair on touch point.
            gfxLine(x,y-20,x,y+20,1);
            lcdWriteBuffer(gfxBuf);
            //DBPUTS(dbgStr);
            touchWaitForRelease();
        }
        delay_ms(10);
        //DBPUTS("Cal complete; Any key...");
        //DBGETC(&c);
    }
}

// updateBarGraph() - Accepts a "level", from 0..15, and draws
//    a graphic display of 15 bars to show that level. For level 0,
//    all bars are "empty"; For level 15 all bars are "full".
void updateBarGraph(int level)
{
	int i;

    //static int oldLevel = 0;

	// Range check, and see if this is a level change.
	if(level < 0) level = 0;
	if(level > 15) level = 15;
    //if(oldLevel == level) return;  // Nothing new
    //oldLevel = level;

    //gfxFRect(45,18,117,54,0);
    //gfxFRect(45,23,117,49,0);   // Clear bargraph area (TEST VERSION: leave outer bar edges)
    gfxFRect(33,23,105,49,0);   // Clear bargraph area (TEST VERSION: leave outer bar edges)

    for(i=0; i<15; i++)
    {
        if(i < level) {
            gfxFRect(33+(i*5), 25, 35+(i*5), 47, 1);
        }
        else {
            gfxRect (33+(i*5), 25, 35+(i*5), 47, 1);
        }
    }
    lcdWriteBuffer(gfxBuf);
}


// ESI M4557 Demo
//
// Top level - 4 buttons:
//   "Light  "  - PWMx Adjust light
//   "Fan    "  - PWMx Adjust fan speed (or heat, or whatever)
//   "Heat   "  - n/a
//   "Display"  - LCD Display Contrast
//

void esi_M4557()
{
    int16_t x,y;

    // Initialize graphics variables (this clears the buffer)
    gfxInit(128, 64, gfxBuf);

    // Run calibration routine. Displays 2 dots; asks user to touch each.
    touchCalibration();


    // Touch Screen Test
    //testCal();  // Show coords & cross-hair of touches


    while(1)
    {
        // Copy top-level bitmap to display. Top level bitmap shows
        // four options / buttons:
        //    - Light
        //    - Fan
        //    - Heat
        //    - Display
        memcpy(gfxBuf, esiDemoTop, 1024);
        lcdWriteBuffer(gfxBuf);

		touchWaitForRelease();   // Make sure last press is inactive

        while(!esiGetXY(&x,&y))  // Wait for a button press
        {
            delay_ms(10);
        }

		touchWaitForRelease();   // Make sure last press is inactive

        // This is a button press from top-level menu. See
        // which button, and go to a "level" display for the
        // appropriate item (Light, Fan, etc... )
        //
        if(x > 2 && x < 30)        // Bottom 1/4 of touchscreen?
        {
            demoLevel("  Contrast", DEMO_CONTRAST);
        }
        else if(x > 34 && x < 62)  // 2nd from bottom?
        {
            demoLevel("  Heat", DEMO_HEAT);
        }
        else if(x > 66 && x < 94)  // 3rd from bottom?
        {
            demoLevel("  Fan", DEMO_FAN);
        }
        else if(x > 98)            // Top 1/4?
        {
            demoLevel("  Light", DEMO_LIGHT);
        }
    }
}


// demoLevel - Show a level bar-graph, and buttons to allow
//   increasing or decreasing the level. Also, a button to return
//   to the top-level menu is present.
//
// Inputs
//    leftSideText - A short string to describe what
//                   is being controlled. It is displayed
//                   to the left of the bar-graph.
//    demoNumber - ...
//
void demoLevel(char *leftSideText, int demoNumber)
{
	int16_t x,y;
    int level;   // The level of our "dimmer" or whatever, 0..15

    if(demoNumber < DEMO_LIGHT || demoNumber > DEMO_CONTRAST) return;

    // Copy base bitmap image to buffer, and add the descriptive string
    // along left edge.
    memcpy(gfxBuf, esiDemoLevelSet, 1024);
    gfxString(32, 0, leftSideText);

    // Init the level to the last savedlevel, for the item we are controlling
    level = savedLevel[demoNumber];
	updateBarGraph(level);

    // TEST-ONLY (?)
	// Test bar-graph - cycle through its range. Quit on any touch.
    /*
    char incOrDec;
    incOrDec = 1;
    level = 0;
    while(1)
    {
        level += incOrDec;             // Increment or Decrement
        if(level > 15)
        {
            level = 15;
            incOrDec = -1;
        }
        if(level < 0)
        {
            level = 0;
            incOrDec = 1;
        }

        updateBarGraph(level);         // Update bar graph
        SetDCOC2PWM(pwmTableLED[level]); // Update PWM level
        SetDCOC3PWM(pwmTableFan[level]); // Update PWM level
                    
        delay_ms(250);

		if(esiGetXY(&x,&y)) break;
    }
    */

	// Look for presses on the up/down buttons,
    // inc/dec the level, and update display & PWM level.
	while(1)
	{
		if(esiGetXY(&x,&y))
		{
			// Is this an up or down button press (bottom of screen)?
			if(x > 2 && x <24)
			{
                if(y > 2 && y < 25)        // Up button (left side of touch screen)?
                {
                    level++; 
                    if(level > 15) level = 15;
                }
                else if(y > 30 && y < 53)  // Down button (right side of touchscreen)?
                {
                    level--;
                    if(level < 0) level = 0;
                }

			}
            else if(x > 28 && x < 108)  // Bar graph area of touchscreen?
            {
                // The bar-graph bars are spaced 5px apart, and start
                // somewhere around px 30. Figure out the level based on that.
                level = ((x-30)/5) & 0x0f;
            }
            else if(x > 110)  // "Top" button (top portion of touchscreen)?
            {
				break;       // Return to main menu
            }
            else
            {
                // No button defined for this x,y location
            }

            // If this was a change of level, update the bargraph,
            // and the item that we are controlling (PWM, contrast, etc).
            //if(level != savedLevel[demoNumber])
            {
                updateBarGraph(level);        // Show new level on bar graph
                switch(demoNumber)
                {
                    case DEMO_LIGHT:          // Update PWM level
                        SetDCOC2PWM(pwmTableLED[level]);
                        break;
                    case DEMO_FAN:            // Update PWM level
                        SetDCOC3PWM(pwmTableFan[level]);
                        break;
                    case DEMO_HEAT:
                        break;
                    case DEMO_CONTRAST:
                        // The LCD Controller IC's "volume" level is one of the
                        // controls for the LCD's drive level. We init the value
                        // to 35 (its valid range is 0..63). For our demo, map
                        // our bar-graph level 0..15 to a range around our
                        // default "volume", 28..42.
                        lcdCmd(cVOLUME);  // LCD will expect volume level next
                        lcdCmd(level + 28);
                        break;
                }
            }
		}
		delay_ms(65);	
	}

    // Save this level for this controlled item
    savedLevel[demoNumber] = level;
}
