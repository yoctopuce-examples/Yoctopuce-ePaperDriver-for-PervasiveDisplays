/*******************************************************************************
 *
 * Demo program to illustrate the use of Yoctopuce driver for
 * Pervasive Display ePaper panels.
 *
 * License:
 * 
 * Creative Commons Attribution-ShareAlike 4.0 International (CC BY-SA 4.0)
 * Copyright (c) Yoctopuce Sarl, 2026
 * 
 *****************************************************************************/

#include "api.h"

// =======================================================================
//   Hardware configuration for PIC24FJ256DA206
// =======================================================================

// Watchdog & Debug port
_CONFIG1(FWDTEN_OFF & ICS_PGx2 & GWRP_OFF & GCP_OFF & JTAGEN_OFF);
// Cristal-less operation at 16 Mips
_CONFIG2(POSCMOD_NONE & IOL1WAY_OFF & OSCIOFNC_ON & FCKSM_CSDCMD & FNOSC_FRCPLL & PLL96MHZ_ON & PLLDIV_DIV2 & IESO_OFF);
// We may want to use SOSC for I/O
_CONFIG3(SOSCSEL_EC);

static void setupHardware(void)
{
    int pll_startup_counter = 600;
    CLKDIVbits.PLLEN = 1;
    while (pll_startup_counter--);
    CLKDIVbits.RCDIV = 0;
    ANSB  = 0x0000;
    ANSC  = 0x0000;
    ANSD  = 0x0000;
    ANSF  = 0x0000;
    ANSG  = 0x0000;
    TRISB = 0xfe0f;
    TRISC = 0x0000;
    TRISD = 0x0301;
    TRISE = 0x00df;
    TRISF = 0x0088;
    TRISG = 0x03cc;
    YOCTO_LED_TRIS          = TRIS_OUTPUT;   // same pin as OC1
    PANEL_CS_TRIS           = TRIS_OUTPUT;   
    SDI1_TRIS               = TRIS_OUTPUT;   
    SCK1_TRIS               = TRIS_OUTPUT;   // same pin as PANEL_SCK
    PANELRESET_TRIS         = TRIS_OUTPUT;   
    PANELDC_TRIS            = TRIS_OUTPUT;   
    PANELPOWER_TRIS         = TRIS_OUTPUT;   

    // Turn 3.3V power off
    PANELPOWER_OUT = 1;
    
    // Timer 2 is set to 2MHz for microsecond timings
    T2CON = 0x0010;             // TCKPS = 1 (1:8 prescaler -> 2 MHz)
    PR2 = 0xffff;
    T2CONbits.TON = 1;

    // Timer 3 is set to 16MHz with a period of 1ms for timing
    T3CON = 0;                  // TCKPS = 0 (1:1 prescaler -> 16 MHz)
    PR3 = 15999;                // Period = 16000 -> 1 ms
    T3CONbits.TON = 1;          // Start timer
    _T3IP = 5;
    _T3IF = 0;
    _T3IE = 1;                  // Enable interrupts
    
    // Give 10ms for the power to stabilise
    u16 i;
    for(i = 0; i < 100; i++) delay_us(100);
}

// our interrupt-based millisecond counter
volatile u16 msCounter;
_FAST_ISR_NO_PSV _T3Interrupt(void)
{
    msCounter++;
    _T3IF = 0;
}

// delay_us: blocking wait for a given number of microseconds
void delay_us(u8 microseconds)
{
    u16 start = TMR2; // 2Mhz timer
    u16 wtime = (u16)microseconds << 1;
    while(1) {
        u16 now = TMR2;
        u16 elapsed = (now >= start) ? (now - start) : (now + 50000u - start);
        if(elapsed >= wtime) break;
    }
}

// =======================================================================
//   Linkage to our asynchronous PervasiveDisplays driver
// =======================================================================

u8          mainbuffer[MAX_PIXEL_BYTES];
const char *epaper_errmsg = "";
u16         bytesPerCol;

// Clear the display buffer
void clearFramebuffer(void)
{
    u8 whiteByte = (epaper_disp.bpp == 1 ? 0 : 0x55);
    memset(mainbuffer, whiteByte, epaper_disp.pixbytes);
}

// Change a single pixel in the display buffer
void putPixel(int x, int y, int col)
{
    x = epaper_disp.width - 1 - x;
    if(epaper_disp.bpp == 1) {
        u16 ofs = x * bytesPerCol + (y >> 3);
        if(col == COL_WHITE || col == COL_YELLOW) {
            mainbuffer[ofs] &= ~(0x80 >> (y & 7));
        } else {
            mainbuffer[ofs] |= (0x80 >> (y & 7));
        }
    } else { // bpp == 2
        u16 ofs = x * bytesPerCol + (y >> 2);
        u16 bitno = 2 * (y & 3);
        col ^= 1;
        mainbuffer[ofs] = mainbuffer[ofs] & ~(0xc0 >> bitno) | (col << (6 - bitno));
    }
}

