/*******************************************************************************
 *
 * $Id: ydisplay_PervasiveDisplay.c $
 *
 * Hardware support for Pervasive Displays screens
 *
 * Based on the following libraries from Pervasive Displays Inc.:
 *
 * Pervasive Displays EPD hardware drivers (release 10.0)
 *      https://github.com/PervasiveDisplays/Pervasive_Wide_Small (Film K)
 *      https://github.com/PervasiveDisplays/Pervasive_BWRY_Small (Film Q)
 *      Copyright (c) Pervasive Displays, 2010-2025
 *      Portions (c) Rei Vilo, 2010-2025
 *      Based on highView technology
 *      For exclusive use with Pervasive Displays screens
 *      License: Attribution-ShareAlike 4.0 International (CC BY-SA 4.0)
 *  
 * Pervasive Displays Library Suite - Basic edition - Fast Update (release 8.2)
 *      https://github.com/PervasiveDisplays/PDLS_EXT3_Basic_Fast (Film P)
 *      Copyright (c) Rei Vilo, 2010-2025
 *      Portions (c) Pervasive Displays, 2010-2025
 *      Based on highView technology
 *      For exclusive use with Pervasive Displays screens
 *      License: Attribution-ShareAlike 4.0 International (CC BY-SA 4.0)
 * 
 * Pervasive Displays Library Suite - Basic edition - Global Update (release 8.2)
 *      https://github.com/PervasiveDisplays/PDLS_EXT3_Basic_Global (Film C,J)
 *      Copyright (c) Rei Vilo, 2010-2025
 *      Portions (c) Pervasive Displays, 2010-2025
 *      Based on highView technology
 *      For exclusive use with Pervasive Displays screens
 *      License: Attribution-ShareAlike 4.0 International (CC BY-SA 4.0)
 * 
 * This file is a derivative work is therefore published as well under
 * Creative Commons Attribution-ShareAlike 4.0 International (CC BY-SA 4.0)
 * 
 * Specific features of this Yoctopuce driver for Pervasive Displays EPD:
 *      - a single driver covering all "Small" displays (incl. C, J, P, K, Q films)
 *      - recoded using only vanilla C functions (in a single .c file + header file)
 *      - adapt low-level I/O to PIC24F peripherals (incl. IRQ processing)
 *      - change blocking functions to pseudo-asynchronous functions (state machines)
 *      - support for both global update and fast update with the same entry point
 *      - same 2bpp framebuffer format for Film J and Film Q
 * These enhancementsd are Copyright (c) Yoctopuce Sarl, 2026
 * License: Attribution-ShareAlike 4.0 International (CC BY-SA 4.0)
 *
 * This driver is included "as is" in Yocto-Display-ePaper-C firmware.
 * 
 * If you reuse part of this work, you must give appropriate credit.
 * 
 *****************************************************************************/

#include "api.h"

// State machines for asynchronous operation
enum COGR_States { 
    COGR_START = 0,
    COGR_STEP1,
    COGR_STEP2,
    COGR_STEP3,
    COGR_STEP4,
    COGR_STEP5,
    COGR_COMPLETED
};

enum COGOTP_states {
    COGOTP_START = 0,
    COGOTP_BWSTEP1,
    COGOTP_BWSTEP2,
    COGOTP_BWSTEP3,
    COGOTP_BWSTEP4,
    COGOTP_QSTEP1,
    COGOTP_QSTEP2,
    COGOTP_QSTEP3_DRVF,
    COGOTP_QSTEP4_DRVF,
    COGOTP_QSTEP3_DRV6,
    COGOTP_QSTEP4_DRV6,
    COGOTP_QSTEP5_DRV6,
    COGOTP_QSTEP6_DRV6,
    COGOTP_QSTEP7,
    COGOTP_COMPLETED
};

enum ACGINIT_states { 
    ACGINIT_START = 0,
    ACGINIT_BWSTEP1_DRVJ,
    ACGINIT_BWSTEP2_DRVJ,
    ACGINIT_BWSTEP1_OTHER,
    ACGINIT_BWSTEP2_OTHER,
    ACGINIT_QSTEP1_DRVF,
    ACGINIT_QSTEP2_DRVF,
    ACGINIT_QSTEP3_DRVF,
    ACGINIT_QSTEP1_DRV6,
    ACGINIT_QSTEP2_DRV6,
    ACGINIT_QSTEP3_DRV6,
    ACGINIT_QSTEP4_DRV6,
    ACGINIT_QSTEP5_DRV6,
    ACGINIT_COMPLETED
};

enum COGUPDT_states { 
    COGUPDT_START = 0,
    COGUPDT_STEP1_DRVJ,
    COGUPDT_STEP2_DRVJ,
    COGUPDT_STEP1_DRVC,
    COGUPDT_STEP2_DRVC,
    COGUPDT_STEP1_OTHER,
    COGUPDT_STEP2_OTHER,
    COGUPDT_STEP3_OTHER,
    COGUPDT_STEP4_OTHER,
    COGUPDT_COMPLETED
};

enum COGPWROFF_States {
    COGPWROFF_START = 0,
    COGPWROFF_DEFAULTDISPLAY,
    COGPWROFF_WAITBSY,
    COGPWROFF_WAITMORE,
    COGPWROFF_COMPLETED
};

enum INITCTRL_States {
    INITCTRL_START = 0, 
    INITCTRL_STEP1, 
    INITCTRL_STEP2, 
    INITCTRL_STEP3
};

enum PUPD_States {
    PUPD_START = 0, 
    PUPD_STEP1, 
    PUPD_STEP2, 
    PUPD_STEP3, 
    PUPD_STEP4, 
    PUPD_STEP5, 
    PUPD_STEP6,
    PUPD_DONE
};

enum DUPD_States {
    DUPD_START = 0, 
    DUPD_STEP1_fast, 
    DUPD_STEP1_normal, 
    DUPD_STEP2, 
    DUPD_STEP3, 
    DUPD_STEP4
};

// State variables to be cleared back to zero
// - on startup
// - when trying a new panel model
// - after a fatal failure, or communication timeout
struct {
    u8      otpChk;
    u8      OTP_done;
    u8      COG_data[48];               // OTP
    u8      flag50;                     // Register 0x50
    u16     offsetA5;
    u16     offsetPSR;
    u16     chipId;
    u8      readBytes;
    u8      bank;
    u8      optTemperature;
    u8      dataToSend[2];              // for sending PSR values
    u8      displayFailure;
    u8      fastUpdatePossible;
    u8      updateInProgress;

    // asynchronous function states
    u16     timeout;
    const   u8 *dataPtr;
    const   u8 *halfPtr;
    u16     dataBlk;
    u16     dump_SPI_index;
    u8      dump_SPI_STATE;
    u8      COGR_State;
    u8      COGOTP_state;
    u8      ACGINIT_state;
    u8      COGUPDT_state;
    u8      COGPWROFF_State;
    u8      INITCTRL_state;
    u8      PUPD_state;
    u8      DUPD_state;
} EPD;

typedef union {
    u32         asDword;
    struct {
        u16     size;               // panel size, in inches, as in panel model name
        char    film;               // panel film type
        char    driver;             // panel driver   
    };
} E2panel;

// environmental parameters, to be set externally
s8      epaper_temperature = 25;

// active screen properties, set by display_setPanel()
E2panel epaper_panel = { 0 };       // currently selected display panel
EPprops epaper_disp = { 0 };        // properties for currently selected panel

#define b_delayCS       50  /* [us] */

