// RRTGen.h -- header file for a repeatable random test generator.
//
//                                             <by Cary WR Campbell>
//
// This software is copyrighted © 2007 - 2020 by Codecraft, Inc.
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

#include <string.h>
#include <stdio.h>
#include "RRTGenClasses.h"

// Comment out the following line to omit static requirements from log.
#define LOG_STATIC_REQUIREMENTS

//typedef unsigned char byte;

#define RRTGEN_VERSION "v.2.0"

typedef struct {
   unsigned int eventTime ;
   unsigned int value ;
   unsigned int value2 ;
} Notification ;

// Coroutines and handlers.
void         addAlertMessage( const char messageLetter ) ;
bool         addModelInstance( FSM *modelInstance ) ;
void         alert( const char *message ) ;          // defined in test module
int          chronicleTestRun( const char testRunsCsvFile[],
                               const char sut[], const char machine[],
                               int testNumber, unsigned int sequenceNumber,
                               int score, bool passed, const char alerts[] ) ;
void         dashTrailing( char *str ) ;
void         enterCriticalSection( unsigned short *csVar ) ;
bool         extractVersionInformation( char *pathBuffer,
                                        unsigned char &minFileRev,      // ByRef
                                        unsigned char &majFileRev,      // ByRef
                                        unsigned char &minProductRev,   // ByRef
                                        unsigned char &majProductRev,   // ByRef
                                        unsigned char &unusedProductRev,// ByRef
                                        unsigned char &buildNumProductRev ) ;
                                                                        // ByRef
char         *getAlertMessagePtr( const char messageLetter ) ;
bool         getTestVersionInfo( unsigned char &minorProductRev,         // ByRef
                                 unsigned char &majorProductRev,         // ByRef
                                 unsigned char &unusedProductRev,        // ByRef
                                 unsigned char &buildNumProductRev ) ;   // ByRef
unsigned int getTimeInMs( void ) ;
int          kbhit( void ) ;
void         leaveCriticalSection( unsigned short *csVar ) ;
void         modelThread( Fifo *normalQ, Fifo *priorityQ, FSM **pFsmArray,
                          char *modelName ) ;  // coroutine
void         notificationHandler( const Notification &notification ) ;
void         notificationScrubber( void ) ;    // coroutine
void         outputAlertMessages( void ) ;
FSM          *removeModelInstance( FSM *modelInstance ) ;
void         roundtripCounter( void ) ;        // coroutine
int          RRTGetErrorStatus( void ) ;       // defined in test module
char         *RRTGetLogfileHeader( void ) ;    // defined in test module
char         *RRTGetLogfileDirectory( void ) ; // defined in test module
int          RRTGetSequenceNumber( void ) ;    // defined in test module
unsigned int RRTGetTime( void ) ;              // defined in test module
bool         RRTIsTestInProgress( void ) ;     // defined in test module
void         RRTLog( const char *message ) ;   // defined in test module
char         *strlwr( char *string ) ;         // missing in Unix string.h
char         *strupr( char *string ) ;         // missing in Unix string.h

enum {
   COMMAND_FAILED                             =  -1,
   SUCCESS                                    =   0,
   BUFFER_OVERFLOWED,
   ERROR_OCCURRED_DURING_TEST,
   LOG_FILE_ERROR_OCCURRED,
   SEQUENCE_DOT_DAT_FILE_MISSING,
   SEQUENCE_DOT_DAT_FILE_ERROR_OCCURRED,
   SEQUENCE_DOT_DAT_FILE_HAS_BAD_FORMAT,
   TEST_ALREADY_IN_PROGRESS,
   TEST_NOT_IN_PROGRESS
} ;

// Global constants for use by multiple components.
extern char       alert_list[] ;
//const int         broadcast                   =   0;
extern FSM        *broadcast ;
extern FSM        *DEVICE_INTERFACE ;
const char        DEVICE_INTERFACE_NAME[]     = "device interface" ;
const unsigned int INVALID_SEQUENCE_NUMBER    = 0xFFFFFFFF ;
const int         MAX_ALERT_MESSAGES          =  10 ;
const int         MAX_LOG_RECORD_SIZE         = 160 ;
const int         MAX_TEST_DESCRIPTION_LENGTH =  50 ;
extern FSM        *MODEL_INTERFACE ;
const char        MODEL_INTERFACE_NAME[]      = "model interface" ;
extern FSM        *RRTGEN_FRAMEWORK ;
const char        RRTGEN_FRAMEWORK_NAME[]     = "RRTGen framework" ;
const char        sequenceDotDatFile[]        = "sequence.dat" ;

// Global variables output from the model compiler.
extern const char REQUIREMENTS_CSV_FILE[] ;
extern FSM        *theModel[] ;
extern unsigned int TOTAL_REQ_COUNT ;
extern const char TRANSACTION_LOG_FILE_EXT[] ;