// Draw a pixmap at specified location in the display buffer
void drawPixmap(u16 x, u16 y, const u8 *pixmap, u16 pixmapWidth, u16 pixmapHeight)
{
    u16 dx, dy;

    for(dy = 0; dy < pixmapHeight; dy++) {
        for(dx = 0; dx < pixmapWidth; dx++) {
            putPixel(x + dx, y + dy, *pixmap & 3);
            pixmap++;
        }
    }
}

// Fast flash LED when there is a problem initializing display
void panic(const char *errormsg)
{
    while(1) {
        YOCTO_LED_OUT = ((ytime16() & 0x7f) < 0x10 ? 1 : 0);            
    }
}

void initDisplay(yDisplayPanel *panel)
{
    int retCode;

    epaper_errmsg = display_setPanel(panel);
    if(epaper_errmsg) {
        panic("unsupported panel");
    }
    do {
        retCode = display_init_controler();
    } while(retCode > 0);
    if(retCode < 0) {
        panic("failed to initialise panel");
    }
    clearFramebuffer();
    bytesPerCol = (epaper_disp.height / 8) * epaper_disp.bpp;
    display_forceFullUpdate(1);
}

// =======================================================================
//   Small demo application
// =======================================================================

const u8 demoImage[] =
    "::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::"
    "::''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''::"
    "::''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''::"
    "::''                                                                                                        ''::"
    "::''                                                                                                        ''::"
    "::''   ====    ===                                                                                          ''::"
    "::''   ====    ===                                                                                          ''::"
    "::''    ====  ===                                                                                           ''::"
    "::''    ====  ===                                                                                           ''::"
    "::''      ======    =========  =========  =========  =========  ========  ==    ===  =========  ========    ''::"
    "::''      ======    =========  =========  =========  =========  ========  ==    ===  =========  ========    ''::"
    "::''        ===     ==     ==  ==     ==     ==      ==     ==  ==    ==  ==    ===  ==     ==  ==          ''::"
    "::''        ===     ==     ==  ==            ==      ==     ==  ==    ==  ==    ===  ==         ==          ''::"
    "::''        ===     ==     ==  ==            ==      ==     ==  ========  ==    ===  ==         ======      ''::"
    "::''        ===     ==     ==  ==     ==     ==      ==     ==  ==        ==    ===  ==     ==  ==          ''::"
    "::''        ===     ==     ==  ==     ==     ==      ==     ==  ==        ==    ===  ==     ==  ==          ''::"
    "::''        ===     =========  =========     ==      =========  ==        =========  =========  ========    ''::"
    "::''        ===     =========  =========     ==      =========  ==        ========   =========  ========    ''::"
    "::''                                                                                                        ''::"
    "::''                                                                                                        ''::"
    "::''                                                                                                        ''::"
    "::''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''::"
    "::''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''::"
    "::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::";
#define DEMO_IMAGE_WIDTH   112
#define DEMO_IMAGE_HEIGHT  24

void idle(void)
{
    // blink LED at a fixed frequency (2 Hz) to demonstrate
    // that display driver functions are non-blocking
    YOCTO_LED_OUT = ((ytime16() & 0x1ff) < 0x10 ? 1 : 0);
}

int main(void)
{
    // perform board-specific hardware pin configuration
    setupHardware();
    
    // >>>>>>>>>>>>>>>>>>>> Configure your display panel here <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
    // yDisplayPanel panel = { PANEL_TYPE_E2, .size=271, .film='K', .cog={'0','C'}, .hwrev=1 };
    // yDisplayPanel panel = { PANEL_TYPE_E2, .size=154, .film='Q', .cog={'0','F'}, .hwrev=1 };
    // yDisplayPanel panel = { PANEL_TYPE_E2, .size=266, .film='Q', .cog={'0','F'}, .hwrev=1 };
    // yDisplayPanel panel = { PANEL_TYPE_E2, .size=266, .film='J', .cog={'0','C'}, .hwrev=1 };
    yDisplayPanel panel = { PANEL_TYPE_E2, .size=271, .film='J', .cog={'0','C'}, .hwrev=1 };
    initDisplay(&panel);

    // If display panel supports fast refresh, update every 3 seconds
    // Otherwise, only update every minute
    u16 updateInterval = (epaper_disp.canfast ? 3000 : 60000);
    u16 lastRefresh = ytime16() + 1000;
    while(1) {        
        // refresh display every second, using Fast Refresh if supported
        u16 now = ytime16();
        if ((u16)(now - lastRefresh) > updateInterval) {
            lastRefresh = now;

            // prepare to update display buffer (non-blocking calls)
            while(display_prepShowBuffer() > 0) {
                idle();
            }

            // Demo code: move the pixmap around
            u16 x = TMR2 % (epaper_disp.width - DEMO_IMAGE_WIDTH);
            u16 y = ytime16() % (epaper_disp.height - DEMO_IMAGE_HEIGHT);
            clearFramebuffer();
            drawPixmap(x, y, demoImage, DEMO_IMAGE_WIDTH, DEMO_IMAGE_HEIGHT);

            // complete the display update (non-blocking calls)
            while(display_doShowBuffer() > 0) {
                idle();
            }
        }
        // update LED
        idle();
    }
}

