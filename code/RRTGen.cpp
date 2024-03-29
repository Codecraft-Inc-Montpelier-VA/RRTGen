// RRTGen -- a repeatable random test generator framework.
//
//                                             <by Cary WR Campbell>
//
// This software is copyrighted � 2007 - 2015 by Codecraft, Inc.
//
// The following terms apply to all files associated with the software
// unless explicitly disclaimed in individual files.
//
// The authors hereby grant permission to use, copy, modify, distribute,
// and license this software and its documentation for any purpose, provided
// that existing copyright notices are retained in all copies and that this
// notice is included verbatim in any distributions. No written agreement,
// license, or royalty fee is required for any of the authorized uses.
// Modifications to this software may be copyrighted by their authors
// and need not follow the licensing terms described here, provided that
// the new terms are clearly indicated on the first page of each file where
// they apply.
//
// IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
// FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
// ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
// DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
// IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
// NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
// MODIFICATIONS.
//
// GOVERNMENT USE: If you are acquiring this software on behalf of the
// U.S. government, the Government shall have only "Restricted Rights"
// in the software and related documentation as defined in the Federal
// Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
// are acquiring the software on behalf of the Department of Defense, the
// software shall be classified as "Commercial Computer Software" and the
// Government shall have only "Restricted Rights" as defined in Clause
// 252.227-7014 (b) (3) of DFARs.  Notwithstanding the foregoing, the
// authors grant the U.S. Government and others acting in its behalf
// permission to use and distribute the software in accordance with the
// terms specified in this license.

/******************************* Configuration ********************************/
/* Uncomment the following define line for a histogram of coroutine           **
** round-trip latencies.                                                      */
#define SHOW_HISTOGRAM
/* Uncomment the following define line(s) for a display of event details.     */
//#define SHOW_EVENT_ENVELOPE
//#define SHOW_EVENT_PROCESSING
/******************************************************************************/

#include <iostream>
#include <stdlib.h>
#include <stdio.h>                     // getchar
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <typeinfo>                    // typeid (for Cygwin)
#include "histospt.h"                  // histogram support
#include "RRTGenClasses.h"             // must follow EVTModel_Insert.h
#include "RRTGen.h"
#include "sccorlib.h"

using namespace std;

// Local prototypes.
void doSendDelayedEvent( int msToWait, event theEvent, long parm1, long parm2,
                         FSM *to, FSM *from, bool *continuing, bool *canceling ) ;
void doSendDelayedEventToId( int msToWait, event theEvent, long parm1,
                             long parm2, Id *to, FSM *from, 
                             bool *continuing, bool *canceling ) ;

// Variables defined elsewhere.
extern int currentNumberOfModelInstances ;
extern FSM **pModelComponentArray ;

// Global variables.
unsigned int      alertIssued ;
char              alert_list[ 2 * MAX_ALERT_MESSAGES + 1 ] ; // with spaces, nul
int               alertMessageIndex ;
char              *alertMessages[ MAX_ALERT_MESSAGES ] ;
FSM               *broadcast = 0 ;
bool              configuring ;         // test is in configuration phase
unsigned int      mostRecentResponseTime = 0 ;
unsigned int      mostRecentCommandTime = 0 ;
bool              stopping ;            // test is in stopping phase
unsigned int      TOTAL_REQ_COUNT = 0 ; // updated in Gen_Commands_Model::AVTModel_1200
Fifo              theEventQ( "theEventQ" ) ;
Fifo              theEventSelfDirectedQ( "theEventSelfDirectedQ" ) ;
#ifdef SHOW_HISTOGRAM
 TimeIntervalHistogram itHistCR( "RRTGen coresume round-trip times (us)",
                                  0L, 250, 40 ) ; // 0 - 10 ms
#endif // def SHOW_HISTOGRAM
notificationFifo  nFifo ;
char              roundtripCounterOutputString[ 80 ] ; // ample
int               score ;
bool              testing ;             // primary flag that test is still active

// Global objects for use elsewhere.
FSM  *DEVICE_INTERFACE      = (FSM *)0x41414141 ;   // just a unique value
FSM  *MODEL_INTERFACE       = (FSM *)0x42424242 ;   // a second unique value
Fifo *pModelNormalQueue     = &theEventQ ;          // defensively; actual values
Fifo *pModelPriorityQueue   = &theEventSelfDirectedQ ; //  are set in ModelThread
FSM  *RRTGEN_FRAMEWORK      = (FSM *)0x43434343 ;   // another unique value
FSM  *sender                = 0 ;                   //
 
////////////////////////////////////////////////////////////////////////////////
//
// addAlertMessage
//
// This utility adds an ALERT message to the alert message array for output
// at the end of the test.
//
// As a side-effect, the alertIssued global variable is set to a prime
// representing the message's "index" relative to 'A' (i.e., for alertB
// => alertIssued % 3 is 0).
//
// Only the first MAX_ALERT_MESSAGES unique messages are stored.
//

void addAlertMessage( const char messageLetter ) {
   short i ;
   bool  messageFound = false ; // assume not included yet
   char  *pMessage = getAlertMessagePtr( messageLetter ) ;
   if ( alertMessageIndex < MAX_ALERT_MESSAGES ) {
      for ( i = 0; i < alertMessageIndex; i++ ) {
         if ( alertMessages[ i ] == pMessage ) {
            messageFound = true ;
            break ;
         }
      }
      if ( !messageFound ) {
         alertMessages[ alertMessageIndex++ ] = (char *)pMessage ;
      }

      // Note that alertIssued was initialized to a default value in
      // the test presentation component. We check here to see if this alert
      // has already been reported since the last strip chart line was
      // printed; if it has already been reported, we'll just skip it now.

      if ( pMessage == alertA && alertIssued % 2 ) {
         alertIssued *= 2 ;
      } else if ( pMessage == alertB && alertIssued % 3 ) {
         alertIssued *= 3 ;
      } else if ( pMessage == alertC && alertIssued % 5 ) {
         alertIssued *= 5 ;
      } else if ( pMessage == alertD && alertIssued % 7 ) {
         alertIssued *= 7 ;
      } else if ( pMessage == alertE && alertIssued % 11 ) {
         alertIssued *= 11 ;
      } else if ( pMessage == alertF && alertIssued % 13 ) {
         alertIssued *= 13 ;
      } else if ( pMessage == alertG && alertIssued % 17 ) {
         alertIssued *= 17 ;
      } else if ( pMessage == alertH && alertIssued % 19 ) {
         alertIssued *= 19 ;
      } else if ( pMessage == alertI && alertIssued % 23 ) {
         alertIssued *= 23 ;
      } else if ( pMessage == alertJ && alertIssued % 29 ) {
         alertIssued *= 29 ;
      } else if ( pMessage == alertK && alertIssued % 31 ) {
         alertIssued *= 31 ;
      } else if ( pMessage == alertL && alertIssued % 37 ) {
         alertIssued *= 37 ;
      } else if ( pMessage == alertM && alertIssued % 41 ) {
         alertIssued *= 41 ;
      } else if ( pMessage == alertN && alertIssued % 43 ) {
         alertIssued *= 43 ;
      } else if ( pMessage == alertO && alertIssued % 47 ) {
         alertIssued *= 47 ;
      } else if ( pMessage == alertP && alertIssued % 53 ) {
         alertIssued *= 53 ;
      } else if ( pMessage == alertQ && alertIssued % 59 ) {
         alertIssued *= 59 ;
      } else if ( pMessage == alertR && alertIssued % 61 ) {
         alertIssued *= 61 ;
      } else if ( pMessage == alertS && alertIssued % 67 ) {
         alertIssued *= 67 ;
      } else if ( pMessage == alertT && alertIssued % 71 ) {
         alertIssued *= 71 ;
      } else if ( pMessage == alertU && alertIssued % 73 ) {
         alertIssued *= 73 ;
      }
#if 0
     //     :      :     :     :   `
      else if (alertIssued % 101) {
         alertIssued *= 101; // default is 'Z' (101 is the 26th prime)
      }
#endif // 0
   }
}

////////////////////////////////////////////////////////////////////////////////
//
// addModelInstance
//
// This utility adds a non-zero entry to theModel array.
//
// A return of false indicates the instance value is zero or that there is
// no room for the addition.  In either case, an error message is generated
// and the test is stopped by setting the global testing flag to false.
//

bool addModelInstance( FSM *modelInstance ) {
   bool brc = true ; // assume success
   if ( currentNumberOfModelInstances >= ( MAX_NUMBER_OF_MODEL_INSTANCES - 1 ) ) {
      char str[ 80 ] ; // ample
      sprintf( str, "No room to add model instance '%s' to theModel.",
               modelInstance->getName() ) ;
      alert( str ) ;
      testing = false ;
      brc = false ;
   } else if ( modelInstance == 0 ) {
      char str[ 80 ] ; // ample
      sprintf( str, "The model instance passed to AddModelInstance was 0." ) ;
      alert( str ) ;
      testing = false ;
      brc = false ;
   } else {
      theModel[ currentNumberOfModelInstances++ ] = modelInstance ;

      // Mark the end of the initial instances.
      theModel[ currentNumberOfModelInstances ] = 0 ; // terminal nul
   }
   return brc ;
}

////////////////////////////////////////////////////////////////////////////////
//
// chronicleTestRun
//
// This utility appends an entry with this test's run information to the
// test run summary .csv file.
//

int chronicleTestRun( const char testRunsCsvFile[],
                      const char sut[], const char machine[],
                      int testNumber, unsigned int sequenceNumber,
                      int score, bool passed, const char alerts[] ) {
   const int eStrLen = 80 ;
   char      str[ eStrLen + 80 ] ; // ample
   char      eStr[ eStrLen ] ; // should be ample
   int       rc = SUCCESS ; // optimistically
   const int MAX_READ_BUFFER_SIZE = sizeof str ;
   FILE      *fp ;

   if ( ( fp = fopen( testRunsCsvFile, "a+" ) ) ) {
      // Get the current file size.
      int result = fseek( fp, 0L, SEEK_END ) ;
      if ( result ) {
         strerror_r( errno, eStr, eStrLen ) ;
         sprintf( str, "Unable to seek in test run summary file %s: %s",
                  testRunsCsvFile, eStr ) ;
         alert( str ) ;
         rc = COMMAND_FAILED ;
      } else {
         int position = ftell( fp ) ;
         if ( !position ) {
            // Add the header if the file is empty.
            fprintf( fp, "SUT,Machine,Test Number,Log File Number,Score,Pass/Fail,"
                     "Alerts,Date,Time\n" ) ;
         }
         // Now write our test run summary to the file.
         time_t theTime = time( NULL ) ;
         struct tm *timePieces = localtime( &theTime ) ;
         char dbuf[ 16 ] ; // ample
         sprintf( dbuf, "%i/%02i/%02i", timePieces->tm_mon + 1, // 0-indexed
                  timePieces->tm_mday, timePieces->tm_year - 100 ) ; // from 1900
         char tbuf[ 16 ] ; // ample
         sprintf( tbuf, "%02i:%02i:%02i", timePieces->tm_hour,
                  timePieces->tm_min, timePieces->tm_sec ) ;
         int bytesWritten = fprintf( fp, "%s,%s,%5i,%5i,%6i,%s,%s,%s,%s\n",
                                     sut, machine, testNumber, sequenceNumber,
                                     score, passed ? "Pass" : "Fail", alerts,
                                     dbuf, tbuf ) ;
         if ( !bytesWritten ) {
            // An error occurred attempting to append to the file.
            strerror_r( errno, eStr, eStrLen ) ;
            sprintf( str, "Unable to write test run summary file %s: %s",
                     testRunsCsvFile, eStr ) ;
            alert( str ) ;
            rc = COMMAND_FAILED ;
         }
      }

      fclose( fp ) ;
   } else {
      // Open operation failed.
      strerror_r( errno, eStr, eStrLen ) ;
      sprintf( str, "Unable to open test run summary file %s: %s",
               testRunsCsvFile, eStr ) ;
      alert( str ) ;
      rc = COMMAND_FAILED ;
   }

   return rc ;
}

