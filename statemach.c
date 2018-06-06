/************************************************************************/
/************************************************************************/
/*                                                                      */
/* Short PC Test program for a combination lock state machine:          */
/* File Name:           statemach.c                                     */
/*                                                                      */
/* Used By:             rkpdigital                                      */
/*                                                                      */
/* Operating System:    None Linux gcc.                                 */
/* Compiler:            Geaney and gcc                                  */
/* Model:               Assume Flat 16 bit                              */
/*                                                                      */
/* Purpose:             Event processing state machine monitors keys    */
/*               and internal status and controls the lock.             */
/*                                                                      */
/* Comments:                                                            */
/*                                                                      */
/*                                                                      */
/* Author:              R.K.Parker                                      */
/* Creation Date:       May 04, 2018                                    */
/*                                                                      */
/*                                                                      */
/* Revision 0.1  2018/05/04 RKP -  Initial prototype                    */
/*                                                                      */
/*                                                                      */
/************************************************************************/
/************************************************************************/
/***********************************!************************************/

/************************************************************************/
/*                          Compile Switches                            */
/************************************************************************/
//#define __DOS__



/************************************************************************/
/*                            GNU Includes                              */
/************************************************************************/
#if defined(__DOS__)
#include <windows.h>
#include <conio.h>
#else  /* Some type of unix. */
#include <termios.h>
#endif  /* Some type of unix. */

#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/************************************************************************/
/*                               Defines                                */
/************************************************************************/

#define TRUE           1
#define FALSE          0

#ifndef NULL
#define NULL           0
#endif

#define TIMER_IDLE 0
#define TIMER_DONE 1

#define LOCK_BUSY 1
#define LOCK_WAIT 2

#define GROGGY 1
#define WAKEUP 2

#define CODE_INVALID 0x02
#define CODE_VALID   0x03
#define CODE_START_OFFSET  0
#define CODE_NOT_VALID 1
#define CODE_CMP_FAILED 2
#define CODE_CMP_SUCCESS 3

#define CODE_IS_ADMIN_CODE 1
#define CODE_IS_PASSCODE 2
#define CODE_IS_UNKNOWN 3
#define CODE_IS_ESCAPE  4

#define UPPERC      0x5F
#define MAXDEC      0x39
#define DECC        0x30
#define HEXC        0x37
#define LFKEY       0x0A
#define CRKEY       0x0D
#define ESCKEY      0x1B

#define TERM_CHAR_CLEAR '-'
#define TERM_CHAR_ENTER '+'
#define TERM_CHAR_PROGEN '*'

#define MAX_KEY_CODE 16
#define MAX_CODE_SIZE 6
#define POSITION_CODE_SIZE 1
#define MAX_NUM_OF_PASSCODES 10

#define INTER_DIGIT_TIME 20    /* Changed from 2000 to 4000 */
#define UNLOCK_TICK_COUNT 2    /* 2 is minimum for all timers? */
#define LOCK_TICK_COUNT   2
#define LED_TURNOFF_COUNT   23
#define BEEP_TIME_ON_RIGHT_CODE 3
#define BEEP_TIME_OFF_SPACE 1
#define PUSH_BACK_COUNT     2


/************************************************************************/
/*                          Types/Enumerations                          */
/************************************************************************/

typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned long U32;

typedef signed char S8;
typedef signed short S16;
typedef signed long S32;

typedef union UU16
{
   U16 U16;
   S16 S16;
   U8 U8[2];
   S8 S8[2];
} UU16;

typedef U8 tBoolean;


typedef enum  //Put these in increasing order of time/priority so most ephemeral are cleared first. 
{
  BUZZ_TIMER     = 0,
  DEBOUNCE_TIMER = 1,
  UN_LOCK_TIMER  = 2,
  KEY_FUNC_TIMER = 3,
  LED_TIMER      = 4,
  LAST_TIMER     = 5
} TIMERTYPE_;

