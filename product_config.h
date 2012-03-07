#ifndef _PRODUCT_CONFIG_H_
#define _PRODUCT_CONFIG_H_

// product_config.h
//
// Items that configure options for a particular product, build-type,
// processor, etc.


// CPU clock speed
#define CPU_HZ 80000000L


// Uncomment one..
//#define LCD_SERIAL
#define LCD_PARALLEL

// Uncomment one...
//#define ST7565_NHD_PROTOTYPE_STARTERKIT
//#define ST7565_M4492_PROTOTYPE_OLIMEX_UEXTPORT
//#define ST7565_M4557_PROTOTYPE_STARTERKIT
#define M4557_DUINOMITE

// Define C++/C99 style bool type, with values true and false.
// Note: Stay away from uppercase TRUE & FALSE; uChip defines
//       those somewhere in USB code, I think.
// TODO: Check if this resolves to int or char type - if int, change to char.
typedef enum { false=0, true=1 } bool;

#endif