////////////////////////////////////////////////////////////////////////////////
//
// dashTrailing
//
// This utility removes whitespace from the end of a string.
//
void dashTrailing( char *str ) {
   for ( int i = strlen( str ) - 1; i > 0; i-- ) {
      if ( str[ i ] == ' ' || str[ i ] == '\t' ) {
         str[ i ] = '\0' ;
      } else {
         return ;
      }
   }
}

////////////////////////////////////////////////////////////////////////////////
//
// doSendDelayedEvent
//
// This utility waits a specified time and sends an event to the model's
// normal queue.
//
// Note that the calling coroutine is blocked while waiting for the event
// to be sent.
//
// This utility is intended to be called from SendDelayedEvent, which provides
// necessary checking and will not block the caller's coroutine.
//

void doSendDelayedEvent( int msToWait, event theEvent, long parm1, long parm2,
                         FSM *to, FSM *from, bool *continuing, bool *canceling ) {
   // We'll wait the requested time and then queue the event (without checking)
   // in the normal queue.
   waitEx( msToWait, continuing, canceling ) ;
   if ( canceling == NULL || *canceling == false ) {
      sendEvent( theEvent, parm1, parm2, to, from ) ;
   }
}

void doSendDelayedEventToId( int msToWait, event theEvent, long parm1,
                             long parm2, Id *to, FSM *from, 
                             bool *continuing, bool *canceling ) {
   // We'll wait the requested time and then queue the event (without checking)
   // in the normal queue.
   waitEx( msToWait, continuing, canceling ) ;
   if ( canceling == NULL || *canceling == false ) {
      sendEventToId( theEvent, parm1, parm2, to, from ) ;
   }
}

////////////////////////////////////////////////////////////////////////////////
//
// enterCriticalSection
//
// This utility marks the start of a critical section.  Only one coroutine
// at a time is allowed to execute code within a critical section.
//
// The parameter is a critical section control variable, which should be
// initialized to zero before making any calls to this routine.
//

void enterCriticalSection( unsigned short *csVar ) {
   when ( *csVar == 0 ) {
      ++*csVar ;
   }
}

#if 0
////////////////////////////////////////////////////////////////////////////////
//
// ExtractVersionInformation
//
// Internal routine to get the major and minor revision numbers for a module.
//

bool  ExtractVersionInformation(char *pathBuffer,
                                 unsigned char &minFileRev,
                                 unsigned char &majFileRev,
                                 unsigned char &minProductRev,
                                 unsigned char &majProductRev,
                                 unsigned char &unusedProductRev,
                                 unsigned char &buildNumProductRev) {
   bool brc = true;                    // assume success
   DWORD verHnd = 0;                   // 'ignored' parameter

   DWORD verInfoSize = GetFileVersionInfoSize(pathBuffer, &verHnd);
   if (!verInfoSize) {
      brc = false;
   } else {
      LPSTR vffInfo;
      LPSTR pFileVersion;
      LPSTR pProductVersion;
      HANDLE hMem;
      UINT  fileVersionLen;
      UINT  productVersionLen;
      hMem = GlobalAlloc(GMEM_MOVEABLE, verInfoSize);
      vffInfo = (char *)GlobalLock(hMem);
      if (!GetFileVersionInfo(pathBuffer, verHnd, verInfoSize, vffInfo)) {
         brc = false;
      } else {
         char textStrFile[]    = "\\StringFileInfo\\040904B0\\FileVersion";
         char textStrProduct[] = "\\StringFileInfo\\040904B0\\ProductVersion";
         BOOL bRetCode1 = VerQueryValue((LPVOID)vffInfo,
                                        TEXT(textStrFile),
                                        (LPVOID *)&pFileVersion,
                                        &fileVersionLen);
         BOOL bRetCode2 = VerQueryValue((LPVOID)vffInfo,
                                        TEXT(textStrProduct),
                                        (LPVOID *)&pProductVersion,
                                        &productVersionLen);
         if (bRetCode1 && bRetCode2) {
            // Establish string and get the integer part (first token)
            // for the file.
            char *token = strtok(pFileVersion, ".");
            if (token == NULL) {
               minFileRev = 0;
               majFileRev = 0;
            } else {
               majFileRev = unsigned char(atoi(token));

               // Get the fractional part (next token) for the file.
               token = strtok(NULL, ".");
               if (token == NULL) {
                  minFileRev = 0;
               } else {
                  minFileRev = unsigned char(atoi(token));
               }
            }

            // Establish string and get the integer part (first token)
            // for the product.
            token = strtok(pProductVersion, ".");
            if (token == NULL) {
               minProductRev = 0;
               majProductRev = 0;
               unusedProductRev = 0;
               buildNumProductRev = 0;
            } else {
               majProductRev = unsigned char(atoi(token));

               // Get the fractional part (next token) for the product.
               token = strtok(NULL, ".");
               if (token == NULL) {
                  minProductRev = 0;
                  unusedProductRev = 0;
                  buildNumProductRev = 0;
               } else {
                  minProductRev = unsigned char(atoi(token));

                  // Get the unused part (next token) for the product.
                  token = strtok(NULL, ".");
                  if (token == NULL) {
                     unusedProductRev = 0;
                     buildNumProductRev = 0;
                  } else {
                     unusedProductRev = unsigned char(atoi(token));

                     // Get the build number (next token) for the product.
                     token = strtok(NULL, ".");
                     if (token == NULL) {
                        buildNumProductRev = 0;
                     } else {
                        buildNumProductRev = unsigned char(atoi(token));
                     }
                  }
               }
            }
         } else {
            brc = false;
         }
      }
   }

   return brc;
}
#endif // 0

////////////////////////////////////////////////////////////////////////////////
//
// getAlertMessagePtr
//
// This utility gets a corresponding alert message pointer from an alert letter.
//

char *getAlertMessagePtr( const char messageLetter ) {
   char *pStr ;

   switch ( toupper( messageLetter ) ) {
      case 'A':
         pStr = (char *)alertA ;
         break ;

      case 'B':
         pStr = (char *)alertB ;
         break ;

      case 'C':
         pStr = (char *)alertC ;
         break ;

      case 'D':
         pStr = (char *)alertD ;
         break ;

      case 'E':
         pStr = (char *)alertE ;
         break ;

      case 'F':
         pStr = (char *)alertF ;
         break ;

      case 'G':
         pStr = (char *)alertG ;
         break ;

      case 'H':
         pStr = (char *)alertH ;
         break ;

      case 'I':
         pStr = (char *)alertI ;
         break ;

      case 'J':
         pStr = (char *)alertJ ;
         break ;

      case 'K':
         pStr = (char *)alertK ;
         break ;

      case 'L':
         pStr = (char *)alertL ;
         break ;

      case 'M':
         pStr = (char *)alertM ;
         break ;

      case 'N':
         pStr = (char *)alertN ;
         break ;

      case 'O':
         pStr = (char *)alertO ;
         break ;

      case 'P':
         pStr = (char *)alertP ;
         break ;

      case 'Q':
         pStr = (char *)alertQ ;
         break ;

      case 'R':
         pStr = (char *)alertR ;
         break ; 

      case 'S':
         pStr = (char *)alertS ;
         break ;

      case 'T':
         pStr = (char *)alertT ;
         break ;

      case 'U':
         pStr = (char *)alertU ;
         break ;

      default:
         pStr = (char *)alertZ ;
         break ;
   }

   return pStr ;
}

#if 0
////////////////////////////////////////////////////////////////////////////////
//
// GetTestVersionInfo
//
// This utility gets version information for this module.
//

bool  GetTestVersionInfo(unsigned char &minorProductRev,       // ByRef
                          unsigned char &majorProductRev,       // ByRef
                          unsigned char &unusedProductRev,      // ByRef
                          unsigned char &buildNumProductRev) {  // ByRef
   bool brc = true; // assume success
   char pathBuffer[MAX_PATH];
   unsigned char minorFileRev;    // value is not used
   unsigned char majorFileRev;    // value is not used

   // Get the product version for this module.
   HMODULE hModule = GetModuleHandle(OUR_MODULE_NAME);
   if (hModule == NULL) {
      brc = false;
   }

   if (brc && !GetModuleFileName(hModule, pathBuffer, sizeof pathBuffer)) {
      brc = false;
   } else if (!ExtractVersionInformation(pathBuffer,
                                         minorFileRev, majorFileRev,
                                         minorProductRev, majorProductRev,
                                         unusedProductRev,
                                         buildNumProductRev)) {
      brc = false;
   }

   return brc;
}
#endif // 0

////////////////////////////////////////////////////////////////////////////////
//
// getTimeInMs
//
// This utility returns the system time in ms.
//

unsigned int getTimeInMs( void ) {
   struct timeval tv ;

   gettimeofday( &tv, (struct timezone *)NULL ) ;
   return tv.tv_sec * 1000 + tv.tv_usec / 1000 ;
}

////////////////////////////////////////////////////////////////////////////////
//
// leaveCriticalSection
//
// This utility marks the end of a critical section.
//
// The parameter is a critical section control variable.
//
// NB: **??** TBD The following statement is not yet implemented. **??**
// Multiple EnterCriticalSection calls may be executed in sequence for the same
// critical section, but, overall, the EnterCriticalSection and
// LeaveCriticalSection calls must be balanced.
//

void leaveCriticalSection( unsigned short *csVar ) {
   --*csVar ;
}

////////////////////////////////// Coroutine ///////////////////////////////////
//
// modelThread
//
// This coroutine is a "postmaster" that hands events to the FSM model
// components when their events arrive in the model's normal or self-directed
// queues.
//
// The FSM model array parameter points to an array of FSM pointers. The array
// is NULL-terminated.  Events are handed to all FSM's if an event's
// destination is NULL (0).
//