typedef enum
{
  STATEMACH_IDLE            = 0,
  STATEMACH_GET_PASSCODE    = 1,
  STATEMACH_GET_ADMIN_CODE  = 2,
  STATEMACH_GOT_ADMIN_CODE  = 3,
  STATEMACH_ADMIN_CODE_RPT  = 4,
  STATEMACH_INDX_OR_CLEAR   = 5,
  STATEMACH_CLEAR_CONFIRM   = 6,
  STATEMACH_PASS_CODE_GET   = 7,
  STATEMACH_LAST            = 8
} MACHSTATE_;


__attribute__((packed))  struct codestruct
{
    unsigned char flag;
    unsigned char code[MAX_CODE_SIZE];
};


/************************************************************************/
/*            External/Function Prototypes/Forward Refrences            */
/************************************************************************/
void disable_os_io(void);


/************************************************************************/
/*                     Global Variables/Constants                       */
/************************************************************************/
#if defined(__DOS__)
#else  /* Some type of unix. */
int Confd;
struct termios Condes;
struct termios OCondes;
#endif  /* Some type of unix. */

unsigned char keyChanged;
unsigned char lastKeyPressed;
unsigned int keyCodeIndx;
unsigned char keyCode[MAX_KEY_CODE];
unsigned int adminCodeIndx;
unsigned char tmpAdminCode1[MAX_KEY_CODE];
unsigned int selPassCodeIndx = 0;

struct codestruct adminCode;
struct codestruct passCode[MAX_NUM_OF_PASSCODES];

unsigned int TimerArray[LAST_TIMER + 1];
clock_t TimerDiff = 0;

MACHSTATE_ stateMachineState;

/*Debug*/
unsigned int SleepFlag = FALSE;


/************************************************************************/
/*                    Generic/Internal Functions                        */
/************************************************************************/
/************************************************************************/
/*                                                                      */
/* Function Name: enable_os_io                                          */
/*                                                                      */
/* Purpose:   Setup unbuffered IO for unix systems.                     */
/*                                                                      */
/* Input(s):  None                                                      */
/*                                                                      */
/* Output(s): None                                                      */
/*                                                                      */
/* Return(s): None                                                      */
/*                                                                      */
/* Calls:     None                                                      */
/*                                                                      */
/* Tasks:     None                                                      */
/*                                                                      */
/* Errors:    None                                                      */
/*                                                                      */
/* Comments:  Disables STDIO buffering in both DOS and UNIX systems.    */
/*                                                                      */
/* Revision:                                                            */
/* 0.1  00/08/18 RKP     Initial prototype                              */
/*                                                                      */
/************************************************************************/
void enable_os_io(void)
{
#ifdef __DOS__
  setvbuf(stdin, NULL, _IONBF,0);
  setvbuf(stdout, NULL, _IONBF,0);
  setvbuf(stderr, NULL, _IONBF,0);

#else  /* NOT DOS Some type of unix. */

  Confd = STDIN_FILENO;
  tcgetattr(Confd, &Condes);
  OCondes = Condes;
  Condes.c_iflag |= IGNBRK;
  Condes.c_iflag &= ~(BRKINT | ICRNL);
  Condes.c_cc[VMIN] = 0;                      /* Min char to read.*/
  Condes.c_cc[VTIME] = 0;                     /* Wait time for char.*/
  Condes.c_lflag &= ~ICANON;                   /* Raw mode.*/
  Condes.c_lflag &= ~PENDIN;        /* This makes this work better.*/
  Condes.c_lflag &= ~(ECHO | ECHOCTL | ECHONL);  /* No local echo.*/
  Condes.c_lflag &= ~ISIG;selPassCodeIndx = 

  tcsetattr(Confd, TCSANOW, &Condes);

#endif  /* Some type of unix. */
}


