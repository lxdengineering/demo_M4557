//
// main_p32.c
//
// ESI M4557  LCD with touchscreen demo.
//
// 2011, LXD Research & Display
//
// Platforms:
//   - Microchip PIC32 USB Starter Kit II (with Starter Kit I/O Expansion Board)
//   - Olimex PIC32-MX460 (pic32mx460F512L)
//   - Olimex Duinomite (pic32mx795F512H)


#include <plib.h>       // PIC32 Peripheral library
#include <stdint.h>
#include <string.h>

#include "product_config.h"
#include "st7565.h"
#include "gfx.h"
#include "p32_utils.h"  // Our misc utils for pic32 (delays, etc)
#include "tsc2046.h"
#include "M4557demo.h"


// Set Configuration Bits in code
#pragma config FNOSC    = PRIPLL        // Oscillator Selection
#pragma config FPLLIDIV = DIV_2         // PLL Input Divider (PIC32 Starter Kit: use DIV_2 only!)
#pragma config FPLLMUL  = MUL_20        // PLL Multiplier
#pragma config FPLLODIV = DIV_1         // PLL Output Divider
#pragma config FPBDIV   = DIV_2         // Peripheral Clock divisor
#pragma config FWDTEN   = OFF           // Watchdog Timer 
#pragma config WDTPS    = PS1           // Watchdog Timer Postscale
#pragma config FCKSM    = CSDCMD        // Clock Switching & Fail Safe Clock Monitor
#pragma config OSCIOFNC = OFF           // CLKO Enable
#pragma config POSCMOD  = XT            // Primary Oscillator
#pragma config IESO     = OFF           // Internal/External Switch-over
#pragma config FSOSCEN  = OFF           // Secondary Oscillator Enable
#pragma config CP       = OFF           // Code Protect
#pragma config BWP      = OFF           // Boot Flash Write Protect
#pragma config PWP      = OFF           // Program Flash Write Protect
#if defined M4557_DUINOMITE
  #pragma config ICESEL   = ICS_PGx1    // ICE/ICD Comm Channel Select
#else
  #pragma config ICESEL   = ICS_PGx2    // ICE/ICD Comm Channel Select
#endif
#pragma config DEBUG    = OFF           // Debugger Disabled for Starter Kit
// Note on DEBUG... To get PIC32 Starter Kit to run stand-alone (it's not obvious
// how), DEBUG will have to be OFF.  Since in many examples, DEBUG is already
// OFF, and the debugger seems to work OK, I don't know why you would want
// to set it on.  From Microchip PIC32 forum - how to get the SK working
// stand-alone...
/*
Use case:
1. Write program
    Compile and link as "Release"
    Connect USB SK
    Select debugger to be PIC32 Starter Kit
2. Program the chip (click away the messages about debugger programming release build)
3. Select debugger to be "None"
5. Program executes. Whether MPLAB is running or not. 

       ... and, make sure #pragma DEBUG is OFF
 */
//            


// Global variables -------------------------------------------------
//
//

#if defined ST7565_M4557_PROTOTYPE_STARTERKIT
  // From Microchip example, port_io.c
  unsigned int dummy;
#elif defined M4557_DUINOMITE
  
#else
  #error Need a product type definedf
#endif


// Adjust st7565 resistor ratio and "volume"
void contrastSetup()
{
    char s[256];
    uint8_t tmp8;

    DBPUTS("Enter resistor ratio (0..7)...");
    DBGETS(s, sizeof(s));
    
    tmp8 = s[0] - '0';   //
    if(tmp8 < 8)
    {
        lcdCmd(cRESISTOR_RATIO | tmp8);
        DBPRINTF("Set to %d.\n", tmp8);
    }
    else
    {
        DBPUTS("Invalid range.\n");
    }

    DBPUTS("Enter volume (0..63)...");
    DBGETS(s, sizeof(s));
    
    tmp8 = atoi(s);   //
    if(tmp8 < 64)
    {
        lcdCmd(cVOLUME);
        lcdCmd(tmp8);
        DBPRINTF("Set to %d.\n", tmp8);
    }
    else
    {
        DBPUTS("Invalid range.\n");
    }

    // Clear the do-user-input flag
    //DoUserUserInputSemaphore = 0;
}