void modelThread( Fifo *normalQ, Fifo *priorityQ, FSM **pFsmArray,
                   char *modelName ) {
   event theEvent ;
   FSM   *sender ;
   FSM   *destination ;
   FSM   **ppFsm ;
   long  parm1 ;
   long  parm2 ;
   long  rc ;
   char  str[ 120 ] ; // ample

   sprintf( str, "%s model started.", modelName ) ;
   RRTLog( str ) ;

   // Share the model's queue address with the external world for sending
   // external events.
   pModelNormalQueue   = normalQ ;
   pModelPriorityQueue = priorityQ ;

   // Share the model's list of components with the external world.
   pModelComponentArray = pFsmArray ;
   // Get everything started by broadcasting the START event. FSM's should be
   // initialized to be in a state to receive and process this START event.
   sendEvent( START, ' ', ' ', broadcast, RRTGEN_FRAMEWORK ) ;

   while ( testing ) {
      // Check our queues for any event.
      when( !testing || !( normalQ->isEmpty() ) || !( priorityQ->isEmpty() ) ) {
         if ( !testing ) {
            break ;
         }

         // Handle all self-directed events in the priority queue.
         while ( testing && !( priorityQ->isEmpty() ) ) {
            if ( ( rc = priorityQ->popEvent( theEvent, parm1, parm2,
                                             &sender, &destination ) )
                                                               != FIFO_SUCCESS ) {
               if ( rc == OVERRUN_OCCURRED ) {
                  sprintf( str, "Overrun occurred in %s.",
                           priorityQ->getName() ) ;
               } else {
                  sprintf( str, "Unknown error %li in %s.", rc,
                           priorityQ->getName() ) ;
               }
               alert( str ) ;
               score += fatalDiscrepancyFound ;
               testing = false ;
               break ;
            } else {
               // The self-directed queue should only contain events from
               // the sender to herself.  We'll complain, but continue.
               if ( sender != destination ) {
                  // NB: An error in gcc causes it to destroy sender and
                  // destination unless we separate the text strings.
                  char sendName[ 50 ] ; // ample
                  char destName[ 50 ] ; // ample
                  char pqName[ 50 ] ;   // ample
                  if ( destination ) {
                     strcpy( destName, destination->getName() ) ;
                  } else {
                     strcpy( destName, "?" ) ;
                  }
                  if ( sender ) {
                     strcpy( sendName, sender->getName() ) ;
                  } else {
                     strcpy( sendName, "?" ) ;
                  }
                  strcpy( pqName, priorityQ->getName() ) ;
                  sprintf( str, "Sender '%s' doesn't match the destination "
                           "'%s' in %s.", sendName, destName, pqName ) ;
                  alert( str ) ;
                  score += modelDiscrepancyFound ;
               }
        
               // Just the destination component gets to see it.
               #if defined(SHOW_EVENT_ENVELOPE)
               {
                char destName[ 50 ] ; // ample
                int  destInstance ;
                if (destination) {
                   strcpy( destName, destination->getName() ) ;
                   destInstance = destination->getInstance() ;
                } else {
                   strcpy( destName, "?" ) ;
                   destInstance = 0 ;
                }
                sprintf( str, " >>> SD event '%s(%ld,%ld)' sent to '%s' "
                         "(instance %i).", eventText( theEvent ), parm1, parm2,
                         destName, destInstance ) ;
                cout << str << endl;
                RRTLog( str ) ;
               }
               #endif // defined(SHOW_EVENT_ENVELOPE)
               destination->update( theEvent, parm1, parm2 ) ;
            }
         }

         // Handle one ordinary event in the normal queue.
         if ( testing && !( normalQ->isEmpty() ) ) {
            if ( ( rc = normalQ->popEvent( theEvent, parm1, parm2,
                                           &sender, &destination ) )
                                                               != FIFO_SUCCESS ) {
               if ( rc == OVERRUN_OCCURRED ) {
                  sprintf( str, "Overrun occurred in %s.",
                           normalQ->getName() ) ;
               } else {
                  sprintf( str, "Unknown error %li in %s.", rc,
                           normalQ->getName() ) ;
               }
               alert( str ) ;
               score += fatalDiscrepancyFound ;
               testing = false ;
               break ;
            } else if ( destination == broadcast ) {
               // Everybody gets to see it.
               ppFsm = pFsmArray ;
               do {
                  #if defined(SHOW_EVENT_ENVELOPE)
                  {
                     // NB: An error in gcc causes it to destroy sender,
                     //     destination, parm1, and parm2 unless we separate
                     //     the text strings (instead of using ?: in the
                     //     sprintf).
                     char  sendName[ 50 ] ; // ample
                     int   sendInstance ;
                     char  destName[ 50 ] ; // ample
                     int   destInstance ;
                     if ( *ppFsm ) {
                        strcpy( destName, (*ppFsm)->getName() ) ;
                        destInstance = (*ppFsm)->getInstance() ;
                     } else {
                        strcpy( destName, "?" ) ;
                        destInstance = 0 ;
                     }
                     if ( sender == DEVICE_INTERFACE ) {
                        strcpy( sendName, DEVICE_INTERFACE_NAME ) ;
                     } else if ( sender == MODEL_INTERFACE ) {
                        strcpy( sendName, MODEL_INTERFACE_NAME ) ;
                     } else if ( sender == RRTGEN_FRAMEWORK ) {
                        strcpy( sendName, RRTGEN_FRAMEWORK_NAME ) ;
                     } else if ( sender ) {
                        strcpy( sendName, sender->getName() ) ;
                        sendInstance = sender->getInstance() ;
                     } else {
                        strcpy( sendName, "?" ) ;
                        sendInstance = 0 ;
                     }
                     char *pet = eventText( theEvent ) ;
                     if ( sender == DEVICE_INTERFACE
                       || sender == MODEL_INTERFACE
                       || sender == RRTGEN_FRAMEWORK ) {
                        sprintf( str, " >>> Event '%s(0x%16lx,0x%16lx)' sent to "
                                 "'%s' (%i) from %s.", pet, parm1, parm2,
                                 destName, destInstance, sendName ) ;
                     } else {
                        sprintf( str, " >>> Event '%s(0x%16lx,0x%16lx)' sent to "
                                      "'%s' (%i) from '%s' (%i).", pet,
                                 parm1, parm2, destName, destInstance,
                                 sendName, sendInstance ) ;
                     }
                     cout << str << endl;
                     RRTLog( str ) ;
                  }
                  #endif // defined(SHOW_EVENT_ENVELOPE)
                  (*ppFsm)->update( theEvent, parm1, parm2 ) ;
               } while ( *(++ppFsm) ) ;
            } else {
               #if 0
               {
                  sprintf(str, "theEvent (before SHOW_EVENT_ENVELOPE): 0x%02x",
                           theEvent);
                  cout << str << endl;
                  sprintf(str, "sender (before SHOW_EVENT_ENVELOPE): 0x%02x",
                           sender);
                  cout << str << endl;
                  sprintf(str, "destination (before SHOW_EVENT_ENVELOPE): 0x%02x",
                           destination);
                  cout << str << endl;
                  sprintf(str, "ppFsm (before SHOW_EVENT_ENVELOPE): 0x%02x",
                           ppFsm);
                  cout << str << endl;
                  sprintf(str, "parm1 (before SHOW_EVENT_ENVELOPE): 0x%02x",
                           parm1);
                  cout << str << endl;
                  sprintf(str, "parm2 (before SHOW_EVENT_ENVELOPE): 0x%02x",
                           parm2);
                  cout << str << endl;
               }
               #endif // 0

               // Just the destination component gets to see it.
               #if defined(SHOW_EVENT_ENVELOPE)
               {
                  // NB: An error in gcc causes it to destroy sender,
                  // destination, parm1, and parm2 unless we separate the
                  // text strings.
                  char  sendName[ 50 ] ; // ample
                  int   sendInstance ;
                  char  destName[ 50 ] ; // ample
                  int   destInstance ;
                  if ( destination ) {
                     strcpy( destName, destination->getName() ) ;
                     destInstance = destination->getInstance() ;
                  } else {
                     strcpy( destName, "?" ) ;
                     destInstance = 0 ;
                  }
                  if ( sender == DEVICE_INTERFACE ) {
                     strcpy( sendName, DEVICE_INTERFACE_NAME ) ;
                  } else if ( sender == MODEL_INTERFACE ) {
                     strcpy( sendName, MODEL_INTERFACE_NAME ) ;
                  } else if ( sender == RRTGEN_FRAMEWORK ) {
                     strcpy( sendName, RRTGEN_FRAMEWORK_NAME ) ;
                  } else if ( sender ) {
                     strcpy( sendName, sender->getName() ) ;
                     sendInstance = sender->getInstance() ;
                  } else {
                     strcpy( sendName, "?" ) ;
                     sendInstance = 0 ;
                  }
                  char *pet = eventText( theEvent ) ;
                  if ( sender == DEVICE_INTERFACE
                    || sender == MODEL_INTERFACE
                    || sender == RRTGEN_FRAMEWORK ) {
                     sprintf( str, " >>> Event '%s(0x%16lx,0x%16lx)' sent to '%s'"
                              " (%i) from %s.", pet, parm1, parm2,
                              destName, destInstance, sendName ) ;
                  } else {
                     sprintf( str, " >>> Event '%s(0x%16lx,0x%16lx)' sent to '%s'"
                              " (%i) from '%s' (%i).", pet, parm1, parm2,
                              destName, destInstance, sendName, sendInstance ) ;
                  }
                  cout << str << endl ;
                  RRTLog( str ) ;
               }
               #endif // defined(SHOW_EVENT_ENVELOPE)
               #if 0
               cout << "Before update call in ModelThread." << endl;
               {
                  sprintf(str, "theEvent: 0x%02x", theEvent);
                  cout << str << endl;
                  sprintf(str, "sender: 0x%02x", sender);
                  cout << str << endl;
                  sprintf(str, "destination: 0x%02x", destination);
                  cout << str << endl;
                  sprintf(str, "ppFsm: 0x%02x", ppFsm);
                  cout << str << endl;
                  sprintf(str, "parm1: 0x%02x", parm1);
                  cout << str << endl;
                  sprintf(str, "parm2: 0x%02x", parm2);
                  cout << str << endl;
               }
               #endif // 0
               destination->update( theEvent, parm1, parm2 ) ;
            }
         }
      }
   }
   sprintf( str, "%s model ended.", modelName ) ;
   RRTLog( str ) ;
}

////////////////////////////////////////////////////////////////////////////////
//
// notificationHandler
//
// This is our notification callback handler. It handles all messages sent from
// the DUT. Notifications are queued in a circular buffer accessed by waiting
// coroutines.
//
// A "scrubber" coroutine removes and logs any ignored notifications that
// aren't picked up during a few cycles around the coroutine ring.
//

void notificationHandler( const Notification &notification ) {
   // Get local copies (for performance).
   unsigned int nextWrite = nFifo.nextWriteIndex ;
   unsigned int currentRead = nFifo.currentReadIndex ;

   // Make sure there is room in the ring buffer for this sample.
   if ( ( ( nextWrite == NUMBER_OF_RING_BUFFER_SLOTS - 1 )
       && ( currentRead == 0 ) )
     || ( nextWrite + 1 == currentRead ) ) {
      nFifo.overrunOccurred = true ;
      score += fatalDiscrepancyFound ;
      testing = false ;
   } else {
      // Copy data from notifier into our circular buffer.
      nFifo.rbuf[ nextWrite ].value     = notification.value ;
      nFifo.rbuf[ nextWrite ].value2    = notification.value2 ;
      nFifo.rbuf[ nextWrite ].eventTime = notification.eventTime ;

      // Clear the slot's stale indicator.
      nFifo.rbuf[ nextWrite ].isStale = 0 ;

      // Bump the real nextWriteIndex.
      if ( ++nFifo.nextWriteIndex == NUMBER_OF_RING_BUFFER_SLOTS ) {
         nFifo.nextWriteIndex = 0 ;
      }
   }
}

