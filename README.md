/* Short PC Test program for a combination lock state machine:	    	*/
/*                                                                      */
/* Purpose:             Event processing state machine monitors keys	*/
/*			and internal status and controls the lock.	*/
/*                                                                      */
/* Operating System:    None Linux gcc.      				*/
/* Compiler:            Geaney and gcc					*/
/* Model:               Assume Flat 16 bit            			*/
/*                                                            		*/
/* Comments:            Additional support for an Administrator password*/
/*                      to add and change passcodes. A backdoor button  */
/*                      to allow editing the Administrator passsword.	*/
/*                      						*/
/*                      The state machine and supporting functions can	*/
/*                      be added to embedded firmware that scans keys on*/
/*                      a numerical keypad with enter and clear, has a	*/
/*                      hardware timer interrupt, and lock driver.	*/
/*                      						*/
/*                      Code in the main loop or timer interrupt should */
/*                      InitDeviceState(); to clear state if the unit	*/
/*			times out and goes to sleep.			*/


/*                            State Machine                             */
/*                            USE Cases                                 */
/* 1. Restore to Default Codes (All including admin):                   */
/*    A. Press the PROGEN button (*).                                   */
/*    B. Press the CLEAR button (-).                                    */
/*                                                                      */
/* 2. Set an Admin Code:                                                */
/*    A. Press the PROGEN button.                                       */
/*    B. ENTER the old Admin code and press ENTER (+)                   */
/*    C. ENTER the new Admin code and press ENTER                       */
/*    D. RE-ENTER the new Admin code and ENTER                          */
/*                                                                      */
/* 3. Delete all Access Codes in all positions:                         */
/*    A. ENTER the Admin code and press ENTER                           */
/*    B. Press the ENTER button.                                        */
/*    C. Press the ENTER button.                                        */
/*                                                                      */
/* 4. Set an Access Code in a position:                                 */
/*    A. ENTER the Admin code and press ENTER                           */
/*    B. ENTER the AccessCode Position (0-9) and press ENTER            */
/*    C. ENTER the 6 digit AccessCode and press ENTER                   */
/*                                                                      */
/* 5. Delete an Access Code in a position:                              */
/*    A. ENTER the Admin code and press ENTER                           */
/*    B. ENTER the AccessCode Position (0-9) and press ENTER            */
/*    C. Press the ENTER button.                                        */
/*                                                                      */
/*                                                                      */
/* 6. Use a passCode:                                                   */
/*    A. ENTER a PassCode and press ENTER                               */
/*                                                                      */

