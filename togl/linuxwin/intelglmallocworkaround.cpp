//========= Copyright Valve Corporation, All rights reserved. ============//
#include "intelglmallocworkaround.h"
#include "mach_override.h"

// memdbgon -must- be the last include file in a .cpp file.
#include "tier0/memdbgon.h"

IntelGLMallocWorkaround* IntelGLMallocWorkaround::s_pWorkaround = NULL;

void *IntelGLMallocWorkaround::ZeroingAlloc(size_t size)
{
	// We call into this pointer that resumes the original malloc.
	void *memory = s_pWorkaround->m_pfnMallocReentry(size);
	if (size < 96)
	{
		// Since the Intel driver has an issue with a small allocation 
		// that's left uninitialized, we use memset to ensure it's zero-initialized.
		memset(memory, 0, size);
	}

	return memory;
}

IntelGLMallocWorkaround* IntelGLMallocWorkaround::Get()
{
	if (!s_pWorkaround)
	{
		s_pWorkaround = new IntelGLMallocWorkaround();
	}

	return s_pWorkaround;
}

bool IntelGLMallocWorkaround::Enable()
{
	if ( m_pfnMallocReentry != NULL )
	{
		return true;
	}

	mach_error_t error = mach_override_ptr( (void*)&malloc, (const void*)&ZeroingAlloc, (void**)&m_pfnMallocReentry );
	if ( error == err_cannot_override )
	{
		m_pfnMallocReentry = NULL;
		return false;
	}

	return true;
}