////////////////////////////////// Coroutine ///////////////////////////////////
//
// notificationScrubber
//
// Notifications are queued in a circular buffer accessed by waiting coroutines.
// This coroutine removes and logs any ignored notifications that aren't picked
// up during STALE_COUNT cycles around the coroutine ring.
//

void notificationScrubber( void ) {
   const int STALE_COUNT                         = 10 ; // cycles until "stale"
   const int MAX_NUMBER_OF_IGNORED_NOTIFICATIONS =  3 ;
   int   numberOfIgnoredNotifications            =  0 ;
   char  str[ 80 ] ; // ample

   RRTLog( "NotificationScrubber started." ) ;

   while ( testing ) {
      if ( nFifo.overrunOccurred ) {
         score += fatalDiscrepancyFound ;
         testing = false ; // probably already indicated in NotificationHandler
         break ;
      } else {
         // Get local copies (for performance).
         unsigned int nextWrite = nFifo.nextWriteIndex ;
         unsigned int currentRead = nFifo.currentReadIndex ;

         // If there is anything in the notification queue, check to see
         // if the device has reset.  If not, see if the notification at the
         // head is stale (i.e., was ignored by all other coroutines).
         if ( currentRead != nextWrite ) {
            // Mark the notification at the head of the queue so we'll
            // know that all other coroutines have had a chance to remove
            // it before we look at it again next cycle.
            if ( ++( nFifo.rbuf[ currentRead ].isStale ) >= STALE_COUNT ) {
               // If nobody wants it, we'll log an alert and throw the
               // notification away by bumping the currentReadIndex.
               sprintf( str, "Ignored notification removed." ) ;
               alert( str ) ;
               if ( currentRead == NUMBER_OF_RING_BUFFER_SLOTS - 1 ) {
                  nFifo.currentReadIndex = 0 ;
               } else {
                  nFifo.currentReadIndex++ ;
               }

               // See if we've exceeded the limit on ignored notifications.
               if ( ++numberOfIgnoredNotifications
                                        >= MAX_NUMBER_OF_IGNORED_NOTIFICATIONS ) {
                  sprintf( str, "%i notifications ignored. "
                           "Aborting...", numberOfIgnoredNotifications ) ;
                  alert( str ) ;
                  score += fatalDiscrepancyFound ;
                  testing = false ;
               }
            }
         }
      }

      // Give the other coroutines a chance.
      coresume() ;
   }

   // If we missed it above...
   if ( nFifo.overrunOccurred ) {
      sprintf( str, "Data overrun has occurred." ) ;
      alert( str ) ;
      score += fatalDiscrepancyFound ;
   }

   RRTLog( "notificationScrubber ended." ) ;
}

////////////////////////////////////////////////////////////////////////////////
//
// outputAlertMessages
//
// API routine to display any encountered alert messages on the console and in
// the log file.
//
// A space-separated list of alert letters is created in the global array,
// alert_list.
//

void outputAlertMessages( void ) {
   alert_list[ 0 ] = '\0' ; // reset
   if ( alertMessageIndex > 0 ) {
      char str[ 128 ] ; // big enough to hold any alert message
      char c ;        // temp
      cout << endl ;  // start with a blank line
      for ( int i = 0; i < alertMessageIndex; i++ ) {
         if ( alertMessages[i] == alertA ) {
            c = 'A' ;
         } else if ( alertMessages[i] == alertB ) {
            c = 'B' ;
         } else if ( alertMessages[i] == alertC ) {
            c = 'C' ;
         } else if ( alertMessages[i] == alertD ) {
            c = 'D' ;
         } else if ( alertMessages[i] == alertE ) {
            c = 'E' ;
         } else if ( alertMessages[i] == alertF ) {
            c = 'F' ;
         } else if ( alertMessages[i] == alertG ) {
            c = 'G' ;
         } else if ( alertMessages[i] == alertH ) {
            c = 'H' ;
         } else if ( alertMessages[i] == alertI ) {
            c = 'I' ;
         } else if ( alertMessages[i] == alertJ ) {
            c = 'J' ;
         } else if ( alertMessages[i] == alertK ) {
            c = 'K' ;
         } else if ( alertMessages[i] == alertL ) {
            c = 'L' ;
         } else if ( alertMessages[i] == alertM ) {
            c = 'M' ;
         } else if ( alertMessages[i] == alertN ) {
            c = 'N' ;
         } else if ( alertMessages[i] == alertO ) {
            c = 'O' ;
         } else if ( alertMessages[i] == alertP ) {
            c = 'P' ;
         } else if ( alertMessages[i] == alertQ ) {
            c = 'Q' ;
         } else if ( alertMessages[i] == alertR ) {
            c = 'R' ;
         } else if ( alertMessages[i] == alertS ) {
            c = 'S' ;
         } else if ( alertMessages[i] == alertT ) {
            c = 'T' ;
         } else if ( alertMessages[i] == alertU ) {
            c = 'U' ;
         } else {
            c = 'Z';
         }
         sprintf( str, "[%c] %s", c, alertMessages[ i ] ) ;
         alert( str ) ;
         sprintf( str, "%c ", c ) ;
         strcat( alert_list, str ) ;
      }
   }
}

////////////////////////////////////////////////////////////////////////////////
//
// removeModelInstance
//
// This utility removes an entry from theModel array. The modelInstance is
// returned if it is found in theModel array.
//
// The search is made from back-to-front, since the volatile entries are
// located towards the end of the array.
//
// A return of zero indicates that the indicated model was not found in
// the array. In this case, an error message is generated  and the test
// is stopped by setting the global testing flag to false.
//

FSM *removeModelInstance( FSM *modelInstance ) {
   bool instanceFound = false ; // assume instance is not there
   for ( int i = currentNumberOfModelInstances - 1; i >= 0 ; i-- ) {
      if ( theModel[ i ] == modelInstance ) {
         instanceFound = true ;

         // Move the tail back one slot.
         for ( int j = i; j < currentNumberOfModelInstances - 1; j++ ) {
            theModel[ j ] = theModel[ j + 1 ] ;
         }
         theModel[ --currentNumberOfModelInstances ] = 0 ; // safely
         break ;
      }
   }
   if ( !instanceFound ) {
      char str[ 80 ] ; // ample
      sprintf( str, "The model instance '%s' was not found in "
               "RemoveModelInstance.", modelInstance->getName() ) ;
      alert( str ) ;
      testing = false ;
   }
   return instanceFound ? modelInstance : 0 ;
}

//////////////////////////////////// Method ////////////////////////////////////
//
// RepeatableRandomTest::abortTest
//
// This is the implementation of the abortTest method of the
// RepeatableRandomTest class.
//

int RepeatableRandomTest::abortTest( void ) {
   int rc = SUCCESS ; // optimistically

   tstLog( "abortTest" ) ;

   testIsInProgress = false ;

   if ( fclose( f_dotLog ) ) {
      // Close operation failed on .log file.
      const int eStrLen = 80 ;
      char str[ eStrLen + 80 ] ; // ample
      char eStr[ eStrLen ] ; // should be ample
      strerror_r( errno, eStr, eStrLen ) ;
      sprintf( str, "abortTest error attempting to close %s file: %s",
               dotLogFile, eStr ) ;
      cout << str << endl ;
      rc = LOG_FILE_ERROR_OCCURRED ;
   }

   if ( fclose( f_dotTst ) ) {
      // Close operation failed on .tst file.
      const int eStrLen = 80 ;
      char str[ eStrLen + 80 ] ; // ample
      char eStr[ eStrLen ] ; // should be ample
      strerror_r( errno, eStr, eStrLen ) ;
      sprintf( str, "abortTest error attempting to close %s file: %s",
               dotTstFile, eStr ) ;
      cout << str << endl ;
      rc = LOG_FILE_ERROR_OCCURRED ;
   }

   return rc ;
}

//////////////////////////////////// Method ////////////////////////////////////
//
// RepeatableRandomTest::finishTest
//
// This is the implementation of the finishTest method of the
// RepeatableRandomTest class.
//

int RepeatableRandomTest::finishTest( void ) {
   int rc = SUCCESS ; // optimistically

   if ( testIsInProgress ) {
      tstLog( "finishTest" ) ;

      testIsInProgress = false ;
      if ( errorStatus != SUCCESS ) {
         rc = ERROR_OCCURRED_DURING_TEST ;
      }
      if ( fclose( f_dotLog ) ) {
         // Close operation failed on .log file.
         const int eStrLen = 80 ;
         char str[ eStrLen + 80 ] ; // ample
         char eStr[ eStrLen ] ; // should be ample
         strerror_r( errno, eStr, eStrLen ) ;
         sprintf( str, "finishTest error attempting to close %s file: %s",
                  dotLogFile, eStr ) ;
         alert( str ) ;
         rc = LOG_FILE_ERROR_OCCURRED ;
      }

      if ( fclose( f_dotTst ) ) {
         // Close operation failed on .tst file.
         const int eStrLen = 80 ;
         char str[ eStrLen + 80 ] ; // ample
         char eStr[ eStrLen ] ; // should be ample
         strerror_r( errno, eStr, eStrLen ) ;
         sprintf( str, "finishTest error attempting to close %s file: %s",
                  dotTstFile, eStr ) ;
         alert( str ) ;
         rc = LOG_FILE_ERROR_OCCURRED ;
      }
   } else {
      rc = TEST_NOT_IN_PROGRESS ;
   }

   return rc ;
}

//////////////////////////////////// Method ////////////////////////////////////
//
// RepeatableRandomTest::isTestInProgress
//
// This is the implementation of the isTestInProgress method of the
// RepeatableRandomTest class.
//
// This method determines whether a test is ongoing.
//

bool RepeatableRandomTest::isTestInProgress( void ) {
   return testIsInProgress ;
}

//////////////////////////////////// Method ////////////////////////////////////
//
// RepeatableRandomTest::log
//
// This is the implementation of the log method of the RepeatableRandomTest
// class.
//
// The implementation expects an input string without trailing newline char.
// Trailing newline char is added here.
//
// The maximum allowed size of the input log string is 160 bytes, without nl.
//