/************************************************************************/
/*                                                                      */
/* Function Name: disable_os_io                                         */
/*                                                                      */
/* Purpose:   Close down the unbuffered IO console for unix systems.    */
/*                                                                      */
/* Input(s):  None                                                      */
/*                                                                      */
/* Output(s): None                                                      */
/*                                                                      */
/* Return(s): None                                                      */
/*                                                                      */
/* Calls:       None                                                    */
/*                                                                      */
/* Tasks:     None                                                      */
/*                                                                      */
/* Errors:    None                                                      */
/*                                                                      */
/* Comments:  Does nothing for DOS systems. Restores original console   */
/*              settings on a unix system.                              */
/*                                                                      */
/* Revision:                                                            */
/* 0.1  00/08/18 RKP     Initial prototype                              */
/*                                                                      */
/************************************************************************/
void disable_os_io(void)
{
#if defined(__DOS__)
#else  /* Some type of unix. */

//  ioctl( io_fd, PCCONDISABIOPL, 0 );
//  close( io_fd );

  tcsetattr(Confd, TCSANOW, &OCondes);
#endif /* Some type of unix. */
}


/************************************************************************/
/*                       Test Specific Functions                        */
/************************************************************************/
void
setTimer(TIMERTYPE_ timerName, unsigned int timerValue)
{
    if (timerName < LAST_TIMER)
        TimerArray[timerName] = timerValue;
}


// If allTimers is TRUE the timerValue S.B. 0 and timerName S.B. the last timer to clear.  
void
clearTimer(TIMERTYPE_ timerName, unsigned int timerValue, unsigned int allTimers)
{
  unsigned int indx;

    if (allTimers == TRUE)
        for (indx = 0; indx <= timerName; indx++)
            TimerArray[timerName] = timerValue;

    else if (timerName < LAST_TIMER)
        TimerArray[timerName] = timerValue;
}


unsigned int
checkTimer(TIMERTYPE_ timerName)
{
  unsigned int indx;
  unsigned int TimerBusy = FALSE;

    if (timerName > LAST_TIMER)
        return(TIMER_IDLE);

    if (TimerDiff != clock() / CLOCKS_PER_SEC) // PC simulation of timer interrupt.
    {
        TimerDiff = clock() / CLOCKS_PER_SEC;  // Not needed in embedded firmware.

/************************************************************************/
/*     This code S.B. in the timer interrupt on embedded firmware.      */
       for (indx = 0; indx < LAST_TIMER; indx++)
        {
            if (TimerArray[indx] != TIMER_IDLE)
                TimerBusy = TRUE;
            if (TimerArray[indx] > TIMER_DONE)
                TimerArray[indx]--;
        }

        if (TimerBusy == FALSE)
        {
             if (SleepFlag == FALSE)
             {
                printf("\nGoing to sleep!!\n");
                SleepFlag = TRUE; // This is a great place for a breakpoint.
             }
        }
        else
        {
            SleepFlag = FALSE;
            printf(" Tick %d %d", timerName, TimerArray[timerName]);  // Debug
            fflush(stdout);  // Debug
        }
/*                          End of interrupt code                       */
/************************************************************************/
    }  // Not needed in embedded firmware.

    if (TimerArray[timerName] == TIMER_DONE)
    {
        TimerArray[timerName] = TIMER_IDLE;
        return(TIMER_DONE);
    }
    else
        return(TimerArray[timerName]);
}


void
BuzzToneGenerator(unsigned short tone, unsigned int ontime, unsigned int offtime, unsigned int count)
{
  unsigned int toneindx, countindx;

    for (countindx = 0; countindx < count; countindx++)
    {

        printf("\a Beep %d %d %d %d \n", tone, ontime, offtime, count); // PC simulation for debug            
//        setTimer(BUZZ_TIMER, ontime);
//        while ( checkTimer(BUZZ_TIMER) != TIMER_DONE )
//        {
//            BUZZER_TOGGLE;
            toneindx = 0;
            while (toneindx < tone)
            {  
                toneindx++;
            }
//        }
        setTimer(BUZZ_TIMER, offtime);
        while ( checkTimer(BUZZ_TIMER) != TIMER_DONE );
    }
}