// Ring buffer for the interface to the DUT server.
typedef struct {
   unsigned short isStale ;                     // used by NotificationScrubber
   unsigned int   eventTime ;

   // From notification by interface to DUT.
   unsigned int   value ;
   unsigned int   value2 ;
} ringBufferElementType ;

const int         NUMBER_OF_RING_BUFFER_SLOTS =  40 ;
typedef struct {
   volatile unsigned int nextWriteIndex ;
   volatile unsigned int currentReadIndex ;
   ringBufferElementType rbuf[ NUMBER_OF_RING_BUFFER_SLOTS ] ;
   bool                  overrunOccurred ;
} notificationFifo ;

// Internal classes.
class RepeatableRandomTest {
   // This class establishes the Repeatable Random Test environment for
   // exectuion of a given test run.  The default directory must contain
   // a file named sequential.dat that specifies the directory to log
   // into and the next logfile number (comma-separated on one line).  The
   // sequential logfile number is automatically updated in preparation
   // for the next test run.
   //
   // GetTime returns the number of ms since the test started, or 0 if a test
   // is not in progress.  The same time reference is used in the log and
   // test files.

   FILE         *f_dotLog ;
   FILE         *f_sequenceDotDat ;
   FILE         *f_dotTst ;

   char         dotLogFile[ 255 ] ; // ample
   char         dotTstFile[ 255 ] ; // ample
   int          errorStatus ;
   char         fileHeaderStr[ MAX_TEST_DESCRIPTION_LENGTH + 26 + 2 ] ;
   char         logfileDirectory[ 255 ] ; // ample (includes trailing '/')
   int          logfileNumber ;
   unsigned int startTime ;
   char         rrtgenVersion[ 10 ] ; // adequate
   char         testDescription[ MAX_TEST_DESCRIPTION_LENGTH + 1 ] ; // with nul
   bool         testIsInProgress ;

   int tstLog( const char *str ) ;   // writes an entry in the .tst file

   RepeatableRandomTest() ;         // default CTOR is disallowed

 public:

   RepeatableRandomTest( const char *testDesc )
   : errorStatus( SUCCESS ),
     rrtgenVersion ( RRTGEN_VERSION ),
     logfileNumber( INVALID_SEQUENCE_NUMBER ),
     testIsInProgress( false ) {
      strncpy( testDescription, testDesc, MAX_TEST_DESCRIPTION_LENGTH ) ;
      testDescription[ MAX_TEST_DESCRIPTION_LENGTH ] = '\0' ; // for safety
   }

   ~RepeatableRandomTest() {
   }

   int  abortTest( void ) ;
   int  finishTest( void ) ;
   bool isTestInProgress( void ) ;
   int  log( const char *str ) ;
   int  getErrorStatus( void ) { return errorStatus ; }
   char *getRRTGenVersion( void ) { return rrtgenVersion ; }
   char *getLogfileHeader( void ) { return fileHeaderStr ; }
   char *getLogfileDirectory( void ) { return logfileDirectory ; }
   int  getSequenceNumber( void ) { return logfileNumber ; }

   unsigned int getTime() {
      if ( testIsInProgress ) {
         return getTimeInMs() - startTime ;
      } else {
         return 0 ;
      }
   }

   void getErrorStatus( int errStatus ) {
      // We'll just ignore a request to override an existing error.
      if ( errorStatus == SUCCESS ) {
         errorStatus = errStatus ;
      }
   }

   int  startTest( void ) ;
};

class TransactionLog {
   bool bufferHasOverflowed ;
   int  tlBufferSize ;
   char *pTb ; // pointer to the next available byte
   char *tlBuffer ;

 public:

   TransactionLog() ;
   ~TransactionLog() ;

   int messageReceived( char *rMsg, bool fromModel = false ) ;
   int messageSent( char *rMsg ) ;
   int toFile( void ) ;
};

class RequirementsTallyHandler {
   const static int count = 0xFFFF + 1 ; // 64K
   unsigned short *talliesMet ;             // requirement consequent executed
   unsigned short *talliesEncountered ;     // requirement encountered

 public:

   RequirementsTallyHandler() ;
   ~RequirementsTallyHandler() ;

   void tallyEncountered( unsigned short requirementId ) ;
   void tallyMet( unsigned short requirementId ) ;
   int  toFile( unsigned int totalRequirementsCount ) ;
   int  toLog( void ) ;
};

extern RequirementsTallyHandler rth ;

#define _Re(id) rth.tallyEncountered( id )
#define _Rm(id) rth.tallyMet( id )
#define _R(id) rth.tallyEncountered( id );rth.tallyMet( id )

#ifdef LOG_STATIC_REQUIREMENTS
#define _Rs(id) _R(id)
#else
#define _Rs(id)
#endif  // def LOG_STATIC_REQUIREMENTS