int RepeatableRandomTest::log( const char *str ) {
   int rc = SUCCESS ; // optimistically
   char *pStr = const_cast <char *>( str ) ; // we may need a copy to truncate it

   // With time + nl + nul:
   static char strWithTimeAndNl[ MAX_LOG_RECORD_SIZE + 7 + 1 + 1 ] ;

   if ( testIsInProgress ) {
      char tempStr[ MAX_LOG_RECORD_SIZE + 1 ] ;
      if ( strlen( str ) > MAX_LOG_RECORD_SIZE ) {
         // We'll limit the log record size.
         strncpy( tempStr, str, MAX_LOG_RECORD_SIZE ) ;
         tempStr[ MAX_LOG_RECORD_SIZE ] = '\0' ;
         pStr = tempStr ;
      }
      sprintf( strWithTimeAndNl, "%6i,%s\n", getTime(), pStr ) ;
      int strLen = strlen( strWithTimeAndNl ) ;
      size_t bytesWritten = fwrite( strWithTimeAndNl, 1, strLen, f_dotLog ) ;
      if ( bytesWritten != strLen ) {
         // Write operation failed on .log file.
         const int eStrLen = 80 ;
         char strx[ eStrLen + 80 ] ; // ample
         char eStr[ eStrLen ] ; // should be ample
         strerror_r( errno, eStr, eStrLen ) ;
         sprintf( strx, "log error attempting to write %s file: %s",
                  dotLogFile, eStr ) ;
         // alert(strx); // Isn't working correctly!
         cout << strx << endl ;
         rc = LOG_FILE_ERROR_OCCURRED ;
      }
   } else {
      rc = TEST_NOT_IN_PROGRESS ;
   }

   return rc ;
}

//////////////////////////////////// Method ////////////////////////////////////
//
// RepeatableRandomTest::logDebug
//
// This is the implementation of the logDebug method of the RepeatableRandomTest
// class.
// 
// The debug message is written to the .tst file.
//
// The implementation expects an input string without trailing newline char.
// Trailing newline char is added here.
//
// The maximum allowed size of the input log string is 160 bytes, without nl.
//

int RepeatableRandomTest::logDebug( const char *str ) {
   
   return tstLog( str ) ;
}

//////////////////////////////////// Method ////////////////////////////////////
//
// RepeatableRandomTest::startTest
//
// This is the implementation of the startTest method of the
// RepeatableRandomTest class.
//

int RepeatableRandomTest::startTest( void ) {
   const int eStrLen = 80 ;
   char      str[ eStrLen + 80 ] ; // ample
   char      eStr[ eStrLen ] ; // should be ample
   int       rc = SUCCESS ; // optimistically
   const int MAX_READ_BUFFER_SIZE = sizeof str ;

   if ( testIsInProgress ) {
      rc = TEST_ALREADY_IN_PROGRESS ;
   } else {
      // Open the sequence.dat file in the current directory.
      f_sequenceDotDat = fopen( sequenceDotDatFile, "r+" ) ; // read / write
      if ( !f_sequenceDotDat ) {
         // Open operation failed.
         strerror_r( errno, eStr, eStrLen ) ;
         sprintf( str, "startTest failed due to a missing %s file: %s",
                  sequenceDotDatFile, eStr ) ;
         alert( str ) ;
         rc = SEQUENCE_DOT_DAT_FILE_MISSING ;
      } else {
         int  bytesRead = fread( str, 1, MAX_READ_BUFFER_SIZE,
                                 f_sequenceDotDat ) ;
         if (bytesRead > 3) {
            char *pCommaLocation = strchr( str, ',' ) ;
            if ( pCommaLocation != NULL ) {
               *pCommaLocation = '\0' ; // cover the comma with a nul
               logfileNumber = atoi( pCommaLocation + 1 ) ;
               if ( !logfileNumber ) {
                  sprintf( str, "startTest unable to find a log sequence "
                           "number in %s file", sequenceDotDatFile ) ;
                  alert( str ) ;
                  rc = SEQUENCE_DOT_DAT_FILE_HAS_BAD_FORMAT ;
               } else {
                  // Bump the log file number in the sequence.dat file.
                  char sddFileStr[ eStrLen + 80 ] ; // ample
                  sprintf( sddFileStr, "%s, %i \n", str, logfileNumber + 1 ) ;
                  int strLen = strlen( sddFileStr ) ;
                  fseek( f_sequenceDotDat, 0L, SEEK_SET ) ;
                  size_t bytesWritten = fwrite( sddFileStr, 1, strLen,
                                                f_sequenceDotDat ) ;
                  if ( bytesWritten != strLen ) {
                     // Write operation failed on sequence.dat file.
                     strerror_r( errno, eStr, eStrLen ) ;
                     sprintf( str, "startTest unable to read %s file: %s",
                              sequenceDotDatFile, eStr ) ;
                     alert( str ) ;
                     rc = SEQUENCE_DOT_DAT_FILE_ERROR_OCCURRED ;
                  } else {
                     dashTrailing( str ) ; // remove path's trailing whitespace
                     if ( str[ strlen( str ) - 1 ] != '/' ) {
                        strcat( str, "/" ) ; // add a slash
                     }

                     // Keep a copy of the data directory.
                     strcpy( logfileDirectory, str ) ;
                     strcpy( dotLogFile, str ) ; // not including name + ext
                     strcpy( dotTstFile, str ) ; // not including name + ext

                     // Complete the file specs.
                     sprintf( str, "rrt%05i.log", logfileNumber ) ;
                     strcat( dotLogFile, str ) ; // path + name + ext
                     sprintf( str, "rrt%05i.tst", logfileNumber ) ;
                     strcat( dotTstFile, str ) ; // path + name + ext

                     time_t tm = time( NULL ) ;
                     sprintf( fileHeaderStr, "%s,%s", testDescription,
                              ctime( &tm ) ) ;
                     int headerStrLen = strlen( fileHeaderStr ) ;

                     // Check that the log files don't already exist.
                     struct stat fi ;
                     int sr = stat( dotLogFile, &fi ) ;
                     if ( sr == -1 ) {
                        if ( errno != ENOENT ) {
                           strerror_r( errno, eStr, eStrLen ) ;
                           sprintf( str, "startTest failed to obtain file info "
                                    "for %s file: %s", dotLogFile, eStr ) ;
                           alert( str ) ;
                           rc = LOG_FILE_ERROR_OCCURRED ;
                        } else {
                           // Create (and open) the .log file in the log data
                           // directory.
                           f_dotLog = fopen( dotLogFile, "w" ) ; // write
                           if ( !f_dotLog ) {
                              // Open operation failed.
                              strerror_r( errno, eStr, eStrLen ) ;
                              sprintf( str, "startTest failed due to a missing "
                                       "%s file: %s", dotLogFile, eStr ) ;
                              alert( str ) ;
                              rc = SEQUENCE_DOT_DAT_FILE_MISSING ;
                           } else if ( fwrite( fileHeaderStr, 1,
                                               headerStrLen,
                                               f_dotLog ) != headerStrLen ) {
                              // Write header failed on .log file.
                              strerror_r( errno, eStr, eStrLen ) ;
                              sprintf( str, "startTest error attempting to "
                                       "write %s file: %s", dotLogFile, eStr ) ;
                              // alert(str); // Isn't working correctly!
                              cout << str << endl ;
                              rc = LOG_FILE_ERROR_OCCURRED ;
                           }
                        }
                     } else {
                        sprintf( str, "startTest unable to create %s file; it "
                                 "already exists.", dotLogFile ) ;
                        alert( str ) ;
                        rc = LOG_FILE_ERROR_OCCURRED ;
                     }

                     sr = stat( dotTstFile, &fi ) ;
                     if ( sr == -1 ) {
                        if ( errno != ENOENT ) {
                           strerror_r( errno, eStr, eStrLen ) ;
                           sprintf( str, "startTest failed to obtain file info "
                                    "for %s file: %s", dotTstFile, eStr ) ;
                           alert( str ) ;
                           rc = LOG_FILE_ERROR_OCCURRED ;
                        } else {
                           // Create (and open) the .tst file in the log data
                           // directory.
                           f_dotTst = fopen( dotTstFile, "w" ) ; // write
                           if ( !f_dotTst ) {
                              // Open operation failed.
                              strerror_r( errno, eStr, eStrLen ) ;
                              sprintf( str, "startTest failed due to a missing "
                                       "%s file: %s", dotTstFile, eStr ) ;
                              alert( str ) ;
                              rc = SEQUENCE_DOT_DAT_FILE_MISSING ;
                           } else if ( fwrite(fileHeaderStr, 1,
                                              headerStrLen,
                                              f_dotTst) != headerStrLen ) {
                              // Write header failed on .tst file.
                              strerror_r( errno, eStr, eStrLen ) ;
                              sprintf( str, "startTest error attempting to "
                                       "write %s file: %s", dotTstFile, eStr ) ;
                              // alert(str); // Isn't working correctly!
                              cout << str << endl ;
                              rc = LOG_FILE_ERROR_OCCURRED ;
                           } else {
                              startTime = getTimeInMs() ;
                              tstLog( "startTest" ) ;
                           }
                        }
                     } else {
                        sprintf( str, "startTest unable to create %s file; it "
                                 "already exists.", dotTstFile ) ;
                        alert( str ) ;
                        rc = LOG_FILE_ERROR_OCCURRED ;
                     }
                  }
               }
            } else { // comma wasn't found
               sprintf( str, "startTest unable to find a comma in %s file",
                        sequenceDotDatFile ) ;
               alert( str ) ;
               rc = SEQUENCE_DOT_DAT_FILE_HAS_BAD_FORMAT ;
            }
         } else {
            // Read operation failed, or the sequence.dat file contents
            // were invalid.  The format is 'filePath, sequenceNumber'.
            strerror_r( errno, eStr, eStrLen ) ;
            sprintf( str, "startTest unable to read %s file: %s",
                     sequenceDotDatFile, eStr ) ;
            alert( str ) ;
            rc = SEQUENCE_DOT_DAT_FILE_HAS_BAD_FORMAT ;
         }
      }

      if ( rc == SUCCESS ) {
         testIsInProgress = true ;
      }
   }

   return rc ;
}

//////////////////////////////////// Method ////////////////////////////////////
//
// RepeatableRandomTest::tstLog
//
// This is the implementation of the private tstLog method of the
// RepeatableRandomTest class.
//
// The implementation expects an input string without trailing newline char.
// The trailing newline char is added here.
//
// The maximum allowed size of the input string is 160 bytes, without nl.
//

int RepeatableRandomTest::tstLog( const char *str ) {
   int rc = SUCCESS ; // optimistically
   static char strWithTimeAndNl[ MAX_LOG_RECORD_SIZE + 7 + 1 + 1 ] ; // with time
                                                                     // + nl + nul
   char *pStr = const_cast <char *>( str ) ; // we may need a copy to truncate it
   char tempStr[ MAX_LOG_RECORD_SIZE + 1 ] ;
   if ( strlen(str) > MAX_LOG_RECORD_SIZE ) {
      // We'll limit the log record size.
      strncpy( tempStr, str, MAX_LOG_RECORD_SIZE ) ;
      tempStr[ MAX_LOG_RECORD_SIZE ] = '\0' ;
      pStr = tempStr ;
   }                 
   sprintf( strWithTimeAndNl, "%6i,%s\n", getTime(), pStr ) ;
   int strLen = strlen( strWithTimeAndNl ) ;
   size_t bytesWritten = fwrite( strWithTimeAndNl, 1, strLen, f_dotTst ) ;
   if ( bytesWritten != strLen ) {
      // Write operation failed on .tst file.
      const int eStrLen = 80 ;
      char strx[ eStrLen + 80 ] ; // ample
      char eStr[ eStrLen ] ; // should be ample
      strerror_r( errno, eStr, eStrLen ) ;
      sprintf( strx, "tstLog error attempting to write %s file: %s",
               dotTstFile, eStr ) ;
      alert( strx ) ;
      cout << strx << endl ;
      rc = LOG_FILE_ERROR_OCCURRED ;
   }

   return rc ;
}

