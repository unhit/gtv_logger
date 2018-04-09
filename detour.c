#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "print.h"

/*
* Copyright (c) <2008>, <Henrik Friedrichsen>
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the names of the company nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY <Henrik Friedrichsen> ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL <Henrik Friedrichsen> BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

void*
detour(void* org, void* hook, int size)
{
    unsigned long mask = ~(sysconf(_SC_PAGESIZE)-1); // platform independant, 0xE9 however is not.
    unsigned long page = (unsigned long)org & mask;

    if (mprotect((void *)page, 5, PROT_READ|PROT_WRITE|PROT_EXEC) != 0) {
	error("mprotect() failure\n");
	_exit(-1);
    }

    unsigned long jmprel = (unsigned long)hook - (unsigned long)org - 5;
    unsigned char* gateway = malloc(size+5);
    memcpy(gateway, org, size);
    gateway[size] = 0xE9;
    
    *(unsigned long*)(gateway+size+1) = (unsigned long)(org+size) - (unsigned long)(gateway + size) - 5;
    
    unsigned char* p = (unsigned char*)org;
    p[0] = 0xE9;
    *(unsigned long*)(p+1) = jmprel;

    return gateway; // make sure you free() this when closing
} 