#if 0
#define FILM_E          'E' // Old BWR film, unavailable, no spec, no sample code
#define FILM_F          'F' // Old BWR film, unavailable, no spec, no sample code
#define FILM_G          'G' // Old BWY film, unavailable, no spec, no sample code
#define FILM_H          'H' // Old Freeze film, unavailable, no spec, no sample code
#endif
#define FILM_C          'C' // Standard                     (PDLS_EXT3_Basic_Global rel.8.2)
#define FILM_J          'J' // BWR, "Spectra"               (PDLS_EXT3_Basic_Global rel.8.2)
#define FILM_K          'K' // Embed.fast upd + Wide temp.  (PDLS_Basic rel.10.0)
#define FILM_P          'P' // Embed.fast upd               (PDLS_EXT3_Basic_Fast rel.8.2)
#define FILM_Q          'Q' // BWRY, "Spectra 4"            (PDLS_Basic rel.10.0)

#define DRIVER_6        '6' // used with film C, K and Q (chipId 0xc901)
#define DRIVER_9        '9' // used with film C and P
#define DRIVER_C        'C' // used with film C, J, K and P
#define DRIVER_D        'D' // used with film J, K and P
#define DRIVER_E        'E' // used with film K and P
#define DRIVER_F        'F' // used with Film K and Q (F1=chipId 0x0302/F9=chipId 0x8302/F2=chipId 0x8502)
#define DRIVER_J        'J' // used with film K

// Known and tested models (unless marked otherwise)
// E2_152_KS_0J1
// E2_152_QS_061 (sometimes referred as E2_152_QS_H61)
// E2_154_KS_0C2
// E2_154_CS_0C1
// E2_154_QS_0F1
// E2_206_KS_0E1
// E2_206_QS_061
// E2_213_PS_0E1
// E2_213_KS_0E1
// E2_266_JS_0C1
// E2_266_KS_0C3
// E2_266_QS_0F1
// E2_266_QS_0F9
// E2_271_JS_0C1
// E2_271_KS_0C1
// E2_287_PS_091
// E2_290_KS_0F1
// E2_290_QS_0F2 (sometimes referred as E2_290_QS_DF2)
// E2_370_JS_0C1 (requires large buffer; pretend to be E2_370_CS_0C1 for BW usage)
// E2_370_KS_0C1
// E2_417_KS_0D1
// E2_437_KS_0C1
//
// Unsupported "small" panels:
// E2_340_KS_0G1 Resolution 456x392 would need more memory and DRIVER_G support

#define AWAIT(expr)     {   int res = expr;                     \
                            if(res != 0) return res;            \
                        }

static int aSync_delay(int ms)
{
    int remains;
    
    if (!EPD.timeout) {
        EPD.timeout = ytime16() + ms;
        if(!EPD.timeout) EPD.timeout++;
    }
    remains = EPD.timeout - ytime16();
    if(remains > 0) {
        // wait more
        return remains;
    }
    EPD.timeout = 0;
    return 0; 
}

#define HIGH    1
#define LOW     0

static int aSync_waitBusy(u8 desiredState, u16 msTimeout)
{ 
    int remains;

    if (!EPD.timeout) {
        EPD.timeout = ytime16() + msTimeout;
        if(!EPD.timeout) EPD.timeout++;
    }
    if(desiredState == HIGH) {
        if(PANELBUSY_IN) {
            EPD.timeout = 0;    // condition met
            return 0;
        }
    } else {
        if(!PANELBUSY_IN) {
            EPD.timeout = 0;    // condition met
            return 0;
        }
    }
    remains = EPD.timeout - ytime16();
    if(remains <= 0) {
        // timeout occured
        ylog("BSY ");
        ylogNum(msTimeout);
        ylog(" timeout\n");
        EPD.timeout = 0;
        return 0;
    }
    // wait more
    return 1; 
}

// Initial setup function to map SPI peripheral to pin
static void spiPortSetup(void)
{
    __builtin_write_OSCCONL(OSCCON & 0xBF);  // Unlock PPS
    PANEL_SCK_RPOR = PANEL_SCK_FUNC;
    PANEL_DATA_RPOR = PANEL_SDO_FUNC;
    __builtin_write_OSCCONL(OSCCON | 0x40);  // Lock PPS
}

#define SPIMODE_3WIRE_BIDI  0
#define SPIMODE_SEND_FAST   1

static void spiResume(u8 spiMode)
{
    PANELDC_OUT    = 1;
    PANELRESET_OUT = 1;
    PANEL_CS_OUT   = 1;
    PANEL_SCK_OUT  = 0;
    if(spiMode) {
        if (!PANEL_STATbits.SPIEN) {
            PANEL_IE = 0;
            PANEL_IP = 5;              // Fast ISR
            PANEL_STAT = 0x0014;       // SISEL=5 (Interrupt flag when transmit/receive is complete)
            PANEL_CON1 = 0x013b;       // MODE16=0,SMP=0,CKE=1,CKP=0,MSTEN=1,1:2 (Master mode 0, 8Mhz)
            PANEL_CON2 = 0x0001;       // Enhanced mode, not framed
            PANEL_STATbits.SPIEN = 1;
            PANEL_DATA_TRIS = TRIS_OUTPUT;
        }
    } else {
        if (PANEL_STATbits.SPIEN) {
            PANEL_STATbits.SPIEN = 0;
            PANEL_SCK_OUT = 0; 
        }
    }
} 

static void spiRelease(void)
{
    PANEL_STATbits.SPIEN = 0;
    PANELDC_OUT    = 0;
    PANEL_SCK_OUT  = 0; 
    PANEL_DATA_OUT = 0;
}

// 3-wire bidirectional SPI read
//
static u8 SPI3_read(void)
{
    u8 value = 0; 
    u16 i;

    PANEL_DATA_TRIS = TRIS_INPUT; 
    PANEL_CS_OUT = 0;
    delay_us(1);
    for (i = 0; i < 8; ++i) {
        PANEL_SCK_OUT = 1;        
        delay_us(1);
        value |= PANEL_DATA_IN  << (7 - i);     
        PANEL_SCK_OUT = 0; 
        delay_us(1);
    }
    PANEL_CS_OUT = 1;
    delay_us(1);

    return value;
}

// 3-wire bidirectional SPI write
//
static void SPI3_write(u8 value)
{
    u16 i;

    PANEL_DATA_TRIS = TRIS_OUTPUT;
    PANEL_CS_OUT = 0;
    for (i = 0; i < 8; i++) {
        PANEL_DATA_OUT = !!(value & (1 << (7 - i)));
        delay_us(1);
        PANEL_SCK_OUT=1;
        delay_us(1);
        PANEL_SCK_OUT=0;
        delay_us(1);
    }
    PANEL_CS_OUT = 1;
    delay_us(1);
}

// fast SPI send
//
#define DEBUG_SPICMD 0

static void spiSendCmd(u8 cmd)
{
#if DEBUG_SPICMD
    ylogU16(ytime16());
    ylogChar(' ');
    ylogU8(cmd);
    ylogChar('!');
    ylogChar('\n');
#endif    
    PANELDC_OUT = 0;
    PANEL_CS_OUT = 0;
    PANEL_IF = 0;
    PANEL_BUF = cmd;
    while(!PANEL_IF);
    PANEL_CS_OUT = 1;
}

static void spiSendCmdData(u8 cmd, u8 data)
{
#if DEBUG_SPICMD
    ylogU16(ytime16());
    ylogChar(' ');
    ylogU8(cmd);
    ylogChar(':');
    ylogU8(data);
    ylogChar('\n');
#endif    
    PANELDC_OUT = 0;
    PANEL_CS_OUT = 0;
    PANEL_IF = 0;
    PANEL_BUF = cmd;
    while(!PANEL_IF);
    PANELDC_OUT = 1;
    PANEL_IF = 0;
    PANEL_BUF = data;
    while(!PANEL_IF);
    PANEL_CS_OUT = 1;
}