///////////////////////////////////// CTOR /////////////////////////////////////
//
// RequirementsTallyHandler::RequirementsTallyHandler
//
// This is the implementation of the default RequirementsTallyHandler CTOR
// of the RequirementsTallyHandler class.
//

RequirementsTallyHandler::RequirementsTallyHandler() {
   // Not very efficient, but quick !
   talliesEncountered = new unsigned short[ count ] ;
   talliesMet = new unsigned short[ count ] ;
   memset( talliesEncountered, 0x00, count * sizeof( short ) ) ;
   memset( talliesMet, 0x00, count * sizeof( short ) ) ;
}

///////////////////////////////////// DTOR /////////////////////////////////////
//
// RequirementsTallyHandler::~RequirementsTallyHandler
//
// This is the implementation of the DTOR for the RequirementsTallyHandler
// class.
//

RequirementsTallyHandler::~RequirementsTallyHandler() {
   delete [] talliesEncountered ;
   delete [] talliesMet ;
}

//////////////////////////////////// Method ////////////////////////////////////
//
// RequirementsTallyHandler::tallyMet
//
// This is the implementation of the tallyMet method of the
// RequirementsTallyHandler class.
//
// This routine records when we successfully execute the "shall" phrase of a
// requirement.
//

void RequirementsTallyHandler::tallyMet( unsigned short requirementId ) {
   talliesMet[ requirementId ]++ ;
}

//////////////////////////////////// Method ////////////////////////////////////
//
// RequirementsTallyHandler::tallyEncountered
//
// This is the implementation of the tallyEncountered method of the
// RequirementsTallyHandler class.
//
// This routine records any encounters with requirements during the test run.
//

void RequirementsTallyHandler::tallyEncountered( unsigned short requirementId ) {
   talliesEncountered[ requirementId ]++ ;
}

//////////////////////////////////// Method ////////////////////////////////////
//
// RequirementsTallyHandler::toFile
//
// This is the implementation of the toFile method of the
// RequirementsTallyHandler class.
//
// The implementation appends encountered and met requirements from the current
// test run to the requirements REQUIREMENTS_CSV_FILE file repository.
//
// The file appends requirements if this test run was successful, i.e., the
// RepeatableRandomTest::GetErrorStatus returns SUCCESS on entry to this
// routine.
//

int RequirementsTallyHandler::toFile( unsigned int totalRequirementsCount ) {
   const int eStrLen = 80 ;
   char      str[ eStrLen + 80 ] ; // ample
   char      eStr[ eStrLen ] ; // should be ample
   const int MAX_READ_BUFFER_SIZE = sizeof str ;

   int  rc = RRTGetErrorStatus() ;
   if ( rc == SUCCESS ) {
      // Log encountered requirements to the output .csv file.
      FILE *fp ;
      if ( ( fp = fopen( REQUIREMENTS_CSV_FILE, "a+" ) ) ) {
         // Get the current file size.
         int result = fseek( fp, 0L, SEEK_END ) ;
         if ( result ) {
            alert( "Unable to seek in requirements .csv file." ) ;
            rc = COMMAND_FAILED ;
         } else {
            int position = ftell( fp ) ;
            if ( !position ) {
               // Add the header if the file is empty.
               fprintf( fp, "Requirement ID,Met,Encountered\n" ) ;
               fprintf( fp, "; Total Requirements:%d\n", 
                        totalRequirementsCount ) ;
            }
         }

         // Now write our requirements to the file.
         for ( int i = 0; rc == 0 && i < count; i++ ) {
            if ( talliesEncountered[ i ] || talliesMet[ i ] ) {
//               int bytesWritten = fprintf(fp, "%5i,%3i,%3i\n",
               int bytesWritten = fprintf(fp, "   %02X,%3i,%3i\n",
                                           i, talliesMet[ i ],
                                           talliesEncountered[ i ] ) ;
               if ( !bytesWritten ) {
                  // An error occurred attempting to append to the file.
                  alert( "Unable to write to requirements .csv file." ) ;
                  rc = COMMAND_FAILED ;
                  break ;
               }
            }
         }

         fclose( fp ) ;
      } else {
         // Open operation failed.
         strerror_r(errno, eStr, eStrLen ) ;
         sprintf( str, "Unable to open requirements .csv file: %s", eStr ) ;
         alert( str ) ;
         rc = COMMAND_FAILED ;
      }
   }

   return rc ;
}

//////////////////////////////////// Method ////////////////////////////////////
//
// RequirementsTallyHandler::toLog
//
// This is the implementation of the toLog method of the
// RequirementsTallyHandler class.
//
// The implementation produces a formatted table that is added to the .log file.
//
// The routine appends the table if this test run was successful, i.e., the
// RepeatableRandomTest::GetErrorStatus returns SUCCESS on entry to this
// routine.
//

int RequirementsTallyHandler::toLog( void ) {
   const char indent[] = "     " ;
   char line[ 90 ] ; // ample
   char str [ 20 ] ; // ample

   int  rc = RRTGetErrorStatus() ;
   if ( rc == SUCCESS ) {
      // We log encountered requirements to the output test log.
      RRTLog( "" ) ; // blank line
      RRTLog( "Requirements covered by this test run (met/encountered):" ) ;
      strcpy( line, indent ) ;
      int j = 0 ;
      int requirementsMet = 0 ;
      for ( int i = 0; i < count; i++ ) {
         if ( talliesEncountered[ i ] || talliesMet[ i ]) {
            if ( talliesMet[ i ] && talliesEncountered[ i ] == talliesMet[ i ] ) {
               ++requirementsMet ;
            }
//            sprintf( str, "%5i (%3i/%3i)  ",
            sprintf( str, "   %02X (%3i/%3i)  ",
                     i, talliesMet[ i ], talliesEncountered[ i ] ) ;
            strcat( line, str ) ;
            if ( ++j >= 4 ) {
               RRTLog( line ) ;
               strcpy( line, indent ) ;
               j = 0 ;
            }
         }
      }
      if ( j ) {
         RRTLog( line ) ;
      }
      RRTLog( "" ) ; // blank line
      sprintf( line, "'Requirements met' coverage: %i of %i (%i percent).",
               requirementsMet, TOTAL_REQ_COUNT,
               int( 100.0 * requirementsMet / TOTAL_REQ_COUNT + 0.5 ) ) ;
      RRTLog( line ) ;
      RRTLog( "" ) ; // blank line
   }

   return rc ;
}

////////////////////////////////// Coroutine ///////////////////////////////////
//
// roundtripCounter
//
// This optional coroutine provides a count of the number of coroutine cycles.
// It is currently required to be the last coroutine running before returning
// to cobegin.
//

void roundtripCounter( void ) {
   unsigned int coresumeCount = 0 ;
   unsigned int endTime ;
   unsigned int startTime = getTimeInMs() ;

   bool otherCoroutinesComplete = false ;
   while ( !otherCoroutinesComplete ) {
      #ifdef SHOW_HISTOGRAM
      itHistCR.tally() ;
      #endif // def SHOW_HISTOGRAM
      coresumeCount++ ;
      coresume() ;
      if ( !testing && getCoroutineCount() < 2 ) {
         otherCoroutinesComplete = true ;
      }
   }

   endTime = getTimeInMs() ;

   when( getCoroutineCount() < 2 ) {
      // This is the last running coroutine. The current coroutine
      // implementation requires returning to Cobegin from a coroutine
      // with no calling parameters. Here we prepare a line to display after
      // leaving curses windowing.
      sprintf( roundtripCounterOutputString,
               ">>> coresume cycle count: %u (%u / sec).",
               coresumeCount,
               (unsigned int)( coresumeCount
                                     / ( ( endTime - startTime ) / 1000.0 ) ) ) ;
   }
}

////////////////////////////////////////////////////////////////////////////////
//
// sendEvent
//
// This utility sends an event to the model's normal queue.
//
// An event with a NULL "to" is sent to all model components.
// An event with a NULL "from" is sent from an external (to the model) source.
//

void sendEvent( event theEvent, long parm1, long parm2, FSM *to, FSM *from ) {
   #if defined(SHOW_EVENT_PROCESSING)
   char str[ 120 ] ; // ample
   sprintf( str, "Event '%s' sent to %s.", eventText( theEvent ),
            to ? to->getName() : "everyone" ) ;
   cout << str << endl ;
   RRTLog( str ) ;
   #endif // defined (SHOW_EVENT_PROCESSING)

   // Check for a valid destination.
   FSM **ppFsm = pModelComponentArray ;
   #if 0 //defined(SHOW_EVENT_PROCESSING)
   cout << "model component array:" << endl;
   RRTLog("model component array:");
   Dump(ppFsm, 12, true); // cout and RRTLog
   #endif // defined(SHOW_EVENT_PROCESSING)

   bool destinationFound = false ; // pessimistically
   if ( to == 0 ) {
      destinationFound = true ;
   } else {
      do {
         if ( *ppFsm == to ) {
            // The event is being sent to a valid destination (a model
            // component).
            destinationFound = true ;
#if 0
{
   char str[ 120 ] ; // ample
   sprintf( str, " ==> Event sent to %s from %s.",
            to->getName(), from->getName() ) ;
   cout << str << endl ;
   RRTLog( str ) ;
}
#endif // 0
            break ;
         }
      } while ( *( ++ppFsm ) ) ;
   }

   if ( destinationFound ) {
      // OK, so we'll queue the event in the normal queue.
      int rc = pModelNormalQueue->pushEvent( theEvent, parm1, parm2, from, to ) ;
      if ( rc != FIFO_SUCCESS ) {
         if ( rc == OVERRUN_OCCURRED ) {
            testing = false ;
            score += fatalDiscrepancyFound ;
            alert( "Overrun occurred in the model's ordinary queue." ) ;
         }
      }
   } else {
      // The destination is unknown.  We'll complain, but continue without
      // sending the questionable event.
      char str[ 120 ] ; // ample;
      sprintf( str, "Destination '%s' of event '%s' is not a model component.",
               to ? to->getName() : "", eventText( theEvent ) ) ;
      alert( str ) ;
      score += modelDiscrepancyFound ;
   }
}

