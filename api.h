/*******************************************************************************
 *
 * Definitions for using Yoctopuce driver for Pervasive Display ePaper panels.
 *
 * License:
 * 
 * Creative Commons Attribution-ShareAlike 4.0 International (CC BY-SA 4.0)
 * Copyright (c) Yoctopuce Sarl, 2026
 * 
 *****************************************************************************/

#ifndef API_H
#define API_H

typedef unsigned char           u8;
typedef signed   char           s8;
typedef unsigned short int      u16;
typedef signed   short int      s16;
typedef unsigned long int       u32;
typedef signed   long int       s32;

// generic helpers (replace with your own if you have better ones)
#define memset(buf,val,sz)      do{u8*p=(u8*)buf;u16 n=sz;while(n--)*p++=val;}while(0)
#define NULL                    ((void *)0)

// Project-specific timing helpers
extern volatile u16 msCounter;
#define ytime16() msCounter
void    delay_us(u8 microseconds);

// Project-specific debugging helpers (logging+)
#define ylog(str)                   // Put your own nul-terminated string log function (eg. to UART)
#define yloglen(str,len)            // Put your own fixed-size string log function (eg. to UART)
#define ylogChar(ch)                // Put your character log function (eg. to UART)
#define ylogU8(byte)                // Put your own hex byte log function (eg. to UART)
#define ylogU16(byte)               // Put your own hex word log function (eg. to UART)
#define ylogNum(val)                // Put your own decimal log function (eg. to UART)

// Individual pin mappings
#include "p24FJ256DA206.h"
#define YOCTO_LED_OUT           (_LATD9)
#define YOCTO_LED_TRIS          (_TRISD9)
#define YOCTO_LED_ODC           (_ODD9)
#define YOCTO_LED_RPOR          (_RP4R)
#define YOCTO_LED_PIN           (4ul)
#define YBUTTON_IN              ((_RD0)^1)
#define YBUTTON_RAW             (_RD0)
#define YBUTTON_TRIS            (_TRISD0)
#define YBUTTON_ODC             (_ODD0)
#define YBUTTON_IE              (_CN49IE)
#define YBUTTON_PUE             (_CN49PUE)
#define YBUTTON_PDE             (_CN49PDE)
#define YBUTTON_PIN             (11ul)
#define PANEL_CS_OUT            (_LATG9)
#define PANEL_CS_TRIS           (_TRISG9)
#define PANEL_CS_ODC            (_ODG9)
#define PANEL_CS_RPOR           (_RP27R)
#define SDO1_IN                 (_RG6)
#define SDO1_RAW                (_RG6)
#define SDO1_TRIS               (_TRISG6)
#define SDO1_ODC                (_ODG6)
#define SDO1_ANS                (_ANSG6)
#define SDO1_ANIDX              (17)
#define SDO1_IE                 (_CN8IE)
#define SDO1_PUE                (_CN8PUE)
#define SDO1_PDE                (_CN8PDE)
#define SDO1_PIN                (21ul)
#define SDI1_OUT                (_LATG7)
#define SDI1_TRIS               (_TRISG7)
#define SDI1_ODC                (_ODG7)
#define SDI1_RPOR               (_RP26R)
#define SCK1_OUT                (_LATG8)
#define SCK1_TRIS               (_TRISG8)
#define SCK1_ODC                (_ODG8)
#define SCK1_RPOR               (_RP19R)
#define PANEL_SCK_OUT           (_LATG8)
#define PANEL_SCK_TRIS          (_TRISG8)
#define PANEL_SCK_ODC           (_ODG8)
#define PANEL_SCK_RPOR          (_RP19R)
#define PANEL_DATA_OUT          (_LATG6)
#define PANEL_DATA_IN           (_RG6)
#define PANEL_DATA_TRIS         (_TRISG6)
#define PANEL_DATA_ODC          (_ODG6)
#define PANEL_DATA_IE           (_CN8IE)
#define PANEL_DATA_PUE          (_CN8PUE)
#define PANEL_DATA_PDE          (_CN8PDE)
#define PANEL_DATA_RPOR         (_RP21R)
#define PANEL_DATA_PIN          (21ul)
#define PANELRESET_OUT          (_LATE1)
#define PANELRESET_TRIS         (_TRISE1)
#define PANELRESET_ODC          (_ODE1)
#define PANELBUSY_IN            (_RE2)
#define PANELBUSY_RAW           (_RE2)
#define PANELBUSY_TRIS          (_TRISE2)
#define PANELBUSY_ODC           (_ODE2)
#define PANELBUSY_IE            (_CN60IE)
#define PANELBUSY_PUE           (_CN60PUE)
#define PANELBUSY_PDE           (_CN60PDE)
#define PANELDC_OUT             (_LATE3)
#define PANELDC_TRIS            (_TRISE3)
#define PANELDC_ODC             (_ODE3)
#define PANELPOWER_OUT          (_LATE4)
#define PANELPOWER_TRIS         (_TRISE4)
#define PANELPOWER_ODC          (_ODE4)