// sendFrame sends a 8-bytes frame pointed by variables below
// (when EPD.dataPtr is NULL, sends a zero frame)
static void _spiSendFrame(void);

// bit filter for Spectra Red
static s16 spiDataBitFilter = -1;
#define PICKBITS(a,b)   (spiDataBitFilter ?                                               \
                         ((a&128?0x80:0) | (a&32?0x40:0) | (a&8?0x20:0) | (a&2?0x10:0)  | \
                          (b&128?0x08:0) | (b&32?0x04:0) | (b&8?0x02:0) | (b&2?0x01:0)) : \
                         ((a& 64?0:0x80) | (a&16?0:0x40) | (a&4?0:0x20) | (a&1?0:0x10)  | \
                          (b& 64?0:0x08) | (b&16?0:0x04) | (b&4?0:0x02) | (b&1?0:0x01)))

static void spiStartSendingDataBlock(u8 index, const u8 *data, u16 size, s16 bitFilter)
{
    u16 i;

#if DEBUG_SPICMD
    ylogU16(ytime16());
    ylogChar(' ');
    ylogU8(index);
    ylogChar(':');
    if(size < 8) {
        for(i = 0; i < size; i++) {
            ylogU8(data[i]);
        }
    } else {
        ylogChar('[');
        ylogNum(size);
        if(bitFilter >= 0) {
            ylogChar(':');
            ylogNum(bitFilter);            
        }
        ylogChar(']');
    }
    ylogChar('\n');
#endif    
    PANELDC_OUT = 0;
    PANEL_CS_OUT = 0;
    delay_us(b_delayCS);        
    // Send command
    PANEL_IF = 0;
    PANEL_BUF = index;
    while(!PANEL_IF);
    (u8)PANEL_BUF;
    delay_us(b_delayCS);
    PANELDC_OUT = 1;
    delay_us(b_delayCS);
    spiDataBitFilter = bitFilter;
    // Send first bytes of data, until remaining size is a multiple of 8 bytes
    if(data) {
        if(spiDataBitFilter < 0) {
            // send all bits as usual
            for(i = size & 0x7; i > 0; i--) {
                PANEL_IF = 0;
                PANEL_BUF = *data++;
                while(!PANEL_IF);
                (u8)PANEL_BUF;
            }        
            EPD.dataPtr = data;
            EPD.halfPtr = NULL;
            EPD.dataBlk = size >> 3;
        } else {
            // send only even/odd bits (split buffer for Spectra Red support)
            for(i = size & 0x15; i > 0; i--) {
                u16 a = *data++;
                u16 b = *data++;
                PANEL_IF = 0;
                PANEL_BUF = (u8)PICKBITS(a,b);
                while(!PANEL_IF);
                (u8)PANEL_BUF;
            }
            EPD.dataPtr = NULL;
            EPD.halfPtr = data;
            EPD.dataBlk = size >> 4;
        }
    } else {
        for(i = size & 7; i > 0; i--) {
            PANEL_IF = 0;
            PANEL_BUF = (u8)0;
            while(!PANEL_IF);
            (u8)PANEL_BUF;
        }
        EPD.dataPtr = NULL;
        EPD.halfPtr = NULL;
        EPD.dataBlk = size >> 3;
    }
    if(EPD.dataBlk) {
        // Send bulk data using ISR
        _spiSendFrame();
        PANEL_IE = 1;        
    }
}

static void _spiSendFrame(void)
{
    if(EPD.dataPtr) {
        // used by all panels except Spectra Red, most efficient
        PANEL_BUF = *EPD.dataPtr++;
        PANEL_BUF = *EPD.dataPtr++;
        PANEL_BUF = *EPD.dataPtr++;
        PANEL_BUF = *EPD.dataPtr++;
        PANEL_BUF = *EPD.dataPtr++;
        PANEL_BUF = *EPD.dataPtr++;
        PANEL_BUF = *EPD.dataPtr++;
        PANEL_IF = 0;
        PANEL_BUF = *EPD.dataPtr++;
    } else if(EPD.halfPtr) {
        // specific code for Spectra Red: less efficient, but provides
        // support for the same buffer encoding the new Spectra 4 panels
        u16 a = *EPD.halfPtr++;
        u16 b = *EPD.halfPtr++;
        PANEL_BUF = (u8)PICKBITS(a,b);
        a = *EPD.halfPtr++;
        b = *EPD.halfPtr++;
        PANEL_BUF = (u8)PICKBITS(a,b);
        a = *EPD.halfPtr++;
        b = *EPD.halfPtr++;
        PANEL_BUF = (u8)PICKBITS(a,b);
        a = *EPD.halfPtr++;
        b = *EPD.halfPtr++;
        PANEL_BUF = (u8)PICKBITS(a,b);
        a = *EPD.halfPtr++;
        b = *EPD.halfPtr++;
        PANEL_BUF = (u8)PICKBITS(a,b);
        a = *EPD.halfPtr++;
        b = *EPD.halfPtr++;
        PANEL_BUF = (u8)PICKBITS(a,b);
        a = *EPD.halfPtr++;
        b = *EPD.halfPtr++;
        PANEL_BUF = (u8)PICKBITS(a,b);
        a = *EPD.halfPtr++;
        b = *EPD.halfPtr++;
        u8 lastbyte = (u8)PICKBITS(a,b);
        PANEL_IF = 0;
        PANEL_BUF = lastbyte;
    } else {
        PANEL_BUF = (u8)0;
        PANEL_BUF = (u8)0;
        PANEL_BUF = (u8)0;
        PANEL_BUF = (u8)0;
        PANEL_BUF = (u8)0;
        PANEL_BUF = (u8)0;
        PANEL_BUF = (u8)0;
        PANEL_IF = 0;
        PANEL_BUF = (u8)0;
    }
}

_FAST_ISR_NO_PSV PANEL_Interrupt(void)
{
    // empty input buffer
    (u8)PANEL_BUF; (u8)PANEL_BUF;
    (u8)PANEL_BUF; (u8)PANEL_BUF;
    (u8)PANEL_BUF; (u8)PANEL_BUF;
    (u8)PANEL_BUF; (u8)PANEL_BUF;
    if(--EPD.dataBlk) {
        _spiSendFrame();
    } else {
        PANEL_IF = 0;
        PANEL_IE = 0;
    }
}

static int aSync_spiWaitUntilDataBlockIsSent(void)
{
    if(EPD.dataBlk) {
        return 1;
    }
    if(PANEL_CON1bits.MODE16) {
        delay_us(b_delayCS);
        PANEL_CS_OUT = 1;
        delay_us(b_delayCS);
    }
    EPD.dataPtr = NULL;
    return 0;
}

static void spiSendSmallBlock(u8 index, const u8 *data, u32 size)
{
    spiStartSendingDataBlock(index, data, size, -1);
    while(aSync_spiWaitUntilDataBlockIsSent()) {
        // idle loop is never reached when size <= 7
        delay_us(50);
    }
}

