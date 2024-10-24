/*
 * Lanner Parallel LCM driver 
 */
#include "lcm.h"
#include <linux-asm-io.h>
#include <shared.h>
#include <timer.h>
#include <panel.h>

#ifndef NULL
#define NULL 0
#endif

/*
 * Device Depend Function Prototypes
 */

/*
 * Device Depend Definition
 */
#define LPT1 0x378
#define LPT2 0x278
#define LPT3 0x3BC

#define ENABLE 0x02

#if 0
#define udelay(n)   {int xloop = n; do {outb(80, 0) }while(xloop--);}
#else
#define udelay(n)   waiton_timer2(((n)*TICKS_PER_MS)/1000)
#endif
/*
 * Device Depend Variables
 */
static unsigned int  Port_Addr = 0; // LPTx Port Address
static unsigned int  DataPort = 0;
static unsigned int  StatusPort = 0;
static unsigned int  ControlPort = 0;
static unsigned char Backlight = 0; // Backlight ON

void load_timer2(unsigned int ticks)
{
    /* Set up the timer gate, turn off the speaker */
    outb((inb(PPC_PORTB) & ~PPCB_SPKR) | PPCB_T2GATE, PPC_PORTB);
    outb(TIMER2_SEL|WORD_ACCESS|MODE0|BINARY_COUNT, TIMER_MODE_PORT);
    outb(ticks & 0xFF, TIMER2_PORT);
    outb(ticks >> 8, TIMER2_PORT);
}

void LCM_Init(void)
{
//    unsigned int i = 0;
    Port_Addr = LPT1;
#if 0
    if(inb(LPT1) != 0xFF)
    {
        Port_Addr = LPT1;
    }
    else if(inb(LPT2) != 0xFF)
    {
        Port_Addr = LPT2;
    }
    else if(inb(LPT3) != 0xFF)
    {
        Port_Addr = LPT3;
    }
    else
    {
        return;
    }
#endif

    DataPort = Port_Addr;
    StatusPort = Port_Addr + 1;
    ControlPort = Port_Addr + 2;


    if (LCDB_filton == 1) {

        LCM_Command(0, 0, 0x30, 300, NULL); // Function Set
        LCM_Command(0, 0, 0x30,  100, NULL);
    } else {
        LCM_Command(0, 0, 0x38, 8000, NULL); // Function Set
        LCM_Command(0, 0, 0x38,  300, NULL);
    }

    LCM_Command(0, 0, 0x38,  300, NULL);
    LCM_Command(0, 0, 0x38,  300, NULL);
    LCM_Command(0, 0, 0x0F,  300, NULL); // Display On/OFF,  display=ON,cursor=ON,blink=ON
    LCM_Command(0, 0, 0x01, 3000, NULL); // Display Clear

    LCM_Command(0, 0, 0x0C,  300, NULL); // Set No Cursor
    LCM_Command(1, 0, 0x00,  300, NULL);    // Set No Cursor

    LCM_Command(0, 0, 0x06,  300, NULL); // Entry Mode Set
    LCM_Command(0, 0, 0x80,  300, NULL); // Set DDRAM Address  

#if 0
    LCM_Command(0, 0, 0x06,  300, NULL); // Entry Mode Set
    LCM_Command(0, 0, 0x0C,  300, NULL); // Set No Cursor
    LCM_Command(1, 0, 0,  300, NULL);    // Set No Cursor
    LCM_Cls();
#endif

#if 0
    LCM_Command(0, 0, 0x80,  300, NULL); // Set DDRAM Address   
    for(i = 0; i < 40; i++) // Range: 0x00~0x27
    {
        LCM_Command(1, 0, ' ', 300, NULL); // Write Data
    }

    LCM_Command(0, 0, 0xC0, 300, NULL); // Set DDRAM Address
    for(i = 0; i < 40; i++) // Range: 0x40~0x67
    {
        LCM_Command(1, 0, ' ', 300, NULL); // Write Data
    }
    // Add character in CGRAM as below.
    /*
    // 11111 
    // 10001
    // 10101
    // 10101
    // 10101
    // 10001
    // 11111
    // 00000
    */
    for(i = 0; i < 8; i++)
    {
        LCM_Command(0, 0, 0x40+i*8+0, 300, NULL);
        LCM_Command(1, 0, 0x1F, 300, NULL);
        LCM_Command(0, 0, 0x40+i*8+1, 300, NULL);
        LCM_Command(1, 0, 0x11, 300, NULL);
        LCM_Command(0, 0, 0x40+i*8+2, 300, NULL);
        LCM_Command(1, 0, 0x15, 300, NULL);
        LCM_Command(0, 0, 0x40+i*8+3, 300, NULL);
        LCM_Command(1, 0, 0x15, 300, NULL);
        LCM_Command(0, 0, 0x40+i*8+4, 300, NULL);
        LCM_Command(1, 0, 0x15, 300, NULL);
        LCM_Command(0, 0, 0x40+i*8+5, 300, NULL);
        LCM_Command(1, 0, 0x11, 300, NULL);
        LCM_Command(0, 0, 0x40+i*8+6, 300, NULL);
        LCM_Command(1, 0, 0x1F, 300, NULL);
        LCM_Command(0, 0, 0x40+i*8+7, 300, NULL);
        LCM_Command(1, 0, 0x00, 300, NULL);
    }
#endif

    return;
}

