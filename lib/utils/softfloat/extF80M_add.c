
/*============================================================================

This C source file is part of the SoftFloat IEEE Floating-Point Arithmetic
Package, Release 3e, by John R. Hauser.

Copyright 2011, 2012, 2013, 2014 The Regents of the University of California.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions, and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions, and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

 3. Neither the name of the University nor the names of its contributors may
    be used to endorse or promote products derived from this software without
    specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS", AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ARE
DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=============================================================================*/

#include <stdbool.h>
#include "sbi_utils/softfloat/platform.h"
#include "sbi_utils/softfloat/internals.h"
#include "sbi_utils/softfloat/softfloat.h"

#ifdef SOFTFLOAT_FAST_INT64

void
 extF80M_add(
     const extFloat80_t *aPtr, const extFloat80_t *bPtr, extFloat80_t *zPtr )
{
    const struct extFloat80M *aSPtr, *bSPtr;
    uint_fast16_t uiA64;
    uint_fast64_t uiA0;
    bool signA;
    uint_fast16_t uiB64;
    uint_fast64_t uiB0;
    bool signB;
#if ! defined INLINE_LEVEL || (INLINE_LEVEL < 2)
    extFloat80_t
        (*magsFuncPtr)(
            uint_fast16_t, uint_fast64_t, uint_fast16_t, uint_fast64_t, bool );
#endif

    aSPtr = (const struct extFloat80M *) aPtr;
    bSPtr = (const struct extFloat80M *) bPtr;
    uiA64 = aSPtr->signExp;
    uiA0  = aSPtr->signif;
    signA = signExtF80UI64( uiA64 );
    uiB64 = bSPtr->signExp;
    uiB0  = bSPtr->signif;
    signB = signExtF80UI64( uiB64 );
#if defined INLINE_LEVEL && (2 <= INLINE_LEVEL)
    if ( signA == signB ) {
        *zPtr = softfloat_addMagsExtF80( uiA64, uiA0, uiB64, uiB0, signA );
    } else {
        *zPtr = softfloat_subMagsExtF80( uiA64, uiA0, uiB64, uiB0, signA );
    }
#else
    magsFuncPtr =
        (signA == signB) ? softfloat_addMagsExtF80 : softfloat_subMagsExtF80;
    *zPtr = (*magsFuncPtr)( uiA64, uiA0, uiB64, uiB0, signA );
#endif

}

#else

void
 extF80M_add(
     const extFloat80_t *aPtr, const extFloat80_t *bPtr, extFloat80_t *zPtr )
{

    softfloat_addExtF80M(
        (const struct extFloat80M *) aPtr,
        (const struct extFloat80M *) bPtr,
        (struct extFloat80M *) zPtr,
        false
    );

}

#endif