static int aSync_COG_reset(u8 isPowerUp)
{
    // Application note § 2. Power on COG driver
    switch(EPD.COGR_State) {
    case COGR_START:
        if(isPowerUp) {
            // Wait for power stabilisation (not done for every refresh)
            AWAIT(aSync_delay(20));
        }
        PANELRESET_OUT = 1;
        EPD.COGR_State = COGR_STEP1;
    case COGR_STEP1:  
        AWAIT(aSync_delay(epaper_panel.film == FILM_Q ? 10 : 5));
        PANELRESET_OUT = 0;
        EPD.COGR_State = COGR_STEP2;
    case COGR_STEP2:  
        AWAIT(aSync_delay(epaper_panel.film == FILM_Q ? 20 : (epaper_panel.film == FILM_J ? 10 : 5)));
        PANELRESET_OUT = 1;
        EPD.COGR_State = COGR_STEP3;
    case COGR_STEP3:  
        AWAIT(aSync_delay(epaper_panel.film == FILM_Q ? 40 : 5));
        PANEL_CS_OUT = 1;
        PANEL_SCK_OUT = 0; 
        EPD.COGR_State = COGR_STEP4;
    case COGR_STEP4:
        AWAIT(aSync_delay(epaper_panel.film == FILM_Q ? 10 : 5));
        // Check after reset
        if(epaper_panel.driver == DRIVER_J) { // eg. 150_KS_0J or 152_KS_0J
            if (PANELBUSY_IN) {
                epaper_errmsg = "Incorrect panel type (J)";
                ylog(epaper_errmsg);
                ylogChar('\n');
                EPD.displayFailure = 1;
                return -1;
            }
        }
        if(epaper_panel.film != FILM_Q) {
            EPD.COGR_State = COGR_COMPLETED;
            return 1;
        }
        EPD.COGR_State = COGR_STEP5;
    case COGR_STEP5:
        AWAIT(aSync_waitBusy(HIGH, 1001));
        EPD.COGR_State = COGR_COMPLETED;
    case COGR_COMPLETED:
        break;
    }
    EPD.COGR_State = COGR_START; 
    return 0;  
}

static int aSync_dump_SPI(int start, int end) 
{ 
    if (EPD.dump_SPI_STATE == 0) {
        EPD.dump_SPI_index = start;
        EPD.dump_SPI_STATE = 1;
    }
    int count = 1;
    while (EPD.dump_SPI_index < end) {
        SPI3_read();
        EPD.dump_SPI_index++;
        count++;
        if (count > 5) return 1;
    }  
    EPD.dump_SPI_STATE = 0;  
    return 0;
}

static int aSync_COG_getDataOTP(void)
{
    int index;
    switch(EPD.COGOTP_state) { 
    case COGOTP_START:
        spiResume(SPIMODE_3WIRE_BIDI);
        EPD.flag50 = 0;
        EPD.OTP_done = 0;
        if(epaper_panel.film == FILM_Q) {
            EPD.readBytes = 48;
            EPD.COGOTP_state = COGOTP_QSTEP1;
            return 1;
        } else { // all others (B/W)
            EPD.readBytes = 2;
            switch(epaper_panel.driver) {
            case DRIVER_C:  // 154_KS_0C, 266_KS_0C, 271_KS_0C, 370_KS_0C, 437_KS_0C
            case DRIVER_E:  // 206_KS_0E, 213_KS_0E
                EPD.flag50 = 1;
                break;
            case DRIVER_F:  // 290_KS_0F
            case DRIVER_J:  // 150_KS_0J, 152_KS_0J
                ylog("OTP skipped, embedded PSR\n");
                EPD.readBytes = 0;
                EPD.OTP_done = 1;
                EPD.COGOTP_state = COGOTP_COMPLETED;
                return 1;
            default: 
                break;
            }
            EPD.COGOTP_state = COGOTP_BWSTEP1;
        }        
    case COGOTP_BWSTEP1:   
        AWAIT(aSync_COG_reset(0));
        EPD.offsetA5 = 0x0000;
        EPD.offsetPSR = 0x0000;
        PANELDC_OUT = 0;
        SPI3_write(0xa2);
        EPD.COGOTP_state = COGOTP_BWSTEP2;
    case COGOTP_BWSTEP2:  
        AWAIT(aSync_delay(10));
        PANELDC_OUT = 1;
        SPI3_read(); // Dummy read
        delay_us(1);
        EPD.otpChk = SPI3_read(); // First byte to be checked
#if 0
        ylog("otpChk=");
        ylogU8(EPD.otpChk);
        ylog("\n");
#endif        
        // Check EPD.bank
        EPD.bank = ((EPD.otpChk == 0xa5) ? 0 : 1);
        switch(epaper_panel.driver) {
        case DRIVER_9:  // 271_KS_09, 271_PS_09, 287_PS_09
            EPD.offsetPSR = 0x004b;
            EPD.offsetA5 = 0x0000;
            if (epaper_panel.film != FILM_P && EPD.bank > 0) {
                EPD.COG_data[0] = 0xcf;
                EPD.COG_data[1] = 0x82;
                EPD.COGOTP_state = COGOTP_COMPLETED;
                return 1; 
            }
            break;
        case DRIVER_C:  // 154_KS_0C, 266_KS_0C, 271_KS_0C, 370_KS_0C, 437_KS_0C
                        // 154_PS_0C, 266_PS_0C, 271_PS_0C, 370_PS_0C, 437_PS_0C
            EPD.offsetPSR = (EPD.bank == 0) ? 0x0fb4 : 0x1fb4;
            EPD.offsetA5 = (EPD.bank == 0) ? 0x0000 : 0x1000;
            break;
        case DRIVER_E:  // 206_KS_0, 213_KS_0E, 213_PS_0E
            EPD.offsetPSR = (EPD.bank == 0) ? 0x0b1b : 0x171b;
            EPD.offsetA5 = (EPD.bank == 0) ? 0x0000 : 0x0c00;
            break;
        case DRIVER_D:  // 417_KS_0D, 417_PS_0D
            EPD.offsetPSR = (EPD.bank == 0) ? 0x0b1f : 0x171f;
            EPD.offsetA5 = (EPD.bank == 0) ? 0x0000 : 0x0c00;
            break;
        default:
            epaper_errmsg = "OTP check failed, not supported";
            ylog(epaper_errmsg);
            ylogChar('\n');
            EPD.displayFailure = 1;
            return -1;           
        }
        if (EPD.offsetA5 == 0x0000) {
            EPD.COGOTP_state = COGOTP_BWSTEP4;
            return 1;
        }
        // Check second EPD.bank
        EPD.COGOTP_state = COGOTP_BWSTEP3;
    case COGOTP_BWSTEP3:
        AWAIT(aSync_dump_SPI(1, EPD.offsetA5));
        EPD.otpChk = SPI3_read(); // First byte to be checked
        if (EPD.otpChk != 0xa5) {
            epaper_errmsg = "OTP check failed, A5 expected";
            ylog("OTP check failed - Bank ");
            ylogNum(EPD.bank);
            ylog(", read ");
            ylogU8(EPD.otpChk);
            ylog(" instead of A5\n");
            EPD.displayFailure = 1;
            return -1;
        }
        EPD.COGOTP_state = COGOTP_BWSTEP4;
    case COGOTP_BWSTEP4:
        AWAIT(aSync_dump_SPI(EPD.offsetA5 + 1, EPD.offsetPSR));
        for (index = 0; index < EPD.readBytes; index++) {
            EPD.COG_data[index] = SPI3_read(); // Read OTP
        }
#if 0
        ylog("COG: ");
        for (index = 0; index < EPD.readBytes; index++) {
            ylogU8(EPD.COG_data[index]);
        }
        ylogChar('\n');
#endif
        EPD.OTP_done = 1;
        EPD.COGOTP_state = COGOTP_COMPLETED;
        return 1;
        
    case COGOTP_QSTEP1:
        PANELDC_OUT = 0;
        SPI3_write(0x70);
        EPD.COGOTP_state = COGOTP_QSTEP2;
    case COGOTP_QSTEP2:  
        AWAIT(aSync_delay(8));
        PANELDC_OUT = 1;
        EPD.chipId = (u16)SPI3_read() << 8;
        delay_us(1);
        EPD.chipId |= SPI3_read();
        switch(epaper_panel.driver) {
        case DRIVER_F:  // 154_QS_0F, 213_QS_0F, 266_QS_0F, 290_QS_0F
            if((EPD.chipId & 0x7fff) != 0x302 && EPD.chipId != 0x8502) {
                epaper_errmsg = "Unexpected COG ID";
                ylog(epaper_errmsg);
                ylogChar(' ');
                ylogU16(EPD.chipId);
                ylogChar('\n');
                EPD.displayFailure = 1;
                return -1;
            }
            EPD.COGOTP_state = COGOTP_QSTEP3_DRVF;
            return 1;
        case DRIVER_6:
            if(EPD.chipId == 0xc901) {          // 206_QS_06
                EPD.COGOTP_state = COGOTP_QSTEP3_DRV6;
            } else if(EPD.chipId == 0x4801) {   // 152_QS_06 (alias 152_QS_H6)
                EPD.COGOTP_state = COGOTP_QSTEP3_DRV6;
            } else {
                epaper_errmsg = "Unexpected COG ID";
                ylog(epaper_errmsg);
                ylogChar(' ');
                ylogU16(EPD.chipId);
                ylogChar('\n');
                EPD.displayFailure = 1;
                return -1;
            }
            return 1;
        default:
            epaper_errmsg = "Unsupported driver for film Q";
            ylog(epaper_errmsg);
            ylogChar('\n');
            EPD.displayFailure = 1;
            return -1;           
        }
        
    case COGOTP_QSTEP3_DRVF:
        PANELDC_OUT = 0;
        SPI3_write(0xa4);
        PANELDC_OUT = 1;
        SPI3_write(0x15);
        SPI3_write(0x00);
        SPI3_write(0x01);
        EPD.COGOTP_state = COGOTP_QSTEP4_DRVF;
    case COGOTP_QSTEP4_DRVF:  
        AWAIT(aSync_waitBusy(HIGH, 1002));
        PANELDC_OUT = 0;
        SPI3_write(0xa1);
        PANELDC_OUT = 1;
        SPI3_read(); // Dummy read
        delay_us(1);
        EPD.otpChk = SPI3_read(); // First byte to be checked
        if (EPD.otpChk != 0xa5) {
            epaper_errmsg = "OTP check failed, A5 expected";
            ylog("OTP check failed - read ");
            ylogU8(EPD.otpChk);
            ylog(" instead of A5\n");
            EPD.displayFailure = 1;
            return -1;
        }
        EPD.COGOTP_state = COGOTP_QSTEP7;
        return 1;
        
    case COGOTP_QSTEP3_DRV6:   
        PANELDC_OUT = 0;
        SPI3_write(0xf0);
        PANELDC_OUT = 1;
        SPI3_write(0x0b);
        PANELDC_OUT = 0;
        SPI3_write(0x90);
        EPD.COGOTP_state = COGOTP_QSTEP4_DRV6;
    case COGOTP_QSTEP4_DRV6:  
        AWAIT(aSync_waitBusy(HIGH, 1003));
        SPI3_write(0xa2);
        PANELDC_OUT = 1;
        SPI3_write(0x33);
        PANELDC_OUT = 0;
        SPI3_write(0xa0);
        EPD.COGOTP_state = COGOTP_QSTEP5_DRV6;
    case COGOTP_QSTEP5_DRV6:  
        AWAIT(aSync_waitBusy(HIGH, 1004));
        SPI3_write(0xf6);
        PANELDC_OUT = 1;
        if(EPD.chipId == 0xc901) {  // 206_QS_06
            SPI3_write(0x0d);   
            SPI3_write(0x80);
        } else {                    // 152_QS_06 (alias 152_QS_H6)
            // The OTP address for chipId 0x4801 was found by searching OTP 
            // memory for 0xA5, followed 11 bytes later by 0x04 (ncolors)
            SPI3_write(0x2d);   
            SPI3_write(0x80);
        }
        PANELDC_OUT = 0;
        SPI3_write(0x92);
        EPD.COGOTP_state = COGOTP_QSTEP6_DRV6;
    case COGOTP_QSTEP6_DRV6:
        AWAIT(aSync_delay(10));
        PANELDC_OUT = 1;
        SPI3_read(); // Dummy read
        EPD.otpChk = SPI3_read(); // First byte to be checked
        if (EPD.otpChk != 0xa5) {
            epaper_errmsg = "OTP check failed, A5 expected";
            ylog("OTP check failed - read ");
            ylogU8(EPD.otpChk);
            ylog(" instead of A5\n");
            EPD.displayFailure = 1;
            return -1;
        }
        EPD.COGOTP_state = COGOTP_QSTEP7;
        
    case COGOTP_QSTEP7:
        EPD.COG_data[0] = EPD.otpChk;
        for (index = 1; index < EPD.readBytes; index++) {
            EPD.COG_data[index] = SPI3_read(); // Read OTP
        }
#if 1
        ylog("COG 0x");
        ylogU16(EPD.chipId);
        ylogChar(' ');
        ylogU8(EPD.COG_data[2]);
        ylogChar('v');
        ylogNum(EPD.COG_data[3]);
        ylogChar('r');
        ylogNum(EPD.COG_data[4]);
        ylogChar(' ');
        yloglen((char *)EPD.COG_data+5,6);
        ylogChar(' ');
        ylogNum(EPD.COG_data[0xb]);
        ylog(" colors\n");
#endif
        EPD.OTP_done = 1;
        EPD.COGOTP_state = COGOTP_COMPLETED;
        return 1;
        
    case COGOTP_COMPLETED:
        break;
    }
    EPD.COGOTP_state = COGOTP_START;
    return 0;
}

