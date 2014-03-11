//========= Copyright Valve Corporation, All rights reserved. ============//
//
// intelglmallocworkaround.h
//  class responsible for setting up a malloc override that zeroes allocated 
//  memory of less than 96 bytes. this is to work around a bug
//  in the Intel GLSL compiler on Mac OS X 10.8 due to uninitialized memory.
//  
//  96 was chosen due to this quote from Apple: 
//    "I verified that the size of the structure is exactly 64 bytes on 10.8.3, 10.8.4 and will be on 10.8.5."
//
//  certain GLSL shaders would (intermittently) cause a crash the first time they
//  were drawn, and the bug has supposedly been fixed in 10.9, but is unlikely to
//  ever make it to 10.8.
//
//===============================================================================

#ifndef INTELGLMALLOCWORKAROUND_H
#define	INTELGLMALLOCWORKAROUND_H

#include <stdlib.h>

class IntelGLMallocWorkaround
{
public:
	static IntelGLMallocWorkaround *Get();
	bool Enable();

protected:
	IntelGLMallocWorkaround() :m_pfnMallocReentry(NULL) {}
	~IntelGLMallocWorkaround() {}

	static IntelGLMallocWorkaround *s_pWorkaround;
	static void* ZeroingAlloc(size_t);

	typedef void* (*pfnMalloc_t)(size_t);
	pfnMalloc_t m_pfnMallocReentry;
};

#endif // INTELGLMALLOCWORKAROUND_H