// main() ---------------------------------------------------------------------
//
int main(void)
{
    int pbClk;         // Peripheral bus clock

 	// Configure the device for maximum performance, but do not change
	// the PBDIV clock divisor.  Given the options, this function will
	// change the program Flash wait states, RAM wait state and enable
	// prefetch cache, but will not change the PBDIV.  The PBDIV value
	// is already set via the pragma FPBDIV option above.
   	pbClk = SYSTEMConfig(CPU_HZ, SYS_CFG_WAIT_STATES | SYS_CFG_PCACHE);

    // The pic32 has 2 programming i/f's: ICD/ICSP and JTAG. The
    // starter kit uses JTAG, whose pins are muxed w/ RA0,1,4 & 5.
    // If we wanted to disable JTAG to use those pins, we'd need:
#if defined M4557_DUINOMITE
    DDPCONbits.JTAGEN = 0;  // Disable the JTAG port.
#endif

    // Pins that share ANx functions (analog inputs) will default to
    // analog mode (AD1PCFG = 0x0000) on reset.  To enable digital I/O
    // Set all ones in the ADC Port Config register. (I think it's always
    // PORTB that is shared with the ADC inputs). PORT B is NOT 5v tolerant!
    // Also, RB6 & 7 are the ICD/ICSP lines.
    AD1PCFG = 0xffff;  // Port B as digital i/o.

 	//Initialize the DB_UTILS IO channel
	DBINIT();
	
#if defined ST7565_M4557_PROTOTYPE_STARTERKIT
    // Enable pullup resistors for Starter Kit's 3 switches.
    //CNPUE = (CN15_PULLUP_ENABLE | CN16_PULLUP_ENABLE | CN19_PULLUP_ENABLE);
    mCNOpen(CN_ON, CN15_ENABLE | CN16_ENABLE,
            CN15_PULLUP_ENABLE | CN16_PULLUP_ENABLE | CN19_PULLUP_ENABLE);
    // Read the port to clear any mismatch on change notice pins
    dummy = mPORTDRead();
    // Clear change notice interrupt flag
    ConfigIntCN(CHANGE_INT_ON | CHANGE_INT_PRI_2);
    // Ok now to enable multi-vector interrupts
    INTEnableSystemMultiVectoredInt();

    // Display the introduction
    DBPUTS("SW1:Set contrast; SW2:Halt display\n");
    //DBPRINTF("DBPRINTF: The build date and time is (" __DATE__ "," __TIME__ ")\n");

    // Init ports for ST7565 parallel prototype setup (using pic32 starter kit
    // and i/o expansion board): TODO: This belongs somewhere else!!
    //
    //    PIC32       NHD Display
    //    RB10        /CS1, /RES, A0, /WR, /RD
    //    RE0..7      D0..7
    LATBSET  = 0x7C00;   // Set the control bits high (inactive)
    TRISBCLR = 0x7C00;   // Set the control bits as outputs.

    // Init ESI touch-screen controller's (TSC2046) CS line as output
    LATFSET  = BIT_5;     // Set CSn inactive...
    TRISFCLR = BIT_5;     //    and set as output

    // Data is on port E.  The LCD controller (ST7565) uses its parallel
    // mode on port E, RE0..7. The touch screen controller shares port
    // shares a serial interface on some of these bits.
    TRISECLR = 0x00ff;   // Set the cmd/data bits as outputs.

#elif defined M4557_DUINOMITE

    // Init ports for the M4557.
    //
    // Control bits are on port B:
    //
    //    bit    M4557 function
    //    ----   --------------
    //      3     /CS1   - LCD Chip sel (ST7565 controller)
    //      4     /RES   - Reset
    //      6     A0     - 
    //      7     /WR    - Write
    //      9     /RD    - Read
    //      10    /TPCS  - Touch panel Chip Sel (TSC2046)
    //
    // Set the control bits low, and enable as outputs.
    LATBSET  = (BIT_3|BIT_4|BIT_6|BIT_7|BIT_9|BIT_10);
    TRISBCLR = (BIT_3|BIT_4|BIT_6|BIT_7|BIT_9|BIT_10);

    // Data is on port E.  The LCD controller (ST7565) uses its parallel
    // mode on port E, RE0..7. The touch screen controller shares port
    // shares a serial interface on some of these bits.
    TRISECLR = 0x00ff;   // Set the cmd/data bits as outputs.

#else
    #error Need product defined
#endif

    Nop();
    lcdInit(5,35);        // Init lcd controller
	Nop();

    // Output Compare (PWM) pins
    //
    //   OCn  64pin  100pin/port  SKII-J11-       Duinomite
    //   ---  -----  -----------  --------------  ----------
    //   OC1   46     72/RD0       19 (LED1,Red)   SOUND
    //   OC2   49     76/RD1       20 (LED2,Yel)   D13,SD_CLK
    //   OC3   50     77/RD2       17 (LED3,Grn)   D12,SD_MISO
    //   OC4   51     78/RD3       18              D11,SD_MOSI
    //   OC5   52     81           15              vga_hsync
    //

	// Init Timer 2 for use by the OC (PWM) module(s).
    // This will set the PWM frequency, f = pbClk/reloadValue.
    // Examples (pcClk = 40MHz):  
    //     f = pbClk/100000 = 400Hz
    //     f = pbClk/10000  = 4kHz
    //     f = pbClk/2500 = 20kHz
    // 
    //OpenTimer2(T2_ON | T2_32BIT_MODE_ON, 100000);  // f = pbClk/100000 = 400Hz
    //OpenTimer2(T2_ON | T2_32BIT_MODE_ON, 10000);  // f = pbClk/10000 = 4kHz
    OpenTimer2(T2_ON | T2_32BIT_MODE_ON, 2500);  // f = pbClk/2500 = 16kHz


	// I tried the uChip example using "OpenOC2", and as is ofter the
	// case, I can't get it to work, the documentation is lacking, and
	// it's easier to just read the datasheet and program the bloody
	// OCxCON register directly.
    //OpenOC2( OC_ON | OC_TIMER_MODE32 | OC_TIMER2_SRC | OC_CONTINUE_PULSE | OC_LOW_HIGH , 40000, 30000 );
    SetDCOC2PWM(0);
    OC2CON = 0x8026;  // OC on; 32bit-mode; PWM mode.

    SetDCOC3PWM(0);
    OC3CON = 0x8026;  // OC on; 32bit-mode; PWM mode.

/*
    int testOnly = 1;
    uint32_t loadVal = 0;
	int i;
    while(testOnly)
    {
		// Try the dimmer's 16 levels (0..15)
		for(i=0; i<16; i++)
		{
			loadVal = pwmTable1[i];
        	SetDCOC2PWM(loadVal);
        	delay_ms(300);
		}
    }
    CloseOC2();
*/

    // Do ESI M4557 demo application (dimmer-control type demo w/ touchscreen)
    esi_M4557();
    
    return 0;
}


