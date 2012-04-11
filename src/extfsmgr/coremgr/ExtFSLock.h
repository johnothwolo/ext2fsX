/*
* Copyright 2004 Brian Bergstrand.
*
* Redistribution and use in source and binary forms, with or without modification, 
* are permitted provided that the following conditions are met:
*
* 1.	Redistributions of source code must retain the above copyright notice, this list of
*     conditions and the following disclaimer.
* 2.	Redistributions in binary form must reproduce the above copyright notice, this list of
*     conditions and the following disclaimer in the documentation and/or other materials provided
*     with the distribution.
* 3.	The name of the author may not be used to endorse or promote products derived from this
*     software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
* AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
* THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/

/*
 * This is a private implementation file. It should never be accessed by ExtFSDiskManager clients.
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <pthread.h>
#ifdef DIAGNOSTIC
#include <stdio.h>
#endif

#ifdef DIAGNOSTIC
    #define ERET int eerr =
#else
    #define ERET (void)
#endif

#ifdef DIAGNOSTIC
    #define ERETCHK() if (eerr) fprintf(stderr, "ExtFS -%s-: lock failed!\n", __PRETTY_FUNCTION__)
#else
    #define ERETCHK() {}
#endif

static __inline__
void ewlock(void *l)
{
    ERET pthread_rwlock_wrlock((pthread_rwlock_t*)l);
    ERETCHK();
}
static __inline__
void erlock(void *l)
{
    ERET pthread_rwlock_rdlock((pthread_rwlock_t*)l);
    ERETCHK();
}
static __inline__
void eulock(void *l)
{
    ERET pthread_rwlock_unlock((pthread_rwlock_t*)l);
    ERETCHK();
}
static __inline__
void euplock(void *l) // Upgrade lock to write
{
    eulock(l);
    ewlock(l);
}

static __inline__
int eilock(void **l)
{
    int eerr = ENOMEM;
    *l = malloc(sizeof(pthread_rwlock_t));
    if (NULL != *l) {
        if (0 != (eerr = pthread_rwlock_init((pthread_rwlock_t*)*l, NULL)))
            free(l);
    }
    return (eerr);
}
static __inline__
void edlock(void *l)
{
    ERET pthread_rwlock_destroy((pthread_rwlock_t*)l);
    ERETCHK();
    free(l);
}