/************************************************************************/
/*                       Codes Specific Functions                       */
/************************************************************************/
/*
 * Initialize the code array.
 */
void
InitCodeAt(unsigned int codepos)
{
        passCode[codepos].flag = CODE_VALID;
        passCode[codepos].code[0] = codepos; passCode[codepos].code[1] = codepos;
        passCode[codepos].code[2] = codepos; passCode[codepos].code[3] = codepos;
        passCode[codepos].code[4] = codepos; passCode[codepos].code[5] = codepos;
}


void
InitCodeArray(void)
{
    unsigned int indx;

    indx = 0;
    for (indx = 0; indx < MAX_NUM_OF_PASSCODES; indx++)
    {
        InitCodeAt(indx);
    }
}


void
InitAdminCode(void)
{
    adminCode.flag = CODE_VALID;
    adminCode.code[0] = '0';adminCode.code[1] = '1';adminCode.code[2] = '2';
    adminCode.code[3] = '3';adminCode.code[4] = '4';adminCode.code[5] = '5';
}


void
InitCodeBuffers(void)
{
  unsigned int indx;

    keyCodeIndx = 0;
    adminCodeIndx = 0;

    for (indx = 0; indx < MAX_KEY_CODE; indx++)
    {
        keyCode[indx] = 0;
        tmpAdminCode1[indx] = 0;
    }
}


void
InitDeviceState(void)
{

    InitCodeBuffers();
    selPassCodeIndx = 0;
    stateMachineState = STATEMACH_IDLE;
    clearTimer(LED_TIMER, 0, TRUE); // Set all But the LED_TIMER to idle.
}


/*
 * Compare buffers.
 */
unsigned char
CompareBuff(unsigned char *code1, unsigned char *code2 )
{
    unsigned int indx;

    for (indx = 0; indx < MAX_CODE_SIZE; indx++)
    {
        if (code1[indx] != code2[indx])
            return CODE_CMP_FAILED;
    }

    return CODE_CMP_SUCCESS;
}


/*
 * Compare codes.
 */
unsigned char
CompareCode(unsigned char *code, struct codestruct *lcode)
{
    unsigned int indx;

    /*
     * First check whether we have an admin code.
     */
    if (lcode->flag != CODE_VALID )
        return CODE_NOT_VALID;
    for (indx = 0; indx < MAX_CODE_SIZE; indx++)
    {
        if (code[indx] != lcode->code[indx])
            return CODE_CMP_FAILED;
    }

    return CODE_CMP_SUCCESS;
}


/*
 * Validate Code.
 */
unsigned char
ValidateCode(unsigned char *code)
{
    unsigned int indx;

    if (CompareCode(code, &adminCode) == CODE_CMP_SUCCESS)
    {
        return CODE_IS_ADMIN_CODE;
    }

    for (indx = 0; indx < MAX_NUM_OF_PASSCODES; indx++)
    {
        if (CompareCode(code, &passCode[indx]) == CODE_CMP_SUCCESS)
        {
            return CODE_IS_PASSCODE;
        }
    }

    return CODE_IS_UNKNOWN;
}


void
SaveAdminCode(unsigned char *code)
{
    unsigned char j;


    adminCode.flag = CODE_VALID;
    for (j = 0; j < MAX_CODE_SIZE; j++)
    {
        adminCode.code[j] = code[j];
    }
//    UpdateCode(code, savepos);
}


void
SavePasscodeAt(unsigned char *code, unsigned char savepos)
{
    unsigned char j;


    passCode[savepos].flag = CODE_VALID;
    for (j = 0; j < MAX_CODE_SIZE; j++)
    {
        passCode[savepos].code[j] = code[j];
    }
//    UpdateCode(code, savepos);
}