/******************************************************************************
*
*       Change Notice Interrupt Service Routine
*
*   (MMc: From .\PIC32 Starter Kits\Port_IO\port_io.c sample code)
*   Note: Switch debouncing is not performed.
*   Code comes here if SW2 (CN16) PORTD.RD7 is pressed or released.
*   The user must read the IOPORT to clear the IO pin change notice mismatch
*       condition first, then clear the change notice interrupt flag.
******************************************************************************/
#if defined ST7565_M4557_PROTOTYPE_STARTERKIT
void __ISR(_CHANGE_NOTICE_VECTOR, ipl2) ChangeNotice_Handler(void)
{
    // Step #1 - always clear the mismatch condition first, by reading the port (or just the bit)
    dummy = PORTD;

    // Step #2 - then clear the interrupt flag
    mCNClearIntFlag();

    // Step #3 - process the switches
    /*
    if(dummy == BIT_7)
    {
        PORTClearBits(IOPORT_D, BIT_1);       // turn off LED2
        DBPRINTF("Switch SW2 has been released. \n");
    }
    else
    {
        PORTSetBits(IOPORT_D, BIT_1);     // turn on LED2
        DBPRINTF("Switch SW2 has been pressed. \n");
    }
    */

    if((dummy & BIT_6) == 0)    // SW1 pressed?
    {
        contrastSetup();
    }


    if((dummy & BIT_7) == 0)   // SW2 pressed?
    {
        //waitForUser();
        haltDisplay ^= 1;
        delay_ms(20);
    }
    // additional processing here...

 }
#endif


/******************************************************************************
*	InitLEDs()
*
*	This function uses the PIC32 Peripheral Library macros to configure the
*   IO port to drive the leds.
******************************************************************************/ 
#if defined ST7565_M4557_PROTOTYPE_STARTERKIT
void InitLEDs(void)
{
  	// Config PORTD pins RD0, RD1 and RD2 as outputs, and clear
    // (MMc: These macros from "ports.h" are kinda useless, imho)
    //
	mPORTDSetPinsDigitalOut(BIT_0 | BIT_1 | BIT_2); // Same as TRISDCLR = 0x0007
	mPORTDClearBits(BIT_0 | BIT_1 | BIT_2);         // Same as LATDCLR = 0x0007
}
#endif