void sendEventToId( event theEvent, long parm1, long parm2,
                    Id *toId, FSM *from ) {
   FSM *toFsm = 0 ;

   #if defined(SHOW_EVENT_PROCESSING)
   char str[ 120 ] ; // ample
   sprintf( str, "Event '%s' sent to %s.", eventText( theEvent ),
            toId ? toId->getName() : "everyone" ) ;
   cout << str << endl ;
   RRTLog( str ) ;
   #endif // defined (SHOW_EVENT_PROCESSING)

   // Check for a valid destination.
   FSM **ppFsm = pModelComponentArray ;
   #if 0 //defined(SHOW_EVENT_PROCESSING)
   cout << "model component array:" << endl;
   RRTLog("model component array:");
   Dump(ppFsm, 12, true); // cout and RRTLog
   #endif // defined(SHOW_EVENT_PROCESSING)

   bool destinationFound = false ; // pessimistically
   if ( toId == 0 ) {
      // toFsm was initialized to 0 above.
      destinationFound = true ;
   } else {
      char *toName    = toId->getName() ;
      int  toInstance = toId->getInstance() ;
      do {
         if ( (*ppFsm)->getInstance() == toInstance
           && !strcmp( typeid(**ppFsm).name(), toName ) ) {
            // The event is being sent to a valid destination (a model
            // component).
            toFsm = *ppFsm ;
            destinationFound = true ;
#if 0
{
   char str[120]; // ample
   sprintf(str, " ==> Event sent to %s (instance %i).",
            toId->getName(), toId->getInstance());
   RRTLog(str);
}
#endif // 0

            break ;
         }
      } while ( *( ++ppFsm ) ) ;
   }
   if ( destinationFound ) {
      // OK, so we'll queue the event in the normal queue.
      int rc = pModelNormalQueue->pushEvent( theEvent, parm1, parm2,
                                             from, toFsm ) ;
      if ( rc != FIFO_SUCCESS ) {
         if ( rc == OVERRUN_OCCURRED ) {
            testing = false ;
            score += fatalDiscrepancyFound ;
            alert( "Overrun occurred in the model's ordinary queue." ) ;
         }
      }
   } else {
      // The destination is unknown.  We'll complain, but continue without
      // sending the questionable event.
      char str[ 160 ] ; // ample;
      sprintf( str, "Destination Id '%s' (instance %i) of event '%s' is not "
               "a model component.", toId->getName(), toId->getInstance(),
               eventText( theEvent ) ) ;
      alert( str ) ;
      score += modelDiscrepancyFound ;
   }
}

////////////////////////////////////////////////////////////////////////////////
//
// sendPriorityEvent
//
// This utility sends a self-directed event to the model's priority queue.
// The "sender" is a variable that must be set to a model component's "this".
//
// SendPriorityEvent cannot be used by an external (not a model component)
// source.
//

void sendPriorityEvent( event theEvent, long parm1, long parm2 ) {
   #if defined(SHOW_EVENT_PROCESSING)
   char str[ 120 ] ; // ample
   sprintf( str, "Event '%s' sent to %s with priority.", eventText( theEvent ),
            sender ? sender->getName() : "?????" ) ;
   cout << str << endl ;
   RRTLog( str ) ;
   #endif // defined (SHOW_EVENT_PROCESSING)

   // Check for a valid source.
   FSM **pFsm = pModelComponentArray ;
   bool sourceFound = false ; // pessimistically
   do {
      if ( *pFsm == sender ) {
         // The event is being sent from a valid source (a model component).
         sourceFound = true ;
         break ;
      }
   } while ( *( ++pFsm ) ) ;
   if ( sourceFound ) {
      // OK, so we'll queue the event in the self-directed queue.
      int rc = pModelPriorityQueue->pushEvent( theEvent, parm1, parm2,
                                               sender, sender ) ;
      if ( rc != FIFO_SUCCESS ) {
         if ( rc == OVERRUN_OCCURRED ) {
            testing = false ;
            score += fatalDiscrepancyFound ;
            alert( "Overrun occurred in the model's priority queue." ) ;
         }
      }
   } else {
      // The source is unknown.  We'll complain, but continue without
      // sending the questionable event.
      char str[ 160 ] ; // ample;
      sprintf( str, "Source '%s' of self-directed event '%i' (instance %s) "
               "is not a model component.", sender ? sender->getName() : "?",
               sender ? sender->getInstance() : 0, eventText( theEvent ) ) ;
      alert( str ) ;
      score += modelDiscrepancyFound ;
   }
}

////////////////////////////////////////////////////////////////////////////////
//
// sendDelayedEvent
//
// This utility sends an event to the model's normal queue after a specified
// time delay (in ms).
//
// This routine issues an internal invoke so that the SendDelayedEvent doesn't
// block the calling coroutine.
//
// An event with a NULL "to" (i.e., = 0) is sent to all model components.
// An event with a NULL "from" is sent from an external (to the model) source.
//
// The two parameters parm1 and parm2 contain event-related data.
//
// The continuing parameter is the address of a boolean that indicates if this
// test is still ongoing while waiting for the delayed event.
//
// Note: all eight parameters must be specified.
//

void sendDelayedEvent( int msToWait, event theEvent, long parm1, long parm2,
                       FSM *to, FSM *from, bool *continuing, bool *canceling ) {
   #if defined(SHOW_EVENT_PROCESSING)
   char str[ 160 ] ; // ample
   sprintf( str, "Timed event '%s' sent to %s for delivery after %i ms.",
            eventText( theEvent ), to ? to->getName() : "everyone",
            msToWait ) ;
   cout << str << endl ;
   RRTLog( str ) ;
   #endif // defined (SHOW_EVENT_PROCESSING)

   // Check for a valid destination.
   FSM **ppFsm = pModelComponentArray ;
   #if 0 //defined(SHOW_EVENT_PROCESSING)
   cout << "model component array:" << endl;
   RRTLog("model component array:");
   Dump(ppFsm, 12, true); // cout and RRTLog
   #endif // defined(SHOW_EVENT_PROCESSING)

   bool destinationFound = false ; // pessimistically
   if ( to == 0 ) {
      destinationFound = true ;
   } else {
      do {
         if ( *ppFsm == to ) {
            // The event is being sent to a valid destination (a model
            // component).
            destinationFound = true ;
            break ;
         }
      } while ( *(++ppFsm) ) ;
   }
   if ( destinationFound ) {
      // OK, so we'll cause the event to be queued in the normal queue at
      // the requested time from now.
      invoke( COROUTINE( doSendDelayedEvent ), 8,
              msToWait, theEvent, parm1, parm2, to, from, 
              continuing, canceling ) ;
      coresume() ; // start delayed event timer
   } else {
      // The destination is unknown.  We'll complain, but continue without
      // sending the questionable event.
      char str[ 160 ] ; // ample;
      sprintf( str, "Destination '%s' of timed event '%s' is not a model "
               "component.", to ? to->getName() : "", eventText( theEvent ) ) ;
      alert( str ) ;
      score += modelDiscrepancyFound ;
   }
}

void sendDelayedEventToId( int msToWait, event theEvent, long parm1,
                           long parm2, Id *toId, FSM *from, 
                           bool *continuing, bool *canceling ) {
   FSM *toFsm = 0 ;

   #if defined(SHOW_EVENT_PROCESSING)
   char str[ 120 ] ; // ample
   sprintf( str, "Timed event '%s' sent to %s for delivery after %i ms.",
            eventText( theEvent ), toId ? toId->getName() : "everyone",
            msToWait ) ;
   cout << str << endl ;
   RRTLog( str ) ;
   #endif // defined (SHOW_EVENT_PROCESSING)

   // Check for a valid destination.
   FSM **ppFsm = pModelComponentArray ;
   #if 0 //defined(SHOW_EVENT_PROCESSING)
   cout << "model component array:" << endl;
   RRTLog("model component array:");
   Dump(ppFsm, 12, true); // cout and RRTLog
   #endif // defined(SHOW_EVENT_PROCESSING)

   bool destinationFound = false ; // pessimistically
   if ( toId == 0 ) {
      // toFsm was initialized to 0 above.
      destinationFound = true ;
   } else {
      char *toName    = toId->getName() ;
      int  toInstance = toId->getInstance() ;
      do {
         if ( (*ppFsm)->getInstance() == toInstance
           && !strcmp( typeid(**ppFsm).name(), toName ) ) {
            // The event is being sent to a valid destination (a model
            // component).
            toFsm = *ppFsm ;
            destinationFound = true ;
            break ;
         }
      } while ( *( ++ppFsm ) ) ;
   }

   if ( destinationFound ) {
      // OK, so we'll cause the event to be queued in the normal queue at
      // the requested time from now.
      invoke( COROUTINE( doSendDelayedEvent ), 8,
              msToWait, theEvent, parm1, parm2, toFsm, from, 
              continuing, canceling ) ;
      coresume() ; // start delayed event timer
   } else {
      // The destination is unknown.  We'll complain, but continue without
      // sending the questionable event.
      char str[ 160 ] ; // ample;
      sprintf( str, "Destination Id '%s' (instance %i) of delayed event '%s' "
               "is not a model component.",
               toId->getName(), toId->getInstance(), eventText( theEvent ) ) ;
      alert( str ) ;
      score += modelDiscrepancyFound ;
   }
}

////////////////////////////////////////////////////////////////////////////////
//
// strlwr
//
// A utility (missing in Unix gcc) to lowercase a string.
//
// The input string is changed.  The returned string pointer points to the
// updated input string.
//

char *strlwr( char *string ) {
   int strLen = strlen( string ) ;
   for ( int i = 0; i < strLen; i++ ) {
      string[ i ] = tolower( string[ i ] ) ;
   }
   return string ;
}

////////////////////////////////////////////////////////////////////////////////
//
// strupr
//
// A utility (missing in Unix gcc) to uppercase a string.
//
// The input string is changed.  The returned string pointer points to the
// updated input string.
//

char *strupr( char *string ) {
   int strLen = strlen( string ) ;
   for ( int i = 0; i < strLen; i++ ) {
      string[ i ] = toupper( string[ i ] ) ;
   }
   return string ;
}

///////////////////////////////////// CTOR /////////////////////////////////////
//
// TransactionLog::TransactionLog
//
// This is the implementation of the default TransactionLog CTOR
// of the TransactionLog class.
//
                                
TransactionLog::TransactionLog()
 : bufferHasOverflowed( false ),
   tlBufferSize( TRANSACTION_LOG_SIZE ) {
   tlBuffer = new char[ tlBufferSize ] ;
   pTb      = tlBuffer ;
   memset( tlBuffer, 0x00, tlBufferSize * sizeof( char ) ) ;
   extraSpace = 0 ;
}

///////////////////////////////////// DTOR /////////////////////////////////////
//
// TransactionLog::~TransactionLog
//
// This is the implementation of the DTOR for the TransactionLog class.
//

TransactionLog::~TransactionLog() {
   delete [] tlBuffer ;
}

//////////////////////////////////// Method ////////////////////////////////////
//
// TransactionLog::addExtraSpace
//
// This is the implementation of the addExtraSpace method of the
// TransactionLog class.
//
// The extraSpace amount is included between the test time and "   <-- " string
// in order to allow including an outcome indicator on responses.
// 
// For example:
//  
//    161122 --> 0,13,1,0
//    165950   <-- -2, 1, 1
// 
// allows inclusion of a CRC error indication ("O3a"), with extraSpace set to 4:
// 
//    161122     --> 0,13,1,0
//    165950 O3a   <-- -2, 1, 1
//

void TransactionLog::addExtraSpace( int eSpace ) {
   extraSpace = eSpace ;
}