void unlockBusy(void)
{

//                     UNLOCK_HIGH;
                     setTimer(UN_LOCK_TIMER, UNLOCK_TICK_COUNT);
                     while ( checkTimer(UN_LOCK_TIMER) != TIMER_DONE );
//                     UNLOCK_LOW;

                     printf("\nUnlocking Door!!\n");
                     BuzzToneGenerator(250, BEEP_TIME_ON_RIGHT_CODE, BEEP_TIME_OFF_SPACE, 1);
                     BuzzToneGenerator(650, BEEP_TIME_ON_RIGHT_CODE, BEEP_TIME_OFF_SPACE, 1);


                     setTimer(UN_LOCK_TIMER, PUSH_BACK_COUNT);
                     while ( checkTimer(UN_LOCK_TIMER) != TIMER_DONE );

//                     LOCK_HIGH;
                     setTimer(UN_LOCK_TIMER, LOCK_TICK_COUNT);
                     while ( checkTimer(UN_LOCK_TIMER) != TIMER_DONE );
//                     LOCK_LOW;
}


/************************************************************************/
/*                            State Machine                             */
/*                            USE Cases                                 */
/* 1. Restore to Default Codes (All including admin):                   */
/*    A. Press the PROGEN button (*).                                   */
/*    B. Press the CLEAR button (-).                                    */
/* 2. Set an Admin Code:                                                */
/*    A. Press the PROGEN button.                                       */
/*    B. ENTER the old Admin code and press ENTER (+)                   */
/*    C. ENTER the new Admin code and press ENTER                       */
/*    D. RE-ENTER the new Admin code and ENTER                          */
/* 3. Delete all Access Codes in all positions:                         */
/*    A. ENTER the Admin code and press ENTER                           */
/*    B. Press the ENTER button.                                        */
/*    C. Press the ENTER button.                                        */
/* 4. Set an Access Code in a position:                                 */
/*    A. ENTER the Admin code and press ENTER                           */
/*    B. ENTER the AccessCode Position (0-9) and press ENTER            */
/*    C. ENTER the 6 digit AccessCode and press ENTER                   */
/* 5. Delete an Access Code in a position:                              */
/*    A. ENTER the Admin code and press ENTER                           */
/*    B. ENTER the AccessCode Position (0-9) and press ENTER            */
/*    C. Press the ENTER button.                                        */
/*                                                                      */
/* 6. Use a passCode:                                                   */
/*    A. ENTER a PassCode and press ENTER                               */
/*                                                                      */
/*                           Machine States                             */
/*                                                 STATEMACH_IDLE                                                               */
/*                    PROGEN                        CLEAR/ENTER                       NUMBER                                    */
/*                                                                                                                              */
/*           STATEMACH_GET_ADMIN_CODE           InitDeviceState/              STATEMACH_GET_PASSCODE                            */
/*                                                                                                                              */
/*       CLEAR            ENTER           NUMBER                          CLEAR         ENTER            NUMBER                 */
/*if (keyCodeIndx == 0)                 wait for 6                   InitDeviceState                   wait for 6               */
/*    InitAdminCode                     LOAD/CLEAR                                                     LOAD/CLEAR               */
/*InitDeviceState     If good Admin                                         If good PassCode          If good Admin             */
/*               STATEMACH_GOT_ADMIN_CODE                                     unlockBusy/                                       */
/*       CLEAR      ENTER      NUMBER (to tmpAdminCode1)                    InitDeviceState     STATEMACH_INDX_OR_CLEAR         */
/* InitDeviceState          wait for 6                                                        CLEAR      ENTER      NUMBER      */
/*                            LOAD/CLEAR                                             InitDeviceState/             wait for 1    */
/*        STATEMACH_ADMIN_CODE_RPT                                                                                  LOAD        */
/*       CLEAR      ENTER      NUMBER (to keyCode)                                              if (keyCodeIndx == 0)           */
/* InitDeviceState         wait for 6                                                       YES                       NO        */
/*                           LOAD/CLEAR                                         STATEMACH_CLEAR_CONFIRM                         */
/*   If (CompareBuff() == CODE_CMP_SUCCESS)                                     CLEAR/              ENTER                       */
/*       SavePasscodeAt(adminCode,)                                         InitDeviceState/     InitCodeArray                  */ 
/*   InitDeviceState                                                                             InitDeviceState/               */
/*                                                                                                      STATEMACH_PASS_CODE_GET */
/*                                                                                         CLEAR        ENTER        NUMBER     */
/*                                                                                  InitDeviceState/                 wait for 6 */
/*                                                                                                                   LOAD/CLEAR */
/*                                                                                      if (keyCodeIndx == 0)                   */
/*                                                                                  YES                       NO                */
/*                                                              InitCode(selPassCodeIndx)/   SavePasscodeAt(selPassCodeIndx)/   */
/*                                                                        InitDeviceState              InitDeviceState          */
/*                                                                                                                              */
/*                                                                      */
/************************************************************************/
void stateMach(unsigned char inpKey)
{
  unsigned char tempResult = 0;

    if (inpKey == TERM_CHAR_PROGEN) // Reset on out of sequence Programming Enable
        if (stateMachineState != STATEMACH_IDLE)
            InitDeviceState();

    switch(stateMachineState)
    {
    case   STATEMACH_IDLE:
        if (inpKey == TERM_CHAR_PROGEN)
        {
            stateMachineState = STATEMACH_GET_ADMIN_CODE;
        }
        else if ((inpKey == TERM_CHAR_CLEAR) || (inpKey == TERM_CHAR_ENTER))
            InitDeviceState();
        else
        {
            keyCode[keyCodeIndx++] = inpKey;
            stateMachineState = STATEMACH_GET_PASSCODE;
        }
        break;


    case   STATEMACH_GET_PASSCODE:
        if ((inpKey == TERM_CHAR_CLEAR) || (keyCodeIndx > MAX_CODE_SIZE))
            InitDeviceState();
        else if (inpKey == TERM_CHAR_ENTER)
        {
            tempResult = ValidateCode(keyCode);
            if (tempResult == CODE_IS_ADMIN_CODE)
            { 
                InitCodeBuffers(); 
                stateMachineState = STATEMACH_INDX_OR_CLEAR;
            }
            else if (tempResult == CODE_IS_PASSCODE)
            {
                unlockBusy();
                InitDeviceState();
            }
            else 
                InitDeviceState();
        }
        else
        {
            keyCode[keyCodeIndx++] = inpKey;
        }
        break;


    case   STATEMACH_GET_ADMIN_CODE:
        if ((inpKey == TERM_CHAR_CLEAR) || (keyCodeIndx > MAX_CODE_SIZE))
        {
            if (keyCodeIndx == 0)
                InitAdminCode();
            InitDeviceState();
        }
        else if (inpKey == TERM_CHAR_ENTER)
        {
            if (ValidateCode(keyCode) == CODE_IS_ADMIN_CODE)
            {   InitCodeArray();
                  InitCodeBuffers();
                stateMachineState = STATEMACH_GOT_ADMIN_CODE;
            }
        }
        else
        {
            keyCode[keyCodeIndx++] = inpKey;
        }
        break;


    case   STATEMACH_GOT_ADMIN_CODE:
        if ((inpKey == TERM_CHAR_CLEAR) || (keyCodeIndx > MAX_CODE_SIZE))
        {
            InitDeviceState();
        }
        else if (inpKey == TERM_CHAR_ENTER)
        {
             if (keyCodeIndx == MAX_CODE_SIZE)
             {
                stateMachineState = STATEMACH_ADMIN_CODE_RPT;
             }
        }
        else
        {
            keyCode[keyCodeIndx++] = inpKey;
        }
        break;


    case   STATEMACH_ADMIN_CODE_RPT:
        if ((inpKey == TERM_CHAR_CLEAR) || (adminCodeIndx > MAX_CODE_SIZE))
        {
            InitDeviceState();
        }
        else if (inpKey == TERM_CHAR_ENTER)
        {
            if (CompareBuff(keyCode, tmpAdminCode1) == CODE_CMP_SUCCESS)
                SaveAdminCode(keyCode);
            InitDeviceState();
        }
        else
        {
            tmpAdminCode1[adminCodeIndx++] = inpKey;
        }
        break;

 
    case   STATEMACH_INDX_OR_CLEAR:
        if (inpKey == TERM_CHAR_CLEAR)
        {
            InitDeviceState();
        }
        else if (inpKey == TERM_CHAR_ENTER)
        {
            if (keyCodeIndx == 0)
                stateMachineState =  STATEMACH_CLEAR_CONFIRM;
            else
            {
                    InitCodeBuffers();
                stateMachineState = STATEMACH_PASS_CODE_GET;
               }
        }
        else
        {
            selPassCodeIndx = inpKey - DECC;
            keyCodeIndx++;
            if (keyCodeIndx > 1)
                InitDeviceState();
        }
        break;


    case   STATEMACH_CLEAR_CONFIRM:
        if (inpKey == TERM_CHAR_ENTER)
        {
            if (keyCodeIndx == 0)
            {
                InitCodeArray();
            }
        }
        InitDeviceState();
        break;


    case   STATEMACH_PASS_CODE_GET:
        if ((inpKey == TERM_CHAR_CLEAR) || (keyCodeIndx > MAX_CODE_SIZE))
        {
            InitDeviceState();
        }
        else if (inpKey == TERM_CHAR_ENTER)
        {
            if (keyCodeIndx == 0)
                InitCodeAt(selPassCodeIndx);
            else 
                SavePasscodeAt(keyCode, selPassCodeIndx);
            InitDeviceState();
        }
        else
        {
            keyCode[keyCodeIndx++] = inpKey;
        }
        break;


    default:
        break;
    }

}