static int aSync_COG_initialize(u8 updateMode)
{
    switch (EPD.ACGINIT_state) { 
    case ACGINIT_START:
        if(epaper_panel.film == FILM_Q) {
            if(epaper_panel.driver == DRIVER_F) { // 154_QS_0F, 213_QS_0F, 266_QS_0F
                EPD.ACGINIT_state = ACGINIT_QSTEP1_DRVF;
                return 1;
            } else if(epaper_panel.driver == DRIVER_6) { // DRIVER_6: 206_QS_06
                EPD.ACGINIT_state = ACGINIT_QSTEP1_DRV6;
                return 1;
            } else {
                return 0; // not supported (was tested before)
            }
        } else { // all others (B/W)
            if(epaper_panel.driver == DRIVER_J) { // 150_KS_0J, 152_KS_0J
                EPD.ACGINIT_state = ACGINIT_BWSTEP1_DRVJ;
                return 1;
            } else {
                EPD.ACGINIT_state = ACGINIT_BWSTEP1_OTHER;
                return 1;
            }
        }
        
    case ACGINIT_BWSTEP1_DRVJ:
        // Soft reset
        spiSendCmd(0x12);
        PANELDC_OUT = 0;
        EPD.ACGINIT_state = ACGINIT_BWSTEP2_DRVJ;
    case ACGINIT_BWSTEP2_DRVJ: 
        AWAIT(aSync_waitBusy(LOW, 5001));
        // Work settings
        spiSendCmdData(0x1a, epaper_temperature);
        if (updateMode == EPAPER_FULL_UPDATE) {
            spiSendCmdData(0x22, 0xd7);
        } else if (updateMode == EPAPER_FAST_UPDATE) {
            spiSendCmdData(0x3c, 0xc0);
            spiSendCmdData(0x22, 0xdf);
        }
        EPD.ACGINIT_state = ACGINIT_COMPLETED;
        return 1;

    case ACGINIT_BWSTEP1_OTHER:
        if (updateMode != EPAPER_FULL_UPDATE) {
            // Specific settings for fast update
            EPD.optTemperature = epaper_temperature | 0x40; // temperature | 0x40
            EPD.dataToSend[0] = EPD.COG_data[0] | 0x10; // PSR0 | 0x10
            EPD.dataToSend[1] = EPD.COG_data[1] | 0x02; // PSR1 | 0x02
        } else {
            // Common settings
            EPD.optTemperature = epaper_temperature; // Temperature
            EPD.dataToSend[0] = EPD.COG_data[0]; // PSR0
            EPD.dataToSend[1] = EPD.COG_data[1]; // PSR1
        }
        // New algorithm
        spiSendCmdData(0x00, 0x0e); // Soft-reset
        EPD.ACGINIT_state = ACGINIT_BWSTEP2_OTHER;
    case ACGINIT_BWSTEP2_OTHER: 
        AWAIT(aSync_waitBusy(HIGH, 5003));
        spiSendCmdData(0xe5, EPD.optTemperature); // Input Temperature
        spiSendCmdData(0xe0, 0x02); // Activate Temperature
        if(epaper_panel.driver == DRIVER_F) { // 290_KS_0F            
            spiSendCmdData(0x4d, 0x55); // No PSR
            spiSendCmdData(0xe9, 0x02);
        } else {            
            spiSendSmallBlock(0x00, EPD.dataToSend, 2); // Use PSR
        }
        // Specific settings for fast update, all screens
        if (updateMode != EPAPER_FULL_UPDATE) {
            spiSendCmdData(0x50, 0x07); // Vcom and data interval setting
        }
        EPD.ACGINIT_state = ACGINIT_COMPLETED;
        return 1;             

    case ACGINIT_QSTEP1_DRVF:
        spiSendCmdData(0xe0, 0x02);                 // Activate Temperature
        spiSendCmdData(0xe6, epaper_temperature);   // Input Temperature
        spiSendCmd(0xa5);
        EPD.ACGINIT_state = ACGINIT_QSTEP2_DRVF;
    case ACGINIT_QSTEP2_DRVF: 
        AWAIT(aSync_waitBusy(HIGH, 5004));
        spiSendCmdData(0x01, EPD.COG_data[16]);         // PWR
        spiSendSmallBlock(0x00, &EPD.COG_data[17], 2);  // PSR
        spiSendSmallBlock(0x03, &EPD.COG_data[30], 3);  // PFS
        spiSendSmallBlock(0x06, &EPD.COG_data[23], 7);  // BTST
        spiSendCmdData(0x50, EPD.COG_data[39]);         // CDI
        spiSendSmallBlock(0x60, &EPD.COG_data[40], 2);  // TCON
        EPD.ACGINIT_state = ACGINIT_QSTEP3_DRVF;
        return 1;
    case ACGINIT_QSTEP3_DRVF: 
        spiSendSmallBlock(0x61, &EPD.COG_data[19], 4);  // TRES
        spiSendCmdData(0xe7, EPD.COG_data[33]);
        spiSendCmdData(0xe3, EPD.COG_data[42]);         // PWS
        spiSendCmdData(0x4d, EPD.COG_data[43]);
        spiSendCmdData(0xb4, EPD.COG_data[44]);
        spiSendCmdData(0xb5, EPD.COG_data[45]);
        spiSendCmdData(0xe9, 0x01);
        spiSendCmdData(0x30, 0x08);
        EPD.ACGINIT_state = ACGINIT_COMPLETED;
        return 1;             
        
    case ACGINIT_QSTEP1_DRV6:
        spiSendCmdData(0xe0, 0x02);                 // Activate Temperature
        spiSendCmdData(0xe6, epaper_temperature);   // Input Temperature
        spiSendCmd(0xa5);
        EPD.ACGINIT_state = ACGINIT_QSTEP2_DRV6;
    case ACGINIT_QSTEP2_DRV6: 
        AWAIT(aSync_waitBusy(HIGH, 5005));
        spiSendSmallBlock(0x01, &EPD.COG_data[16], 2);
        spiSendSmallBlock(0x00, &EPD.COG_data[18], 2);
        EPD.ACGINIT_state = ACGINIT_QSTEP3_DRV6;
    case ACGINIT_QSTEP3_DRV6: 
        AWAIT(aSync_waitBusy(HIGH, 5006));
        spiSendSmallBlock(0x61, &EPD.COG_data[20], 4);
        EPD.ACGINIT_state = ACGINIT_QSTEP4_DRV6;
    case ACGINIT_QSTEP4_DRV6: 
        AWAIT(aSync_waitBusy(HIGH, 5007));
        spiSendSmallBlock(0x06, &EPD.COG_data[24], 4); // send 4 or 7 ?
        spiSendCmdData(0x03, EPD.COG_data[30]);
        spiSendCmdData(0xe7, EPD.COG_data[33]);
        spiSendSmallBlock(0x65, &EPD.COG_data[34], 4);
        EPD.ACGINIT_state = ACGINIT_QSTEP5_DRV6;
        return 1;
    case ACGINIT_QSTEP5_DRV6: 
        spiSendCmdData(0x30, EPD.COG_data[38]);
        spiSendCmdData(0x50, EPD.COG_data[39]);
        spiSendSmallBlock(0x60, &EPD.COG_data[40], 2);
        spiSendCmdData(0xe3, EPD.COG_data[42]);
        spiSendSmallBlock(0x62, &EPD.COG_data[43], 2);
        spiSendCmdData(0xe9, 0x01);
        EPD.ACGINIT_state = ACGINIT_COMPLETED;
        return 1;             
        
    case ACGINIT_COMPLETED:  
        break;            
    }    
    EPD.ACGINIT_state = ACGINIT_START;
    return 0;
}