//////////////////////////////////// Method ////////////////////////////////////
//
// TransactionLog::messageReceived
//
// This is the implementation of the messageReceived method of the
// TransactionLog class.
// 
// Optional parameters may be used to add an outcome indication to received
// messages to reflect errors. Consider the following taxonomy of 
// command / response outcomes:
// 
//    1. command executed successfully                              ("O1")
//    2. response reported a command error                            
//       a. invalid command                                         ("O2a")
//       b. invalid length                                          ("O2b")
//       c. processing error                                        ("O2c")
//       d. invalid parameter                                       ("O2d")
//       e. functional error                                        ("O2e")
//    3. response reported a functional error                         
//       a. CRC error                                               ("O3a")
//       b. timeout                                                 ("O3b")
//    4. command execution failed                                   
//       a. no response                                             ("O4a")
//       b. response had bad protocol syntax                        ("O4b")
//       c. response contained wrong number of data fields          ("O4c")
//       d. response data field contained wrong data type / value   ("O4d")
//    5. spurious response received (without a command)             ("O5")
//
// An oMsg string may, for example, be used to indicate that the device under 
// test is reporting an invalid command error ("O2a") or that the RRTGen test 
// is indicating that the no response was received from the device under 
// test ("O4a"). The default value for oMsg is five spaces, the suggested 
// value for the nominal case where the command executed successfully ("O1").
//
// Prior to calling messageReceived with an oMsg, the extraSpace member 
// variable should be set, using addExtraSpace, to the longest outcome 
// string size, plus 1, for consistency of presentation with the messageSent 
// and messageReceived entries in the transaction log.
// 
// The optional boolean indicates whether the message is from the debug port
// (rather than from the device under test); if the message is from debug,
// the string used for visual distinction contains "<D-" rather than "<--".
//
// The input message in rMsg should not include a nl char at the end of the 
// message; it is added here.
//
// The message is prepended with the test time (in ms) and a "   <-- " string
// for visual distinction. In the case of reporting an error, say a CRC error,
// the prepended string might be "O3a   <-- ", thus producing the logged string,
// "165950 O3a   <-- -2, 1, 1".
//
// The input rMsg + oMsg length should not exceed MAX_LOG_RECORD_SIZE characters.
//

int TransactionLog::messageReceived( char *rMsg, bool fromDebug, const char *oMsg ) {

   int rc = SUCCESS ; // optimistically

   if ( !bufferHasOverflowed ) {
      if ( strlen( rMsg ) > MAX_LOG_RECORD_SIZE ) {
         // We'll limit the log record size.
         rMsg[ MAX_LOG_RECORD_SIZE ] = '\0' ;
      }
      if ( strlen( oMsg ) > extraSpace ) {
         // We'll alert and halt the test.
         char  str[ 200 ] ; // ample
         sprintf( str, "oMsg ('%s') string is too long (> %d) in messageReceived.", 
                  oMsg, extraSpace ) ;
         alert( str ) ;
         score += fatalDiscrepancyFound ;
         testing = false ;
         return ERROR_OCCURRED_DURING_TEST ;
      }
      mostRecentResponseTime = RRTGetTime() ;
      // Including time + extraSpace + "   <-- " + nl + nul.
      char strWithTimeAndNl[ MAX_LOG_RECORD_SIZE + 6 + extraSpace + 7 + 1 + 1 ] ;
      sprintf( strWithTimeAndNl, "%6i %-*s  <%c- %s\n", mostRecentResponseTime, 
               extraSpace, oMsg, fromDebug ? 'D' : '-', rMsg );
      if ( pTb + strlen( strWithTimeAndNl ) < tlBuffer + tlBufferSize ) {
         strcpy( pTb, strWithTimeAndNl ) ;
         pTb += strlen( strWithTimeAndNl ) ;
      } else {
         // Notify the test (but only once) that the buffer has overflowed.
         if ( !bufferHasOverflowed ) {
            // **??** TBD Consider dynamically extending (e.g., doubling)
            //            the buffer size here.                    **??**
            alert( "Transaction buffer has overflowed." ) ;
            bufferHasOverflowed = true ;
         }
      }
   } else {
      rc = BUFFER_OVERFLOWED ;
   }

   return rc ;
}

//////////////////////////////////// Method ////////////////////////////////////
//
// TransactionLog::messageSent
//
// This is the implementation of the messageSent method of the
// TransactionLog class.
//
// The input message should not include a nl char at the end of the message;
// it is added here.
//
// The message is prepended with the test time (in ms) and a " --> " string
// for visual distinction. There may be additional blanks inserted before the 
// arrow, according on the value of extraSpaces, to accomodate outcome
// indicators displayed on the response for the command.
//
// The input sMsg length should not exceed MAX_LOG_RECORD_SIZE characters.
//

int TransactionLog::messageSent( char *sMsg ) {
   int rc = SUCCESS ; // optimistically

   if ( !bufferHasOverflowed ) {
      if ( strlen( sMsg ) > MAX_LOG_RECORD_SIZE ) {
         // We'll limit the log record size.
         sMsg[ MAX_LOG_RECORD_SIZE ] = '\0' ;
      }
      mostRecentResponseTime = 0 ;
      mostRecentCommandTime = RRTGetTime() ;
      // Including time + extraSpace + " --> " + nl + nul.
      char strWithTimeAndNl[ MAX_LOG_RECORD_SIZE + 6 + extraSpace + 5 + 1 + 1 ] ;
      sprintf( strWithTimeAndNl, "%6i %*s--> %s\n", mostRecentCommandTime, extraSpace, 
               "", sMsg ) ;
      if ( pTb + strlen( strWithTimeAndNl ) < tlBuffer + tlBufferSize ) {
         strcpy( pTb, strWithTimeAndNl ) ;
         pTb += strlen( strWithTimeAndNl ) ;
      } else {
         // Notify the test (but only once) that the buffer has overflowed.
         if ( !bufferHasOverflowed ) {
            // **??** TBD Consider dynamically extending (e.g., doubling)
            //            the buffer size here.                    **??**
            alert( "Transaction buffer has overflowed." ) ;
            bufferHasOverflowed = true ;
         }
      }
   } else {
      rc = BUFFER_OVERFLOWED ;
   }

   return rc ;
}

//////////////////////////////////// Method ////////////////////////////////////
//
// TransactionLog::toFile
//
// This is the implementation of the toFile method of the TransactionLog class.
//
// The implementation writes out the contents of the transaction buffer
// to a file having the usual test sequential filename and the extension given
// by the TRANSACTION_LOG_FILE_EXT constant defined by the test.
//

int TransactionLog::toFile( int testNumber, const char logFileHeader[] ) {
   const int eStrLen = 80 ;
   char      str[ eStrLen + 80 ] ; // ample
   char      eStr[ eStrLen ] ; // should be ample
   char      tlFileSpec[ 255 ] ; // ample
   const int MAX_READ_BUFFER_SIZE = sizeof str ;
   int       rc = SUCCESS ; // optimistically
   int       logSeqNo = RRTGetSequenceNumber() ;

   if ( !logSeqNo || logSeqNo == -1 ) {
      alert( "Unable to write out transaction log due to an invalid log "
             "sequence number." ) ;
      rc = COMMAND_FAILED ;
   } else if ( !RRTIsTestInProgress() ) {
      alert( "Unable to write out transaction log since test is not in "
             "progress." ) ;
      rc = COMMAND_FAILED ;
   } else {
      // Write all transactions to the transactions log file.
      FILE *fp ;
      sprintf( tlFileSpec, "%srrt%05i%s", RRTGetLogfileDirectory(), logSeqNo,
               TRANSACTION_LOG_FILE_EXT ) ;

      // Check that the transaction log file doesn't already exist.
      struct stat fi ;
      int sr = stat( tlFileSpec, &fi ) ;
      if ( sr == -1 ) {
         if ( errno != ENOENT ) {
            strerror_r( errno, eStr, eStrLen ) ;
            sprintf( str, "toFile method failed to obtain transaction log "
                     "file info for %s file: %s", tlFileSpec, eStr ) ;
            alert( str ) ;
            rc = LOG_FILE_ERROR_OCCURRED ;
         } else {
            // Create (and open) the transaction log file in the log data
            // directory.
            char testNumberString[ 10 ] ; // ample, usually 4 digits
            sprintf( testNumberString, "%d", testNumber ) ; 
            char thisTestNumber[ strlen( "Test Number " ) 
                               + strlen( testNumberString ) + 1 ] ; // with nl at end
            sprintf( thisTestNumber, "Test Number %s", testNumberString ) ;
            char fileHeader[ strlen( logFileHeader ) 
                           + strlen( thisTestNumber) ] ; // with nl at end
            // Remove ctime's nl.
            strcpy( fileHeader, logFileHeader ) ; 
            fileHeader[ strlen( fileHeader ) - 1 ] = '\0' ;
            sprintf( fileHeader, "%s,%s\n", fileHeader, thisTestNumber ) ;
            fp = fopen( tlFileSpec, "w" ) ; // write
            if ( !fp ) {
               // Open operation failed.
               strerror_r( errno, eStr, eStrLen ) ;
               sprintf( str, "toFile method failed to open %s file: %s",
                        tlFileSpec, eStr ) ;
               alert( str ) ;
               rc = LOG_FILE_ERROR_OCCURRED ;
            } else if ( fwrite( fileHeader, 1, strlen( fileHeader ), fp ) 
                        != strlen( fileHeader ) ) {
               // Write header failed on transaction log file.
               strerror_r( errno, eStr, eStrLen ) ;
               sprintf( str, "toFile method error occurred attempting to "
                        "write %s file: %s", tlFileSpec, eStr ) ;
               alert( str ) ;
               rc = LOG_FILE_ERROR_OCCURRED ;
            } else {
               // Now write the transactions to the file.
               bool moreTransactions ;
               if ( pTb > tlBuffer ) {
                  moreTransactions = true ;
               } else {
                  moreTransactions = false ;
               }
               char *pB = tlBuffer ;
               // Includes time + extraSpace + "   <-- " + nl + nul
               char tempStr[ MAX_LOG_RECORD_SIZE + 6 + extraSpace + 7 + 1 + 1 ] ;
               int bytesWritten ;
               int tsSize = sizeof tempStr - 1 ;
               while ( moreTransactions ) {
                  for ( int i = 0; i < tsSize; i++ ) {
                     if ( *pB == '\n' ) {
                        tempStr[ i ]     = *pB++ ;
                        tempStr[ i + 1 ] = '\0' ;
                        break ;
                     } else {
                        tempStr[ i ] = *pB++ ;
                     }
                  }
                  bytesWritten = fprintf( fp, "%s", tempStr ) ;
                  if ( !bytesWritten ) {
                     // An error occurred attempting to write to the file.
                     strerror_r( errno, eStr, eStrLen ) ;
                     sprintf( str, "toFile method error occurred attempting to "
                              "write %s file: %s", tlFileSpec, eStr ) ;
                     alert( str ) ;
                     rc = LOG_FILE_ERROR_OCCURRED ;
                     break ;
                  } else {
                     if ( pB >= pTb ) {
                        moreTransactions = false ;
                     }
                  }
               }

               fclose( fp ) ;
            }
         }
      } else {
         sprintf( str, "toFile method unable to create %s file; it "
                  "already exists.", tlFileSpec ) ;
         alert( str ) ;
         rc = LOG_FILE_ERROR_OCCURRED ;
      }
   }

   return rc ;
}


