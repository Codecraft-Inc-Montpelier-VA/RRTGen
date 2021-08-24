// RRTGenClasses.h -- header file for Fifo and FSM classes for RRTGen.
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

// This file is automatically included in the model-compiler-generated
// XXXModel_Insert.h file.
//

#pragma once

// Uncomment the next line to show FSM's CTOR and DTOR execution.
//#define DEBUGGING
#ifdef DEBUGGING
#include <iostream>
using namespace std;
#endif // def DEBUGGING

#include <string.h>                    // strncpy (for Cygwin)

const char blank = ' ';

// Pure virtual base class for model components.
//

class FSM
{
   FSM() {}                            // default CTOR not supported

 protected:
   int   theInstance;
   state currentState;
   char  name[MAX_NAME_LENGTH + 1];  // with room for nul at end

 public:
    FSM(int instance, state initState, const char *handle)
     : theInstance(instance), currentState(initState)
    {
       strncpy(name, handle, MAX_NAME_LENGTH);
       name[MAX_NAME_LENGTH] = '\0';  // for safety

       #ifdef DEBUGGING
       char str[80]; // ample
       sprintf(str, "<<< CTOR: %s (%i)", GetName(), GetInstance());
       cout << str << endl;
       #endif // def DEBUGGING
    }
    virtual ~FSM()
    {
       #ifdef DEBUGGING
       char str[80]; // ample
       sprintf(str, ">>> DTOR: %s (%i)", GetName(), GetInstance());
       cout << str << endl;
       #endif // def DEBUGGING
    }
    int     GetInstance(void) {return theInstance;}
    char    *GetName(void) {return name;}
    virtual void Update(event theEvent, char parm1, char parm2) = 0;
};

class Id
{
   char name[MAX_NAME_LENGTH + 1];
   int  instance;

   Id() {}  // default CTOR not supported

 public:
   Id(const char *toName, int toInstance)
    : instance(toInstance)
   {
      strncpy(name, toName, MAX_NAME_LENGTH);
      name[MAX_NAME_LENGTH] = '\0';  // for safety
   }
   virtual ~Id() {}

   int     GetInstance(void) {return instance;}
   char    *GetName(void) {return name;}
};

enum
{
   FIFO_SUCCESS                    = 0,
   OVERRUN_OCCURRED
};

typedef struct {
   event             theEvent;
   FSM               *source;
   FSM               *destination; // FSM or 0 (broadcast)
   char              parm1;
   char              parm2;
} bufferElementType;

// Note that no critical section is necessary, since only the consumer
// manipulates the read pointer and only the producer manipulates the
// write pointer.  The queue is empty when the pointers are equal.
// We use only n - 1 slots so the write pointer can't reach the read
// pointer "from the back."
class Fifo
{
 public:
   Fifo(const char *fifoName)
   {
      currentReadIndex = nextWriteIndex = 0;
      overrunOccurred = false;
      strncpy(name, fifoName, MAX_NAME_LENGTH - 1);
      name[MAX_NAME_LENGTH] = '\0'; // for safety
   }

   ~Fifo() {}

   char *GetName(void) { return name; }

   bool IsEmpty(void)
   {
      return overrunOccurred ? false : nextWriteIndex == currentReadIndex;
   }

   int PushEvent(event theEvent, char parm1, char parm2,
                  FSM *sender, FSM *destination)
   {
      int rc = FIFO_SUCCESS; // assumed

      // Make sure there is room in the ring buffer for this sample.
      if (((nextWriteIndex == NUMBER_OF_MODEL_BUFFER_SLOTS - 1)
          && (currentReadIndex == 0))
        || (nextWriteIndex + 1 == currentReadIndex)) {
         overrunOccurred = true;
         rc = OVERRUN_OCCURRED;
      } else {
         // Insert the event and its envelope in our circular buffer.
         buf[nextWriteIndex].theEvent    = theEvent;
         buf[nextWriteIndex].parm1       = parm1;
         buf[nextWriteIndex].parm2       = parm2;
         buf[nextWriteIndex].source      = sender;
         buf[nextWriteIndex].destination = destination;

         // Bump the write index.
         if (++nextWriteIndex == NUMBER_OF_MODEL_BUFFER_SLOTS) {
            nextWriteIndex = 0;
         }
      }

      return rc;
   }

   int PopEvent(event &returnedEvent, char &returnedParm1, char &returnedParm2,
                 FSM **returnedSender, FSM **returnedDestination)
   {
      int rc = FIFO_SUCCESS; // assumed

      if (overrunOccurred) {
         rc = OVERRUN_OCCURRED;
      } else {
         // Grab the event and envelope.
         returnedEvent        = buf[currentReadIndex].theEvent;
         returnedParm1        = buf[currentReadIndex].parm1;
         returnedParm2        = buf[currentReadIndex].parm2;
         *returnedSender      = buf[currentReadIndex].source;
         *returnedDestination = buf[currentReadIndex].destination;

         // Remove the event.
         if (currentReadIndex == NUMBER_OF_MODEL_BUFFER_SLOTS - 1) {
            currentReadIndex = 0;
         } else {
            currentReadIndex++;
         }

         rc = FIFO_SUCCESS;
      }

      return rc;
   }

 private:
   unsigned int      nextWriteIndex;
   unsigned int      currentReadIndex;
   bufferElementType buf[NUMBER_OF_MODEL_BUFFER_SLOTS];
   bool              overrunOccurred;
   char              name[MAX_NAME_LENGTH + 1]; // with room for nul at end
};

// Function prototypes.
char  *EventText(event theEvent);
void  SendEvent(event theEvent, char parm1 = ' ', char parm2 = ' ',
                 FSM *to = 0, FSM *from = 0);
void  SendEventToId(event theEvent, char parm1 = ' ', char parm2 = ' ',
                     Id *to = 0, FSM *from = 0);
void  SendPriorityEvent(event theEvent, char parm1 = ' ', char parm2 = ' ');
void  SendDelayedEvent(int msToWait, event theEvent, char parm1, char parm2,
                        FSM *to, FSM *from, bool *continuing);
void  SendDelayedEventToId(int msToWait, event theEvent, char parm1,
                            char parm2, Id *to, FSM *from, bool *continuing);
char  *StateText(state theState);