static int aSync_COG_update(u8 updateMode)
{
    switch(EPD.COGUPDT_state) {
    case COGUPDT_START:        
        if(epaper_panel.driver == DRIVER_J) { // 150_KS_0J, 152_KS_0J
            EPD.COGUPDT_state = COGUPDT_STEP1_DRVJ;
            return 1;
        } else if(epaper_panel.driver == DRIVER_C) { // 266_JS_0C, 437_KS_0C
            EPD.COGUPDT_state = COGUPDT_STEP1_DRVC;
            return 1;
        } else {
            EPD.COGUPDT_state = COGUPDT_STEP1_OTHER;
            return 1;
        }
        
    case COGUPDT_STEP1_DRVJ:
        AWAIT(aSync_waitBusy(LOW, 5008)); // 152 specific
        spiSendCmd(0x20);       // Display Refresh
        PANEL_CS_OUT = 1;
        EPD.COGUPDT_state = COGUPDT_STEP2_DRVJ;
    case COGUPDT_STEP2_DRVJ:           
        AWAIT(aSync_waitBusy(LOW, 5009)); // 152 specific
        EPD.COGUPDT_state = COGUPDT_COMPLETED;
        return 1;

    case COGUPDT_STEP1_DRVC:
        AWAIT(aSync_waitBusy(HIGH, 5020));
        spiSendCmd(0x04);    // Power on
        EPD.COGUPDT_state = COGUPDT_STEP2_DRVC;
    case COGUPDT_STEP2_DRVC:
        // In some cases (but not always), BUSY might not 
        // come up properly (objserved on 266_JS_0C, 437_KS_0C)
        // So we don't rely on it, but simply give enough time.
        AWAIT(aSync_delay(20));
        // Trigger Display Refresh
        spiSendCmd(0x12);
        EPD.COGUPDT_state = COGUPDT_STEP3_OTHER;
        return 1;
        
    case COGUPDT_STEP1_OTHER:
        AWAIT(aSync_waitBusy(HIGH, 5010));
        spiSendCmd(0x04);           // Power on
        EPD.COGUPDT_state = COGUPDT_STEP2_OTHER;
    case COGUPDT_STEP2_OTHER:
        AWAIT(aSync_waitBusy(HIGH, 5011));
        // Display Refresh
        if(epaper_panel.film == FILM_Q) {
            spiSendCmdData(0x12, 0);  
        } else {
            spiSendCmd(0x12);
        }
        EPD.COGUPDT_state = COGUPDT_STEP3_OTHER;
    case COGUPDT_STEP3_OTHER:  
        AWAIT(aSync_delay(5));
        EPD.COGUPDT_state = COGUPDT_STEP4_OTHER;
    case COGUPDT_STEP4_OTHER:           
        AWAIT(aSync_waitBusy(HIGH, 29001));
        EPD.COGUPDT_state = COGUPDT_COMPLETED;
        
    case COGUPDT_COMPLETED:
        break;   
   }  
   EPD.COGUPDT_state = COGUPDT_START;
   return 0;         
}

