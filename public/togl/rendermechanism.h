//========= Copyright Valve Corporation, All rights reserved. ============//
#ifndef RENDERMECHANISM_H
#define RENDERMECHANISM_H

#if defined(DX_TO_GL_ABSTRACTION)

#undef PROTECTED_THINGS_ENABLE

#include <GL/gl.h>
#include <GL/glext.h>

#include "tier0/basetypes.h"
#include "tier0/platform.h"

#include "togl/linuxwin/glmdebug.h"
#include "togl/linuxwin/glbase.h"
#include "togl/linuxwin/glentrypoints.h"
#include "togl/linuxwin/glmdisplay.h"
#include "togl/linuxwin/glmdisplaydb.h"
#include "togl/linuxwin/glmgrbasics.h"
#include "togl/linuxwin/glmgrext.h"
#include "togl/linuxwin/cglmbuffer.h"
#include "togl/linuxwin/cglmtex.h"
#include "togl/linuxwin/cglmfbo.h"
#include "togl/linuxwin/cglmprogram.h"
#include "togl/linuxwin/cglmquery.h"
#include "togl/linuxwin/glmgr.h"
#include "togl/linuxwin/dxabstract_types.h"
#include "togl/linuxwin/dxabstract.h"

#else
	//USE_ACTUAL_DX
	#ifdef WIN32
		#ifdef _X360
			#include "d3d9.h"
			#include "d3dx9.h"
		#else
			#include <windows.h>
			#include "../../dx9sdk/include/d3d9.h"
			#include "../../dx9sdk/include/d3dx9.h"
		#endif
		typedef HWND VD3DHWND;
	#endif

	#define	GLMPRINTF(args)	
	#define	GLMPRINTSTR(args)
	#define	GLMPRINTTEXT(args)
#endif // defined(DX_TO_GL_ABSTRACTION)

#endif // RENDERMECHANISM_H