// Peripheral mappings
#define PANEL_STAT              (SPI1STAT)
#define PANEL_STATbits          (SPI1STATbits)
#define PANEL_CON1              (SPI1CON1)
#define PANEL_CON1bits          (SPI1CON1bits)
#define PANEL_CON2              (SPI1CON2)
#define PANEL_CON2bits          (SPI1CON2bits)
#define PANEL_BUF               (SPI1BUF)
#define PANEL_IE                (_SPI1IE)
#define PANEL_IF                (_SPI1IF)
#define PANEL_IP                (_SPI1IP)
#define PANEL_SCK_FUNC          (8ul)
#define PANEL_SDI_TRIS          (SDI1_TRIS)
#define PANEL_SDI_ODC           (SDI1_ODC)
#define PANEL_SDI_IN            (SDI1_IN)
#define PANEL_SDI_RPINR         (_SDI1R)
#define PANEL_SDO_TRIS          (SDO1_TRIS)
#define PANEL_SDO_ODC           (SDO1_ODC)
#define PANEL_SDO_OUT           (SDO1_OUT)
#define PANEL_SDO_FUNC          (7ul)
#define PANEL_SS_TRIS           (SS1_TRIS)
#define PANEL_SS_ODC            (SS1_ODC)
#define PANEL_SS_OUT            (SS1_OUT)
#define PANEL_SS_FUNC           (9ul)
#define PANEL_Interrupt         (_SPI1Interrupt)

#define TRIS_OUTPUT             0
#define TRIS_INPUT              1

#define _FAST_ISR_NO_PSV        void __attribute__((interrupt, shadow, __no_auto_psv__))

// =======================================================================
//   Linkage to our PervasiveDisplays driver
// =======================================================================

#define PANEL_TYPE_NONE 0               // No panel configured
#define PANEL_TYPE_E2   2               // Pervasive Display ePaper panel

typedef struct {  
    u8      type;                       // a value from PANEL_TYPE_*
    u8      film;                       // type-specific color/film identifier
    u16     size;                       // panel size, as represented in the panel model name
    char    cog[2];                     // chip-on-glass/controller identifier
    u16     hwrev;                      // panel revision number
} yDisplayPanel;

typedef struct {
    u16     width;                      // pixel width (number of pixel columns)
    u16     height;                     // pixel height (always a multiple of 8)
    u16     bpp;                        // number of bits per pixel
    u16     pixbytes;                   // number of bytes to store the panel image
    u16     canfast;                    // display driver has support for fast refresh
    u16     work_tempMin;               // minimum working temperature
    u16     work_tempMax;               // maximum working temperature
    u16     fast_tempMin;               // minimum temperature to use fastUpdate
    u16     fast_tempMax;               // maximum temperature to use fastUpdate    
} EPprops;

extern EPprops epaper_disp;
extern const char *epaper_errmsg;

#define MAX_PIXEL_BYTES         16320
extern u8 mainbuffer[MAX_PIXEL_BYTES];

#define EPAPER_FULL_UPDATE      0x01
#define EPAPER_FAST_UPDATE      0x02

#define COL_WHITE               0
#define COL_BLACK               1
#define COL_RED                 2
#define COL_YELLOW              3

const char *display_setPanel(yDisplayPanel *panel); // select specific panel
int  display_init_controler(void);      // power-on and start controller init sequence
void display_forceFullUpdate(u8 full);  // setup next update type
int  display_prepShowBuffer(void);      // power-up DC/DC, resend old image if needed
int  display_doShowBuffer(void);        // complete panel update, shut down DC/DC
int  display_update_in_progress(void);  // return EPAPER_*_UPDATE iff waiting for panel update
void display_poweroff_controler(void);  // power-off controller

#endif