static int aSync_COG_stopDCDC(void)
{
    switch(EPD.COGPWROFF_State) {
    case COGPWROFF_START :
         // Application note § 7. Turn-off DC/DC
        if(epaper_panel.driver == DRIVER_J) { // 150_KS_0J, 152_KS_0J
            EPD.COGPWROFF_State = COGPWROFF_COMPLETED;
            return 1;
        }
        EPD.COGPWROFF_State = COGPWROFF_DEFAULTDISPLAY;
    case COGPWROFF_DEFAULTDISPLAY:
        // Turn off DC/DC
        if(epaper_panel.film == FILM_Q) {
            spiSendCmdData(0x02, 0x00);
        } else {
            spiSendCmd(0x02);
        }
        EPD.COGPWROFF_State = COGPWROFF_WAITBSY;
    case COGPWROFF_WAITBSY:        
        AWAIT(aSync_waitBusy(HIGH, 5012));
        if(epaper_panel.driver != DRIVER_6) {
            spiRelease();
            break;
        }
        spiSendCmdData(0x07, 0xa5);
        EPD.COGPWROFF_State = COGPWROFF_WAITMORE;
    case COGPWROFF_WAITMORE:
        AWAIT(aSync_delay(50));
        EPD.COGPWROFF_State = COGPWROFF_COMPLETED;
    case COGPWROFF_COMPLETED:
        spiRelease();
        break;
    }    
    EPD.COGPWROFF_State = COGPWROFF_START;
    return 0;
}

/************************************************************************
 *  HAL PART
 ************************************************************************/

// Attempt to change the panel model
// Return NULL on success
//        an error message on failure
const char *display_setPanel(yDisplayPanel *panel)
{
    E2panel prevPanel = epaper_panel;
    const char *errmsg = NULL;
    char film, drv;

    if(panel->type != PANEL_TYPE_E2) {
        if(panel->type == PANEL_TYPE_NONE) {
            errmsg = "no panel selected";
        } else {
            errmsg = "not from Pervasive Displays";
        }
    bad_panelChoice:
        epaper_panel.asDword = 0;
        epaper_disp.width = 16;
        epaper_disp.height = 16;
        epaper_disp.pixbytes = 32;
        epaper_disp.bpp = 1;
        EPD.displayFailure = 1;
        return errmsg;
    }
    switch(panel->size) {
    case 152:
        epaper_disp.width = 200;
        epaper_disp.height = 200;
        break;
    case 154:
        epaper_disp.width = 152;
        epaper_disp.height = 152;
        break;
    case 206:
        epaper_disp.width = 248;
        epaper_disp.height = 128;
        break;        
    case 213:
        epaper_disp.width = 212;
        epaper_disp.height = 104;
        break;
    case 266:
        epaper_disp.width = 296;
        epaper_disp.height = 152;
        break;
    case 271:
        epaper_disp.width = 264;
        epaper_disp.height = 176;
        break;
    case 287:
        epaper_disp.width = 296;
        epaper_disp.height = 128;
        break;
    case 290:
        epaper_disp.width = 384;
        epaper_disp.height = 168;
        break;
    case 370:
        epaper_disp.width = 416;
        epaper_disp.height = 240;
        break;
    case 417:
        epaper_disp.width = 300;
        epaper_disp.height = 400;
        break;
    case 437:
        epaper_disp.width = 480;
        epaper_disp.height = 176;
        break;
    default:
        errmsg = "unsupported size";
        goto bad_panelChoice;
    }
    if((epaper_disp.height & 7) != 0) {
        errmsg = "unsupported size";
        goto bad_panelChoice;
    }
    
    film = panel->film;
    if(film != FILM_K && film != FILM_P && film != FILM_C && 
       film != FILM_Q && film != FILM_J) {
        errmsg = "unsupported film";
        goto bad_panelChoice;        
    }

    epaper_disp.canfast = 1; 
    epaper_disp.bpp = 1;
    switch(film) {
    case FILM_C: // Standard
        epaper_disp.work_tempMin = 0;
        epaper_disp.work_tempMax = 50;
        epaper_disp.fast_tempMin = 0;    // Fast update not officially supported
        epaper_disp.fast_tempMax = 0;    // (but in practice it worked for us...)
        break;
    case FILM_J: // Spectra Red
        epaper_disp.bpp = 2;
        epaper_disp.canfast = 0; 
        epaper_disp.work_tempMin = 0;
        epaper_disp.work_tempMax = 40;
        epaper_disp.fast_tempMin = 0;
        epaper_disp.fast_tempMax = 0;
        break;
    case FILM_K: // Wide temperature
        epaper_disp.work_tempMin = -20;  // spec says -15'C but -20'C is workable
        epaper_disp.work_tempMax = 60;
        epaper_disp.fast_tempMin = 0;
        epaper_disp.fast_tempMax = 50;
        break;
    case FILM_P: // Embedded fast update (Aurora)
        epaper_disp.work_tempMin = 0;
        epaper_disp.work_tempMax = 50;
        epaper_disp.fast_tempMin = 15;
        epaper_disp.fast_tempMax = 30;
        break;
    case FILM_Q: // Spectra 4
        epaper_disp.bpp = 2;
        epaper_disp.canfast = 0; 
        epaper_disp.work_tempMin = 0;
        epaper_disp.work_tempMax = 40;
        epaper_disp.fast_tempMin = 0;
        epaper_disp.fast_tempMax = 0;
        break;
    }

    if(epaper_disp.width * (u32)(epaper_disp.height >> 3) > MAX_PIXEL_BYTES / epaper_disp.bpp) {
        errmsg = "too large";
        goto bad_panelChoice;
    }

    drv = panel->cog[1];
    if(drv != DRIVER_6 && drv != DRIVER_9 && drv != DRIVER_C && drv != DRIVER_D && 
       drv != DRIVER_E && drv != DRIVER_F && drv != DRIVER_J) {
        // eg. drivers 'A' and 'B', used for larger screens
        errmsg = "unknown driver";
        goto bad_panelChoice;                
    }

    epaper_panel.size = panel->size;
    epaper_panel.film = film;
    epaper_panel.driver = drv;
    if(prevPanel.asDword != epaper_panel.asDword) {
        epaper_disp.pixbytes = epaper_disp.width * (epaper_disp.height >> 3) * epaper_disp.bpp;
        memset(mainbuffer, 0, epaper_disp.pixbytes);
        memset(&EPD, 0, sizeof(EPD));
    }
    return NULL;
}