/************************************************************************/
/*                              Main Loop                               */
/************************************************************************/
int main(int argc, char *argv[])
{
  char inpchar[3];

    enable_os_io(); // Defeat console buffering for debug. Still may have to hit enter to fflush stdin.
    printf("\nProgram started..\n");

    //Initialization functions
    keyChanged = FALSE;
    InitCodeArray();
    InitAdminCode();
    InitDeviceState();
    clearTimer(LAST_TIMER, 0, TRUE); // Set all timers to idle.

    // Main loop to handle the State machine and all events.
    while(1)
    {

#if defined(__DOS__)
        if (kbhit() != 0)
        {
            inpchar[0] = (char)getch();
            keyChanged = TRUE;
        }
#else  /* Some type of unix. */
        if (read(Confd, inpchar, 1) != 0)
            keyChanged = TRUE;
#endif  /* Some type of unix. */

        if (keyChanged == TRUE) 
        {                       // 
            if ((inpchar[0] != TERM_CHAR_PROGEN) && 
                (inpchar[0] != TERM_CHAR_ENTER) && (inpchar[0] != TERM_CHAR_CLEAR) &&
                ((inpchar[0] < DECC) || (inpchar[0] > MAXDEC))) // If it's not a valid key
                keyChanged = FALSE;
//             BuzzToneGenerator(250, BEEP_TIME_ON_RIGHT_CODE, BEEP_TIME_OFF_SPACE, 1); // Debug timer/Buzzer.
        }

        if (keyChanged == TRUE)
        {
            printf("\n");
            setTimer(LED_TIMER, LED_TURNOFF_COUNT);
            lastKeyPressed = inpchar[0];
            printf("LastKeyPressed - 0x%02x\n", inpchar[0]);

            stateMach(lastKeyPressed);
            keyChanged = FALSE;
            printf("Machine State is %d\n", stateMachineState);
        }

        if (checkTimer(LED_TIMER) == TIMER_DONE)
            InitDeviceState(); // This clears state if the unit goes to sleep.
    }

    return(0);
}


