// RRandom.cpp -- implementation file for a repeatable random class.
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

#include <assert.h>
#include <iostream>
#include <stdlib.h>
#include <math.h>
#include "EVTTestConstants.h"
#include "RRandom.h"

// The following is the largest integer returned from random().
const double MAX_RANDOM_VALUE = 2147483648.0;

//unsigned long RRandom::PrevValue[ MAX_RRANDOM_IDS ];
//int RRandom::Seed = 0;

RRandom::RRandom()
{
}

RRandom::~RRandom()
{
}

void RRandom::SetTestNumber( int tn )
{
   // We'll start the array of random values using rand.  It isn't as good
   // a random generator, but this allows us to get truly independent random
   // streams since they start on different "seeds."
   Seed = tn;
   //cout << "Seed: " << Seed << endl;
   srand( Seed );
   for ( int i = 0; i < MAX_RRANDOM_IDS ; i++ )
   {
      PrevValue[ i ] = rand();
      if ( ( PrevValue[ i ] % 2 ) != 1 )
      {
         PrevValue[ i ] += 1;
      }
      //cout << "PrevValue[" << i << "] = " << PrevValue[ i ] << endl;
   }
}

// Returns a random integer between min and max inclusively with resolution res.
int RRandom::GetIntValue( RandItems id, int minimum, int maximum, int res )
{
   assert( maximum >= minimum && res > 0 );
//cout << "PrevValue[" << id << "] = " << PrevValue[ id ] << " -> ";
   srandom( PrevValue[ id ] );
   int value = random();
   PrevValue[ id ] = value;
//cout << "PrevValue[" << id << "] = " << PrevValue[ id ] << endl;
//cout << "res: " << res << "; minimum: " << minimum
//     << "; maximum: " << maximum <<"; value: " << value << endl;
//double A = double( value ) * ( maximum + 1 - minimum ) / MAX_RANDOM_VALUE + minimum;
//cout << "A: " << A << endl;
//double B = A / res;
//cout << "B: " << B << endl;
//double C = floor( B );
//cout << "C: " << C << endl;
//double D = C * res;
//cout << "D: " << D << endl;
//int E = int( D );
//cout << "E: " << E << endl;
//cout << "return: " << int( floor( ( value * ( maximum + 1 - minimum )
//                                     / MAX_RANDOM_VALUE + minimum ) / res ) * res ) << endl;
return int( floor( ( double( value ) * ( maximum + 1 - minimum )
                                 / MAX_RANDOM_VALUE + minimum ) / res ) * res );
}

// Returns a random float between min and max, inclusively, with resolution res.
float RRandom::GetFloatValue( RandItems id, float minimum, float maximum,
                              float res )
{
   assert( maximum >= minimum && res > 0.0 );
   srandom( PrevValue[ id ] );
   int value = random();
   PrevValue[ id ] = value;

   return fmin( float( floor( ( double( value ) * ( maximum - minimum )
                                 / MAX_RANDOM_VALUE + minimum ) / res ) * res ),
          maximum );
}

// Returns 1 if a random percent is less than or equal to amount, 0 otherwise.
int RRandom::Percent( RandItems id, int amount )
{
   assert( amount <= 100 && amount >= 0 );
   int value = 0;
   int temp = GetIntValue( id, 1, 100, 1 );
   if ( temp <= amount ) value = 1;

   return value;
}