int display_init_controler(void)
{
    if (EPD.displayFailure || EPD.INITCTRL_state == INITCTRL_START) {
        memset(&EPD, 0, sizeof(EPD));
    }
    switch (EPD.INITCTRL_state) { 
    case INITCTRL_START:
        // Turn 3.3V power on
        PANELPOWER_OUT = 0;
        // Setup SPI port
        spiPortSetup();
        spiResume(SPIMODE_3WIRE_BIDI);
        EPD.INITCTRL_state = INITCTRL_STEP1;
    case INITCTRL_STEP1:
        AWAIT(aSync_COG_reset(1));
        EPD.INITCTRL_state = INITCTRL_STEP2; 
    case INITCTRL_STEP2:
        AWAIT(aSync_COG_getDataOTP());
        EPD.INITCTRL_state = INITCTRL_STEP3;
    case INITCTRL_STEP3:     
        // Reset COG after reading OTP data
        AWAIT(aSync_COG_reset(0));
    }
    EPD.INITCTRL_state = INITCTRL_START;
    return 0;
}

void display_forceFullUpdate(u8 full)
{
    EPD.fastUpdatePossible = !full;
}

int display_prepShowBuffer(void)
{
    if (EPD.displayFailure) return -1;

    switch (EPD.PUPD_state) { 
    case PUPD_START:
        EPD.PUPD_state = PUPD_STEP1;
    case PUPD_STEP1:
        // Reset COG
        AWAIT(aSync_COG_reset(0));
        if (EPD.OTP_done) { 
            // OTP data already loaded, go to the real job
            EPD.PUPD_state = PUPD_STEP4;
            return 1;
        }
        EPD.PUPD_state = PUPD_STEP2; 
    case PUPD_STEP2:
        AWAIT(aSync_COG_getDataOTP());
        EPD.PUPD_state = PUPD_STEP3;
    case PUPD_STEP3:     
        // Reset COG after reading OTP data
        AWAIT(aSync_COG_reset(0));
        EPD.PUPD_state = PUPD_STEP4; 
    case PUPD_STEP4:
        spiResume(SPIMODE_SEND_FAST);
        if(epaper_panel.film == FILM_Q || epaper_panel.film == FILM_J) {
            EPD.updateInProgress = EPAPER_FULL_UPDATE;
            EPD.fastUpdatePossible = 0;
        } else {
            EPD.updateInProgress = (EPD.fastUpdatePossible ? EPAPER_FAST_UPDATE : EPAPER_FULL_UPDATE);
            EPD.fastUpdatePossible = 1;            
        }
        EPD.PUPD_state = PUPD_STEP5;
    case PUPD_STEP5:
        AWAIT(aSync_COG_initialize(EPD.updateInProgress));
        if (EPD.updateInProgress == EPAPER_FULL_UPDATE) {
            // no need to send 'previous' buffer for now, we are done !
            EPD.PUPD_state = PUPD_DONE;
            break;
        }
        // Fast update: resend old image as 1st buffer
        if(epaper_panel.driver == DRIVER_J) { // eg. 150_KS_0J or 152_KS_0J
            spiStartSendingDataBlock(0x24, mainbuffer, epaper_disp.pixbytes, -1);
        } else {
            if (EPD.flag50) {
                spiSendCmdData(0x50, 0x27); // Vcom and data interval setting
            }
            spiStartSendingDataBlock(0x10, mainbuffer, epaper_disp.pixbytes, -1);
        }
        EPD.PUPD_state = PUPD_STEP6;
        return 1;
    case PUPD_STEP6:
        AWAIT(aSync_spiWaitUntilDataBlockIsSent());
        EPD.PUPD_state = PUPD_DONE;
    case PUPD_DONE:
        break;
    }
    // EPD.PUPD_state will be reset in display_doShowBuffer()
    return 0;
}

int display_doShowBuffer(void)
{
    if (EPD.displayFailure) return -1;

    switch (EPD.DUPD_state) { 
    case DUPD_START:
        if (EPD.updateInProgress == EPAPER_FULL_UPDATE) {
            if(epaper_panel.film == FILM_J) { // eg. 266_JS_0C
                // global update: send black bits as 1st buffer
                spiStartSendingDataBlock(0x10, mainbuffer, epaper_disp.pixbytes, 0);
            } else if(epaper_panel.driver == DRIVER_J) { // eg. 150_KS_0J or 152_KS_0J
                // global update: send new image as 1st buffer
                spiStartSendingDataBlock(0x24, mainbuffer, epaper_disp.pixbytes, -1);
            } else {
                // global update: send new image as 1st buffer
                spiStartSendingDataBlock(0x10, mainbuffer, epaper_disp.pixbytes, -1);
            }
            EPD.DUPD_state = DUPD_STEP1_normal;
            return 1;
        } else {
            // fast update: send new image as 2nd buffer
            if(epaper_panel.driver == DRIVER_J) { // eg. 150_KS_0J or 152_KS_0J
                spiStartSendingDataBlock(0x26, mainbuffer, epaper_disp.pixbytes, -1);
            } else {
                spiStartSendingDataBlock(0x13, mainbuffer, epaper_disp.pixbytes, -1);
            }
            EPD.DUPD_state = DUPD_STEP1_fast;
        }
    case DUPD_STEP1_fast:
        AWAIT(aSync_spiWaitUntilDataBlockIsSent());
        if (EPD.flag50) {
            // Additional settings for fast update, 154 213 266 370 and 437 screens 
            spiSendCmdData(0x50, 0x07); // Vcom and data interval setting
        }
        EPD.DUPD_state = DUPD_STEP3;
        return 1;
    case DUPD_STEP1_normal:
        AWAIT(aSync_spiWaitUntilDataBlockIsSent());
        if(epaper_panel.film == FILM_Q) {
            // no need for a second buffer (color bits are interlaced)
            EPD.DUPD_state = DUPD_STEP3;
        } else if(epaper_panel.film == FILM_J) { // eg. 266_JS_0C
            // global update: send red bits as 2nd buffer
            spiStartSendingDataBlock(0x13, mainbuffer, epaper_disp.pixbytes, 1);
            EPD.DUPD_state = DUPD_STEP2;
        } else if(epaper_panel.driver == DRIVER_J) { // eg. 150_KS_0J or 152_KS_0J
            // global update: send empty 2nd buffer
            spiStartSendingDataBlock(0x26, NULL, epaper_disp.pixbytes, -1);
            EPD.DUPD_state = DUPD_STEP2;
        } else {
            // global update: send empty 2nd buffer
            spiStartSendingDataBlock(0x13, NULL, epaper_disp.pixbytes, -1);
            EPD.DUPD_state = DUPD_STEP2;
        }                            
        return 1;
    case DUPD_STEP2:
        AWAIT(aSync_spiWaitUntilDataBlockIsSent());
        EPD.DUPD_state = DUPD_STEP3;
    case DUPD_STEP3:
        AWAIT(aSync_COG_update(EPD.updateInProgress));
        EPD.DUPD_state = DUPD_STEP4;            
    case DUPD_STEP4:
        AWAIT(aSync_COG_stopDCDC());
    }
    EPD.updateInProgress = 0;
    EPD.DUPD_state = PUPD_START;
    // allow next prepShowBuffer to run again as well
    EPD.PUPD_state = PUPD_START;
    return 0;
}

// return non-zero iff the panel is in the process of updating display (after data transfer)
// (value can be EPAPER_FULL_UPDATE or EPAPER_FAST_UPDATE)
int display_update_in_progress(void)
{
    if(EPD.COGUPDT_state != COGUPDT_START) {
        return EPD.updateInProgress;
    }
    return 0;
}

void display_poweroff_controler(void)
{
    // Turn off all digital pins
    spiRelease();
    PANEL_CS_OUT   = 0;    
    PANELRESET_OUT = 0;
    
    // Turn 3.3V power off
    PANELPOWER_OUT = 1;
}