void LCM_Write(unsigned char Line, unsigned char *Text)
{
    unsigned char LCM_Message[40];
    int i = 0;
    int length = strlen(Text);

    for(i = 0; i < 40; i++)
    {
        LCM_Message[i] = ' ';
    }

    if(length > 40)
    {
        return;
    }

    if(Line == 1)
    {
        LCM_Command(0, 0, 0x80, 300, NULL);
    }
    else if(Line == 2)
    {
        if (LCDB_filton == 1)
            LCM_Command(0, 0, 0x90, 300, NULL);
        else
            LCM_Command(0, 0, 0xC0, 300, NULL);
    }
    for(i = 0; i < 40; i++)
    {
        if((length - i) > 0)
        {
            LCM_Message[i] = Text[i];
            if(LCM_Message[i] == 0)
            {
                LCM_Message[i] = ' ';
            }
        }
        LCM_Command(1, 0, LCM_Message[i], 300, NULL);
    }

    return;
}

void LCM_Cls()
{
    LCM_Write(1, "");
    LCM_Write(2, "");

    return;
}

unsigned char LCM_GetButtons()
{
    unsigned char btn;
    btn = inb(StatusPort);
    if (LCDB_filton == 1) {	/* so, filton, do it filton way */
        switch (btn)
        {
            /* down button */
            case 0xC7:
                return BTN_DOWN;
            /* left */
            case 0xE7:
                return BTN_LEFT;
            /* up */
            case 0xCF:
                return BTN_UP;
            /* right */
            case 0xEF:
                return BTN_RIGHT;
            default:
                return 0;
        }
    } else { 			/* all other plcm based platfroms <vashon/kennewick> */
        switch (btn)
        {
            /* down button */
            case 0xC7:
                return BTN_DOWN;
            /* left */
            case 0xCF:
                return BTN_LEFT;
            /* up */
            case 0xE7:
                return BTN_UP;
            /* right */
            case 0xEF:
                return BTN_RIGHT;
            default:
                return 0;
        }
    }
}

void LCM_Command(unsigned char RS, unsigned char RWn, unsigned char CMD, unsigned int uDelay, unsigned char *Ret)
{
    unsigned char Ctrl = 0;

    Ctrl |= Backlight;
    if(RS == 0)
    {
        Ctrl |= 0x08; // RS: Real RS = ~RS
    }
    if(RWn == 1)
    {
        Ctrl |= 0x24; // RWn: Read = 1, Write = 0
    }
    outb(Ctrl | ENABLE, ControlPort); // Set RS and RWn, E = 0
    udelay(40);
    outb(Ctrl & ~ENABLE, ControlPort); // E = 1 
    udelay(10);
    if(RWn == 0)
    {
        outb(CMD, DataPort); // LCM Data Write 
    }
    if((RWn == 1) && (Ret != NULL))
    {
        *Ret = inb(DataPort); // LCM Data Read  
    }
    udelay(300);
    outb(Ctrl | ENABLE, ControlPort); // E = 0
    udelay(uDelay + 1);
    return;
}

void LCM_Backlight(void)
{
    unsigned char Ctrl = inb(ControlPort);

    if(Backlight == 1)
    {
        Ctrl |= 0x01;
    }
    else
    {
        Ctrl &= ~0x01;
    }
    outb(Ctrl, ControlPort);
    return;
}

