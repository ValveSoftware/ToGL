//========= Copyright Valve Corporation, All rights reserved. ============//
//                       TOGL CODE LICENSE
//
//  Copyright 2011-2014 Valve Corporation
//  All Rights Reserved.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
#if 0	// this whole thing is turned off...

//------------------------------------------------------------------------------
// DX9AsmToGL.cpp
//------------------------------------------------------------------------------

#include "tier0/dbg.h"
#include "tier1/strtools.h"
#include "tier1/utlbuffer.h"
#include "togl/rendermechansim.h"
#include "dx9asmtogL.h"
#ifdef POSIX
#include "glmgr/glmgrbasics.h"
#else
	// normally we would get this form glmgr.h.  on a non-POSIX build of this code, we just sneak it in here
	#define	kGLMUserClipPlaneParamBase	253
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef POSIX
#define strcat_s( a, b, c) V_strcat( a, c, b )
#endif

#define DST_REGISTER		0
#define SRC_REGISTER		1

// Tracking and naming sampler dimensions
#define SAMPLER_TYPE_2D		0
#define SAMPLER_TYPE_CUBE	1
#define SAMPLER_TYPE_3D		2

// Code for which component of the "dummy" address register is needed by an instruction
#define ARL_DEST_NONE		-1
#define ARL_DEST_X			 0
#define ARL_DEST_Y			 1
#define ARL_DEST_Z			 2
#define ARL_DEST_W			 3

#ifndef POSIX
#define Debugger() Assert(0)
#endif

char g_pSamplerStrings[3][5] = { "2D", "CUBE", "3D" };


float uint32ToFloat_ASM( uint32 dw )
{
	return *((float*)&dw);
}

uint32 D3DToGL_ASM::GetNextToken( void )
{
	uint32 dwToken = *m_pdwNextToken;
	m_pdwNextToken++;
	return dwToken;
}

void D3DToGL_ASM::SkipTokens( uint32 numToSkip )
{
	m_pdwNextToken += numToSkip;
}

uint32 D3DToGL_ASM::Opcode( uint32 dwToken )
{
	return ( dwToken & D3DSI_OPCODE_MASK );
}

uint32 D3DToGL_ASM::OpcodeSpecificData (uint32 dwToken)
{
	return ( ( dwToken & D3DSP_OPCODESPECIFICCONTROL_MASK ) >> D3DSP_OPCODESPECIFICCONTROL_SHIFT );
}

uint32 D3DToGL_ASM::TextureType ( uint32 dwToken )
{
	return ( dwToken & D3DSP_TEXTURETYPE_MASK ); // Note this one doesn't shift due to weird D3DSAMPLER_TEXTURE_TYPE enum
}



// Print GLSL intrinsic corresponding to particular instruction
void D3DToGL_ASM::OpenIntrinsic( uint32 inst, char* buff, int nBufLen )
{
	switch ( inst )
	{
		case D3DSIO_RSQ:
			V_snprintf( buff, nBufLen, "inversesqrt( " );
			break;
		case D3DSIO_DP3:
		case D3DSIO_DP4:
			V_snprintf( buff, nBufLen, "dot( " );
			break;
		case D3DSIO_MIN:
			V_snprintf( buff, nBufLen, "min( " );
			break;
		case D3DSIO_MAX:
			V_snprintf( buff, nBufLen, "max( " );
			break;
		case D3DSIO_SLT:
			V_snprintf( buff, nBufLen, "lessThan( " );
			break;
		case D3DSIO_SGE:
			V_snprintf( buff, nBufLen, "greaterThan( " );
			break;
		case D3DSIO_EXP:
			V_snprintf( buff, nBufLen, "exp( " );  // exp2 ?
			break;
		case D3DSIO_LOG:
			V_snprintf( buff, nBufLen, "log( " );	// log2 ?
			break;
		case D3DSIO_LIT:
			Assert(0);
			V_snprintf( buff, nBufLen, "lit( " ); // gonna have to write this one
			break;
		case D3DSIO_DST:
			Assert(0);
			V_snprintf( buff, nBufLen, "dst( " ); // gonna have to write this one
			break;
		case D3DSIO_LRP:
			Assert( !m_bVertexShader );
			V_snprintf( buff, nBufLen, "mix( " );
			break;
		case D3DSIO_FRC:
			V_snprintf( buff, nBufLen, "fract( " );
			break;
		case D3DSIO_M4x4:
			Assert(0);
			V_snprintf( buff, nBufLen, "m4x4" );
			break;
		case D3DSIO_M4x3:
		case D3DSIO_M3x4:
		case D3DSIO_M3x3:
		case D3DSIO_M3x2:
		case D3DSIO_CALL:
		case D3DSIO_CALLNZ:
		case D3DSIO_LOOP:
		case D3DSIO_RET:
		case D3DSIO_ENDLOOP:
		case D3DSIO_LABEL:
		case D3DSIO_DCL:
			Assert(0);
			break;
		case D3DSIO_POW:
			V_snprintf( buff, nBufLen, "pow( " );
			break;
		case D3DSIO_CRS:
			V_snprintf( buff, nBufLen, "cross( " );
			break;
		case D3DSIO_SGN:
			Assert(0);
			V_snprintf( buff, nBufLen, "sign( " );
			break;
		case D3DSIO_ABS:
			V_snprintf( buff, nBufLen, "abs( " );
			break;
		case D3DSIO_NRM:
			Assert( 0 );
			V_snprintf( buff, nBufLen, "normalize( " );
			break;
		case D3DSIO_SINCOS:
			Assert( 0 );
			V_snprintf( buff, nBufLen, "sincos( " ); // gonna have to write this one
			break;
		case D3DSIO_REP:
		case D3DSIO_ENDREP:
		case D3DSIO_IF:
		case D3DSIO_IFC:
		case D3DSIO_ELSE:
		case D3DSIO_ENDIF:
		case D3DSIO_BREAK:
		case D3DSIO_BREAKC:		// TODO: these are the reason we even need GLSL...gotta make these work
			Assert(0);
			break;
		case D3DSIO_DEFB:
		case D3DSIO_DEFI:
			Assert(0);
			break;
		case D3DSIO_TEXCOORD:
			V_snprintf( buff, nBufLen, "texcoord" );
			break;
		case D3DSIO_TEXKILL:
			V_snprintf( buff, nBufLen, "kill( " ); // wrap the discard instruction?
			break;
		case D3DSIO_TEX:
			Assert(0);
			V_snprintf( buff, nBufLen, "TEX" );		// We shouldn'g get here
			break;
		case D3DSIO_TEXBEM:
		case D3DSIO_TEXBEML:
		case D3DSIO_TEXREG2AR:
		case D3DSIO_TEXREG2GB:
		case D3DSIO_TEXM3x2PAD:
		case D3DSIO_TEXM3x2TEX:
		case D3DSIO_TEXM3x3PAD:
		case D3DSIO_TEXM3x3TEX:
		case D3DSIO_TEXM3x3SPEC:
		case D3DSIO_TEXM3x3VSPEC:
			Assert(0);
			break;
		case D3DSIO_EXPP:
			V_snprintf( buff, nBufLen, "exp( " );
			break;
		case D3DSIO_LOGP:
			V_snprintf( buff, nBufLen, "log( " );
			break;
		case D3DSIO_CND:
			Assert(0);
			break;
		case D3DSIO_DEF:
			Assert(0);
			V_snprintf( buff, nBufLen, "DEF" );
			break;
		case D3DSIO_TEXREG2RGB:
		case D3DSIO_TEXDP3TEX:
		case D3DSIO_TEXM3x2DEPTH:
		case D3DSIO_TEXDP3:
		case D3DSIO_TEXM3x3:
			Assert(0);
			break;
		case D3DSIO_TEXDEPTH:
			V_snprintf( buff, nBufLen, "texdepth" );
			break;
		case D3DSIO_CMP:
			Assert(0);
			Assert( !m_bVertexShader );
			V_snprintf( buff, nBufLen, "CMP" );
			break;
		case D3DSIO_BEM:
			Assert(0);
			break;
		case D3DSIO_DP2ADD:
			Assert(0);
			break;
		case D3DSIO_DSX:
		case D3DSIO_DSY:
			Assert(0);
			break;
		case D3DSIO_TEXLDD:
			V_snprintf( buff, nBufLen, "texldd" );
			break;
		case D3DSIO_SETP:
			Assert(0);
			break;
		case D3DSIO_TEXLDL:
			V_snprintf( buff, nBufLen, "texldl" );
			break;
		case D3DSIO_BREAKP:
		case D3DSIO_PHASE:
			Assert(0);
			break;
	}
}


// Print ASM opcode
void D3DToGL_ASM::PrintOpcode( uint32 inst, char* buff, int nBufLen )
{
	switch ( inst )
	{
		case D3DSIO_NOP:
			V_snprintf( buff, nBufLen, "NOP" );
			Assert(0);
			break;
		case D3DSIO_MOV:
			V_snprintf( buff, nBufLen, "MOV" );
			break;
		case D3DSIO_ADD:
			V_snprintf( buff, nBufLen, "ADD" );
			break;
		case D3DSIO_SUB:
			V_snprintf( buff, nBufLen, "SUB" );
			break;
		case D3DSIO_MAD:
			V_snprintf( buff, nBufLen, "MAD" );
			break;
		case D3DSIO_MUL:
			V_snprintf( buff, nBufLen, "MUL" );
			break;
		case D3DSIO_RCP:
			V_snprintf( buff, nBufLen, "RCP" );
			break;
		case D3DSIO_RSQ:
			V_snprintf( buff, nBufLen, "RSQ" );
			break;
		case D3DSIO_DP3:
			V_snprintf( buff, nBufLen, "DP3" );
			break;
		case D3DSIO_DP4:
			V_snprintf( buff, nBufLen, "DP4" );
			break;
		case D3DSIO_MIN:
			V_snprintf( buff, nBufLen, "MIN" );
			break;
		case D3DSIO_MAX:
			V_snprintf( buff, nBufLen, "MAX" );
			break;
		case D3DSIO_SLT:
			V_snprintf( buff, nBufLen, "SLT" );
			break;
		case D3DSIO_SGE:
			V_snprintf( buff, nBufLen, "SGE" );
			break;
		case D3DSIO_EXP:
			V_snprintf( buff, nBufLen, "EX2" );
			break;
		case D3DSIO_LOG:
			V_snprintf( buff, nBufLen, "LG2" );
			break;
		case D3DSIO_LIT:
			V_snprintf( buff, nBufLen, "LIT" );
			break;
		case D3DSIO_DST:
			V_snprintf( buff, nBufLen, "DST" );
			break;
		case D3DSIO_LRP:
			Assert( !m_bVertexShader );
			V_snprintf( buff, nBufLen, "LRP" );
			break;
		case D3DSIO_FRC:
			V_snprintf( buff, nBufLen, "FRC" );
			break;
		case D3DSIO_M4x4:
			V_snprintf( buff, nBufLen, "m4x4" );
			break;
		case D3DSIO_M4x3:
		case D3DSIO_M3x4:
		case D3DSIO_M3x3:
		case D3DSIO_M3x2:
		case D3DSIO_CALL:
		case D3DSIO_CALLNZ:
		case D3DSIO_LOOP:
		case D3DSIO_RET:
		case D3DSIO_ENDLOOP:
		case D3DSIO_LABEL:
			Assert(0);
			break;
		case D3DSIO_DCL:
			V_snprintf( buff, nBufLen, "DCL" );
			break;
		case D3DSIO_POW:
			V_snprintf( buff, nBufLen, "POW" );
			break;
		case D3DSIO_CRS:
			V_snprintf( buff, nBufLen, "XPD" );
			break;
		case D3DSIO_SGN:
			Assert(0);
			V_snprintf( buff, nBufLen, "SGN" );
			break;
		case D3DSIO_ABS:
			V_snprintf( buff, nBufLen, "ABS" );
			break;
		case D3DSIO_NRM:
			Assert( 0 );
			V_snprintf( buff, nBufLen, "NRM" );
			break;
		case D3DSIO_SINCOS:
			Assert( !m_bVertexShader );
			V_snprintf( buff, nBufLen, "SCS" );
			break;
		case D3DSIO_REP:
		case D3DSIO_ENDREP:
		case D3DSIO_IF:
		case D3DSIO_IFC:
		case D3DSIO_ELSE:
		case D3DSIO_ENDIF:
		case D3DSIO_BREAK:
		case D3DSIO_BREAKC:
			Assert(0);
			break;
		case D3DSIO_MOVA:
			Assert( m_bVertexShader );
			V_snprintf( buff, nBufLen, "MOV" ); // We're always moving into a temp instead, so this is MOV instead of ARL
			break;
		case D3DSIO_DEFB:
		case D3DSIO_DEFI:
			Assert(0);
			break;
		case D3DSIO_TEXCOORD:
			V_snprintf( buff, nBufLen, "texcoord" );
			break;
		case D3DSIO_TEXKILL:
			V_snprintf( buff, nBufLen, "KIL" );
			break;
		case D3DSIO_TEX:
			V_snprintf( buff, nBufLen, "TEX" );
			break;
		case D3DSIO_TEXBEM:
		case D3DSIO_TEXBEML:
		case D3DSIO_TEXREG2AR:
		case D3DSIO_TEXREG2GB:
		case D3DSIO_TEXM3x2PAD:
		case D3DSIO_TEXM3x2TEX:
		case D3DSIO_TEXM3x3PAD:
		case D3DSIO_TEXM3x3TEX:
		case D3DSIO_TEXM3x3SPEC:
		case D3DSIO_TEXM3x3VSPEC:
			Assert(0);
			break;
		case D3DSIO_EXPP:
			V_snprintf( buff, nBufLen, "EXP" );
			break;
		case D3DSIO_LOGP:
			V_snprintf( buff, nBufLen, "LOG" );
			break;
		case D3DSIO_CND:
			Assert(0);
			break;
		case D3DSIO_DEF:
			V_snprintf( buff, nBufLen, "DEF" );
			break;
		case D3DSIO_TEXREG2RGB:
		case D3DSIO_TEXDP3TEX:
		case D3DSIO_TEXM3x2DEPTH:
		case D3DSIO_TEXDP3:
		case D3DSIO_TEXM3x3:
			Assert(0);
			break;
		case D3DSIO_TEXDEPTH:
			V_snprintf( buff, nBufLen, "texdepth" );
			break;
		case D3DSIO_CMP:
			Assert( !m_bVertexShader );
			V_snprintf( buff, nBufLen, "CMP" );
			break;
		case D3DSIO_BEM:
			Assert(0);
			break;
		case D3DSIO_DP2ADD:
			Assert(0);
			break;
		case D3DSIO_DSX:
		case D3DSIO_DSY:
			Assert(0);
			break;
		case D3DSIO_TEXLDD:
			V_snprintf( buff, nBufLen, "texldd" );
			break;
		case D3DSIO_SETP:
			Assert(0);
			break;
		case D3DSIO_TEXLDL:
			V_snprintf( buff, nBufLen, "texldl" );
			break;
		case D3DSIO_BREAKP:
		case D3DSIO_PHASE:
			Assert(0);
			break;
	}
}


//------------------------------------------------------------------------------
// Helper function which prints ASCII representation of usage-usageindex pair to string
//
// Strictly used by vertex shaders
// not used any more now that we have attribmap metadata
//------------------------------------------------------------------------------
void D3DToGL_ASM::PrintUsageAndIndexToString( uint32 dwToken, char* strUsageUsageIndexName, int nBufLen, bool bGLSL )
{
	uint32 dwUsage = ( dwToken & D3DSP_DCL_USAGE_MASK );
	uint32 dwUsageIndex = ( dwToken & D3DSP_DCL_USAGEINDEX_MASK ) >> D3DSP_DCL_USAGEINDEX_SHIFT;

	switch ( dwUsage )
	{
		case D3DDECLUSAGE_POSITION:
			V_snprintf( strUsageUsageIndexName, nBufLen, "vertex.attrib[0]" );	//"vertex.position" );			// aka generic [0]
			break;
		case D3DDECLUSAGE_BLENDWEIGHT:
			V_snprintf( strUsageUsageIndexName, nBufLen, "vertex.attrib[1]" );	// "vertex.attrib[12]" );			// or [1]
			break;
		case D3DDECLUSAGE_BLENDINDICES:
			V_snprintf( strUsageUsageIndexName, nBufLen, "vertex.attrib[13]" );	// "vertex.attrib[13]" );			// or [ 7 ]
			break;
		case D3DDECLUSAGE_NORMAL:
			V_snprintf( strUsageUsageIndexName, nBufLen, "vertex.attrib[2]" );	// "vertex.normal" );				// aka [ 2 ]
			break;
		case D3DDECLUSAGE_PSIZE:
			Assert(0);
			V_snprintf( strUsageUsageIndexName, nBufLen, "_psize" );					// no analog
			break;
		case D3DDECLUSAGE_TEXCOORD:
			V_snprintf( strUsageUsageIndexName, nBufLen, "vertex.attrib[%d]", 8+dwUsageIndex );	// "vertex.texcoord[%d]", dwUsageIndex );		// aka [8] - [15] ?
			break;
		case D3DDECLUSAGE_TANGENT:
			V_snprintf( strUsageUsageIndexName, nBufLen, "vertex.attrib[15]" );			// aka texc[7]
			break;
		case D3DDECLUSAGE_BINORMAL:
			V_snprintf( strUsageUsageIndexName, nBufLen, "vertex.attrib[14]" );			// aka texc[6]
			break;
//		case D3DDECLUSAGE_TESSFACTOR:
//			Assert(0);
//			V_snprintf( strUsageUsageIndexName, nBufLen, "_position" );					// no analog
//			break;
//		case D3DDECLUSAGE_POSITIONT:
//			Assert(0);
//			V_snprintf( strUsageUsageIndexName, nBufLen, "_positiont" );				// no analog
//			break;
		case D3DDECLUSAGE_COLOR:
			V_snprintf( strUsageUsageIndexName, nBufLen, "vertex.attrib[%d]", 3+dwUsageIndex ); //dwUsageIndex );		// != 0 ? "vertex.color.secondary" : "vertex.color" );	// aka [3] / [4] (second)
			break;
		case D3DDECLUSAGE_FOG:
			V_snprintf( strUsageUsageIndexName, nBufLen, "vertex.attrib[5]" );	//"vertex.position" /* "_fog" */ );	//FIXME, evil		// aka [5] / vertex.fogcoord
			break;
		case D3DDECLUSAGE_DEPTH:
			Assert(0);
			V_snprintf( strUsageUsageIndexName, nBufLen, "_depth" );					// no analog
			break;
		case D3DDECLUSAGE_SAMPLE:
			Assert(0);
			V_snprintf( strUsageUsageIndexName, nBufLen, "_sample" );					// no analog
			break;
		default:
			Debugger();
		break;
	}
}

uint32 D3DToGL_ASM::GetRegType( uint32 dwRegToken )
{
	return ( ( dwRegToken & D3DSP_REGTYPE_MASK2 ) >> D3DSP_REGTYPE_SHIFT2 ) | ( ( dwRegToken & D3DSP_REGTYPE_MASK ) >> D3DSP_REGTYPE_SHIFT );
}

void D3DToGL_ASM::PrintIndentation( char *pBuf, int nBufLen )
{
	for( int i=0; i<m_NumIndentTabs; i++ )
	{
		strcat_s( pBuf, nBufLen, "\t" );
	}
}

void D3DToGL_ASM::FlagIndirectRegister( uint32 dwToken, int *pARLDestReg )
{
	switch ( dwToken & D3DVS_SWIZZLE_MASK & D3DVS_X_W )
	{
		case D3DVS_X_X:
			*pARLDestReg = ARL_DEST_X;
			break;
		case D3DVS_X_Y:
			*pARLDestReg = ARL_DEST_Y;
			break;
		case D3DVS_X_Z:
			*pARLDestReg = ARL_DEST_Z;
			break;
		case D3DVS_X_W:
			*pARLDestReg = ARL_DEST_W;
			break;
	}
}


//------------------------------------------------------------------------------
// PrintParameterToString()
//
// Helper function which prints ASCII representation of passed Parameter dwToken
// to string. Token defines parameter details. The dwSourceOrDest parameter says 
// whether or not this is a source or destination register
//------------------------------------------------------------------------------
void D3DToGL_ASM::PrintParameterToString ( uint32 dwToken, uint32 dwSourceOrDest, char *pRegisterName, int nBufLen, bool bGLSL, int *pARLDestReg = NULL )
{
	char buff[32];
	bool bAllowWriteMask = true;

	uint32 dwRegNum = dwToken & D3DSP_REGNUM_MASK;

	uint32 dwRegType, dwSwizzle;
	uint32 dwSrcModifier = D3DSPSM_NONE;

	// Clear string to zero length
	V_snprintf( pRegisterName, nBufLen, "" );

	// Weird one...bits are split apart in the dwToken
	dwRegType = ( ( dwToken & D3DSP_REGTYPE_MASK2 ) >> D3DSP_REGTYPE_SHIFT2 ) | ( ( dwToken & D3DSP_REGTYPE_MASK ) >> D3DSP_REGTYPE_SHIFT );

	// If this is a dest register
	if ( dwSourceOrDest == DST_REGISTER )
	{
		// Instruction modifiers
		if ( dwToken & D3DSPDM_PARTIALPRECISION )
		{
//			strcat_s( pRegisterName, nBufLen, "_pp" );
		}

		if ( dwToken & D3DSPDM_SATURATE)
		{
			strcat_s( pRegisterName, nBufLen, "_SAT" );
		}

		if ( dwToken & D3DSPDM_MSAMPCENTROID)
		{
//			strcat_s( pRegisterName, nBufLen, "_centroid" );
		}

		if ( !bGLSL )
		{
			strcat_s( pRegisterName, nBufLen, " " );
		}
	}

	// If this is a source register
	if ( dwSourceOrDest == SRC_REGISTER )
	{
		dwSrcModifier = dwToken & D3DSP_SRCMOD_MASK;

		// If there are any source modifiers, check to see if they're at
		// least partially "prefix" and prepend appropriately
		if ( dwSrcModifier != D3DSPSM_NONE )
		{
			switch ( dwSrcModifier )
			{
				// These four start with just minus... (some may result in "postfix" notation as well later on)
				case D3DSPSM_NEG:							 // negate
					strcat_s( pRegisterName, nBufLen, "-" );
					break;
				case D3DSPSM_BIASNEG:						// bias and negate
				case D3DSPSM_SIGNNEG:						// sign and negate
				case D3DSPSM_X2NEG:						  // *2 and negate
					Assert(0);
					strcat_s( pRegisterName, nBufLen, "-" );
					break;
				case D3DSPSM_COMP:							// complement
					Assert(0);
					strcat_s( pRegisterName, nBufLen, "1-" );
					break;
				case D3DSPSM_ABS:							 // abs()
					Assert(0);
					strcat_s( pRegisterName, nBufLen, "abs(" );
					break;
				case D3DSPSM_ABSNEG:						 // -abs()
				Assert(0);
					strcat_s( pRegisterName, nBufLen, "-abs(" );
					break;
				case D3DSPSM_NOT:							 // for predicate register: "!p0"
				Assert(0);
					strcat_s( pRegisterName, nBufLen, "!" );
					break;
			}
		}
	}

	// Register name (from type and number)
	switch ( dwRegType )
	{
		case D3DSPR_TEMP:
			V_snprintf( buff, sizeof( buff ), "r%d", dwRegNum );
			strcat_s( pRegisterName, nBufLen, buff );
			m_dwTempUsageMask |= 0x00000001 << dwRegNum;			// Keep track of the use of this temp
			break;
		case D3DSPR_INPUT:
			if ( m_bVertexShader || ( dwSourceOrDest == SRC_REGISTER ) || bGLSL )
			{
				V_snprintf( buff, sizeof( buff ), "v%d", dwRegNum );
				strcat_s( pRegisterName, nBufLen, buff );
			}
			else // asm pixel shader declaration syntax:
			{
				V_snprintf( buff, sizeof( buff ), dwRegNum == 0 ? "v0 = fragment.color" : "v1 = fragment.color.secondary" );
				strcat_s( pRegisterName, nBufLen, buff );
				bAllowWriteMask = false;
			}
			break;
		case D3DSPR_CONST:
			if ( m_bConstantRegisterDefined[dwRegNum] )
			{
				// Put defined constants into their own namespace "d"
				V_snprintf( buff, sizeof( buff ), "d%d", dwRegNum );
				strcat_s( pRegisterName, nBufLen, buff );
			}
			else if ( dwToken & D3DSHADER_ADDRESSMODE_MASK )
			{
				// Index into single c[] register array with relative addressing
				FlagIndirectRegister( GetNextToken(), pARLDestReg );

				V_snprintf( buff, sizeof( buff ), "c[a0.x + %d]", dwRegNum );
				strcat_s( pRegisterName, nBufLen, buff );
				m_bConstantRegisterReferenced[dwRegNum] = true;
			}
			else
			{
				 // Index into single c[] register array with absolute addressing
				 V_snprintf( buff, sizeof( buff ), "c[%d]", dwRegNum );
				 strcat_s( pRegisterName, nBufLen, buff );
				 m_bConstantRegisterReferenced[dwRegNum] = true;
			}
			break;
		case D3DSPR_ADDR: // aliases to D3DSPR_TEXTURE
			if ( m_bVertexShader )
			{
				V_snprintf( buff, sizeof( buff ), "VEC_ADDRESS_REG" );	// Move into our temp, rather than a0
			}
			else
			{
				if ( dwSourceOrDest == DST_REGISTER )
				{
					V_snprintf( buff, sizeof( buff ), "t%d = fragment.texcoord[%d]", dwRegNum, dwRegNum );
					bAllowWriteMask = false;
				}
				else
				{
					V_snprintf( buff, sizeof( buff ), "t%d", dwRegNum );
				}
			}
			strcat_s( pRegisterName, nBufLen, buff );
			break;
		case D3DSPR_RASTOUT: // vertex shader oPos
			//V_snprintf( buff, sizeof( buff ), "oPos", dwRegNum );	// that stray parameter should have been a clue :)
			switch( dwRegNum )
			{
				case D3DSRO_POSITION:
					strcat_s( pRegisterName, nBufLen, "oPos" );
					m_bDeclareVSOPos = true;
				break;
				
				case D3DSRO_FOG:
					strcat_s( pRegisterName, nBufLen, "oFog" );
					m_bDeclareVSOFog = true;
				break;

				default:
					printf("\nD3DSPR_RASTOUT: dwRegNum is %08x and token is %08x",dwRegNum,dwToken);  
					Assert(0);
				break;
			}
			break;
		case D3DSPR_ATTROUT:
			Assert( m_bVertexShader );
			V_snprintf( buff, sizeof( buff ), "oD%d", dwRegNum );
			strcat_s( pRegisterName, nBufLen, buff );
			m_bOutputColorRegister[dwRegNum] = true;
			break;
		case D3DSPR_TEXCRDOUT: // aliases to D3DSPR_OUTPUT
			if ( m_bVertexShader )
			{
				V_snprintf( buff, sizeof( buff ), "oT%d", dwRegNum );
				m_dwTexCoordOutMask |= ( 0x00000001 << dwRegNum );
			}
			else
			{
				V_snprintf( buff, sizeof( buff ), "oC%d", dwRegNum );
			}
			strcat_s( pRegisterName, nBufLen, buff );
			break;
		case D3DSPR_CONSTINT:
			Assert(0);
			V_snprintf( buff, sizeof( buff ), "xxx%d", dwRegNum );
			strcat_s( pRegisterName, nBufLen, buff );
			break;
		case D3DSPR_COLOROUT:
			if ( bGLSL )
			{
				Assert( dwRegNum == 0 );
				V_snprintf( buff, sizeof( buff ), "gl_FragmentColor" );
			}
			else
			{
				V_snprintf( buff, sizeof( buff ), "oC%d", dwRegNum );
			}
			strcat_s( pRegisterName, nBufLen, buff );
			m_bOutputColorRegister[dwRegNum] = true;
			break;
		case D3DSPR_DEPTHOUT:
			V_snprintf( buff, sizeof( buff ), "oDepth" );
			strcat_s( pRegisterName, nBufLen, buff );
			m_bOutputDepthRegister = true;
			break;
		case D3DSPR_SAMPLER:
			V_snprintf( buff, sizeof( buff ), "texture[%d]", dwRegNum );
			strcat_s( pRegisterName, nBufLen, buff );
			break;
		case D3DSPR_CONST2:
			Assert(0);
			V_snprintf( buff, sizeof( buff ), "c%d", dwRegNum+2048);
			strcat_s( pRegisterName, nBufLen, buff );
			break;
		case D3DSPR_CONST3:
			Assert(0);
			V_snprintf( buff, sizeof( buff ), "c%d", dwRegNum+4096);
			strcat_s( pRegisterName, nBufLen, buff );
			break;
		case D3DSPR_CONST4:
			Assert(0);
			V_snprintf( buff, sizeof( buff ), "c%d", dwRegNum+6144);
			strcat_s( pRegisterName, nBufLen, buff );
			break;
		case D3DSPR_CONSTBOOL:
			Assert(0);
			V_snprintf( buff, sizeof( buff ), "b%d", dwRegNum );
			strcat_s( pRegisterName, nBufLen, buff );
			break;
		case D3DSPR_LOOP:
			Assert(0);
			V_snprintf( buff, sizeof( buff ), "aL%d", dwRegNum );
			strcat_s( pRegisterName, nBufLen, buff );
			break;
		case D3DSPR_TEMPFLOAT16:
			Assert(0);
			V_snprintf( buff, sizeof( buff ), "temp_float16_xxx%d", dwRegNum );
			strcat_s( pRegisterName, nBufLen, buff );
			break;
		case D3DSPR_MISCTYPE:
			Assert(0);
			V_snprintf( buff, sizeof( buff ), "misc%d", dwRegNum );
			strcat_s( pRegisterName, nBufLen, buff );
			break;
		case D3DSPR_LABEL:
			Assert(0);
			V_snprintf( buff, sizeof( buff ), "label%d", dwRegNum );
			strcat_s( pRegisterName, nBufLen, buff );
			break;
		case D3DSPR_PREDICATE:
			Assert(0);
			V_snprintf( buff, sizeof( buff ), "p%d", dwRegNum );
			strcat_s( pRegisterName, nBufLen, buff );
			break;
	}

	// If this is a dest register
	if ( dwSourceOrDest == DST_REGISTER )
	{
		//
		// Write masks
		//
		// If some (not all, not none) of the write masks are set, we should include them
		//
		if ( bAllowWriteMask && ( !((dwToken & D3DSP_WRITEMASK_ALL) == D3DSP_WRITEMASK_ALL) || ((dwToken & D3DSP_WRITEMASK_ALL) == 0x00000000) ) )
		{
			// Put the dot on there
			strcat_s( pRegisterName, nBufLen, "." );

			// Optionally put on the x, y, z or w
			if ( dwToken & D3DSP_WRITEMASK_0 )
			{
				strcat_s( pRegisterName, nBufLen, "x" );
			}
			if ( dwToken & D3DSP_WRITEMASK_1 )
			{
				strcat_s( pRegisterName, nBufLen, "y" );
			}
			if ( dwToken & D3DSP_WRITEMASK_2 )
			{
				strcat_s( pRegisterName, nBufLen, "z" );
			}
			if ( dwToken & D3DSP_WRITEMASK_3 )
			{
				strcat_s( pRegisterName, nBufLen, "w" );
			}
		}
	}
	else // must be a source register
	{
		uint32 dwXSwizzle, dwYSwizzle, dwZSwizzle, dwWSwizzle;

		// Mask out the swizzle modifier
		dwSwizzle = dwToken & D3DVS_SWIZZLE_MASK;

		// If there are any swizzles at all, tack on the appropriate notation
		if ( dwSwizzle != D3DVS_NOSWIZZLE )
		{
			// Separate out the two-bit codes for each component swizzle
			dwXSwizzle = dwSwizzle & D3DVS_X_W;
			dwYSwizzle = dwSwizzle & D3DVS_Y_W;
			dwZSwizzle = dwSwizzle & D3DVS_Z_W;
			dwWSwizzle = dwSwizzle & D3DVS_W_W;

			// Put on the dot
			strcat_s( pRegisterName, nBufLen, "." );

			// See where X comes from
			switch ( dwXSwizzle )
			{
				case D3DVS_X_X:
					strcat_s( pRegisterName, nBufLen, "x" );
					break;
				case D3DVS_X_Y:
					strcat_s( pRegisterName, nBufLen, "y" );
					break;
				case D3DVS_X_Z:
					strcat_s( pRegisterName, nBufLen, "z" );
					break;
				case D3DVS_X_W:
					strcat_s( pRegisterName, nBufLen, "w" );
					break;
			}


			// If the source of the remaining components are aren't
			// identical to the source of x, continue with swizzle
			if ( ((dwXSwizzle >> D3DVS_SWIZZLE_SHIFT) != (dwYSwizzle >> (D3DVS_SWIZZLE_SHIFT + 2))) ||	// X and Y sources match?
			   ((dwXSwizzle >> D3DVS_SWIZZLE_SHIFT) != (dwZSwizzle >> (D3DVS_SWIZZLE_SHIFT + 4))) ||	// X and Z sources match?
			   ((dwXSwizzle >> D3DVS_SWIZZLE_SHIFT) != (dwWSwizzle >> (D3DVS_SWIZZLE_SHIFT + 6))))	  // X and W sources match?
			{

				// OpenGL seems to want us to have either 1 or 4 components in a swizzle, so just plow on through the rest
				switch ( dwYSwizzle )
				{
					case D3DVS_Y_X:
						strcat_s( pRegisterName, nBufLen, "x" );
						break;
					case D3DVS_Y_Y:
						strcat_s( pRegisterName, nBufLen, "y" );
						break;
					case D3DVS_Y_Z:
						strcat_s( pRegisterName, nBufLen, "z" );
						break;
					case D3DVS_Y_W:
						strcat_s( pRegisterName, nBufLen, "w" );
						break;
				}

				switch ( dwZSwizzle )
				{
					case D3DVS_Z_X:
						strcat_s( pRegisterName, nBufLen, "x" );
						break;
					case D3DVS_Z_Y:
						strcat_s( pRegisterName, nBufLen, "y" );
						break;
					case D3DVS_Z_Z:
						strcat_s( pRegisterName, nBufLen, "z" );
						break;
					case D3DVS_Z_W:
						strcat_s( pRegisterName, nBufLen, "w" );
						break;
				}

				switch ( dwWSwizzle )
				{
					case D3DVS_W_X:
						strcat_s( pRegisterName, nBufLen, "x" );
						break;
					case D3DVS_W_Y:
						strcat_s( pRegisterName, nBufLen, "y" );
						break;
					case D3DVS_W_Z:
						strcat_s( pRegisterName, nBufLen, "z" );
						break;
					case D3DVS_W_W:
						strcat_s( pRegisterName, nBufLen, "w" );
						break;
				}

			}

		}

		// If there are any source modifiers, check to see if they're at
		// least partially "postfix" and tack them on as appropriate
		if ( dwSrcModifier != D3DSPSM_NONE )
		{
			switch ( dwSrcModifier )
			{
				case D3DSPSM_BIAS:							// bias
				case D3DSPSM_BIASNEG:						// bias and negate
				Assert(0);
					strcat_s( pRegisterName, nBufLen, "_bx2" );
					break;
				case D3DSPSM_SIGN:							// sign
				case D3DSPSM_SIGNNEG:						// sign and negate
				Assert(0);
					strcat_s( pRegisterName, nBufLen, "_sgn" );
					break;
				case D3DSPSM_X2:							  // *2
				case D3DSPSM_X2NEG:						  // *2 and negate
				Assert(0);
					strcat_s( pRegisterName, nBufLen, "_x2" );
					break;
				case D3DSPSM_ABS:							 // abs()
				case D3DSPSM_ABSNEG:						 // -abs()
				Assert(0);
					strcat_s( pRegisterName, nBufLen, ")" );
					break;
				case D3DSPSM_DZ:							  // divide through by z component
				Assert(0);
					strcat_s( pRegisterName, nBufLen, "_dz" );
					break;
				case D3DSPSM_DW:							  // divide through by w component
				Assert(0);
					strcat_s( pRegisterName, nBufLen, "_dw" );
					break;
			}
		} // end postfix modifiers (really only ps.1.x)
	}
}

// These are the only ARL instructions that should appear in the instruction stream
void D3DToGL_ASM::InsertMoveInstruction( char *pCode, int nCodeSize, int nARLComponent )
{
	switch ( nARLComponent )
	{
		case ARL_DEST_X:
			strcat_s( pCode, nCodeSize, "ARL a0.x, VEC_ADDRESS_REG.x;\n" );
			break;
		case ARL_DEST_Y:
			strcat_s( pCode, nCodeSize, "ARL a0.x, VEC_ADDRESS_REG.y;\n" );
			break;
		case ARL_DEST_Z:
			strcat_s( pCode, nCodeSize, "ARL a0.x, VEC_ADDRESS_REG.z;\n" );
			break;
		case ARL_DEST_W:
			strcat_s( pCode, nCodeSize, "ARL a0.x, VEC_ADDRESS_REG.w;\n" );
			break;
	}
}

// This optionally inserts a move from our dummy address register to the .x component of the real one
void D3DToGL_ASM::InsertMoveFromAddressRegister( char *pCode, int nCodeSize, int nARLComp0, int nARLComp1, int nARLComp2 = ARL_DEST_NONE )
{
	int nNumSwizzles = 0;

	if ( nARLComp0 != ARL_DEST_NONE )
		nNumSwizzles++;

	if ( nARLComp1 != ARL_DEST_NONE )
		nNumSwizzles++;

	if ( nARLComp2 != ARL_DEST_NONE )
		nNumSwizzles++;

	// We shouldn't have any more than one indirect address usage in a single instruction
	Assert( nNumSwizzles < 2 );

	if ( nARLComp0 != ARL_DEST_NONE )
	{
		InsertMoveInstruction( pCode, nCodeSize, nARLComp0 );
	}
	else if  ( nARLComp1 != ARL_DEST_NONE )
	{
		InsertMoveInstruction( pCode, nCodeSize, nARLComp1 );
	}
	else if  ( nARLComp2 != ARL_DEST_NONE )
	{
		InsertMoveInstruction( pCode, nCodeSize, nARLComp2 );
	}
}




//------------------------------------------------------------------------------
// TranslateShader()
//
// This is the main function that the outside world sees.  A pointer to the
// uint32 stream returned from the D3DX compile routine is parsed and used
// to write human-readable asm code into the character array pointed to by
// pDisassembledCode.  An error code is returned.
//------------------------------------------------------------------------------

static int g_translationCounter = 0;

int D3DToGL_ASM::TranslateShader( uint32* code, char *pDisassembledCode, int nBufLen, bool *bVertexShader, uint32 options, int32 nShadowDepthSampler, char *debugLabel )
{
	uint32 i, dwToken, dwRegToken, inst, nNumTokensToSkip;
	char buff[256];
	char pDestReg[32];
	char pSrc0Reg[32];
	char pSrc1Reg[32];
	char pSrc2Reg[32];

	int nARLComp0, nARLComp1, nARLComp2;

	// obey options
	m_bUseEnvParams = (options & D3DToGL_OptionUseEnvParams) != 0;
	m_bDoFixupZ = (options & D3DToGL_OptionDoFixupZ) != 0;
	m_bDoFixupY = (options & D3DToGL_OptionDoFixupY) != 0;
	m_bDoUserClipPlanes = (options & D3DToGL_OptionDoUserClipPlanes) != 0;

	// debugging
	m_bSpew = (options & D3DToGL_OptionSpew) != 0;
	
	// m_bSpew |= (g_translationCounter == 1012 );	// interested in this specific translation run
	
	// These are not accessed below in a way that will cause them to grow, so
	// we could overflow these and/or the buffer pointed to by pDisassembledCode
	CUtlBuffer bufAttribCode( 100, 10000, CUtlBuffer::TEXT_BUFFER );
	CUtlBuffer bufParamCode( 100, 10000, CUtlBuffer::TEXT_BUFFER );
	CUtlBuffer bufALUCode( 100, 30000, CUtlBuffer::TEXT_BUFFER );

	// Pointers to text buffers for assembling sections of the program
	char *pHeaderCode = pDisassembledCode;
	char *pAttribMapStart = NULL;
	char *pAttribCode = (char *)bufAttribCode.Base();
	char *pParamCode = (char *)bufParamCode.Base();
	char *pALUCode = (char *)bufALUCode.Base();

	V_snprintf( pHeaderCode, nBufLen, "" );
	V_snprintf( pAttribCode, bufAttribCode.Size(), "" );
	V_snprintf( pParamCode, bufParamCode.Size(), "" );
	V_snprintf( pALUCode, bufALUCode.Size(), "" );

	for ( i=0; i<MAX_SHADER_CONSTANTS; i++ )
	{
		m_bConstantRegisterReferenced[i] = false;
		m_bConstantRegisterDefined[i] = false;
	}

	// Track shadow sampler usage for proper declaration
	m_nShadowDepthSampler = nShadowDepthSampler;
	m_bDeclareShadowOption = false;

	// Various flags set while parsing code to drive various declaration instructions
	m_bNeedsD2AddTemp = false;
	m_bNeedsLerpTemp = false;
	m_bNeedsNRMTemp = false;
	m_bNeedsSinCosDeclarations = false;
	m_bDeclareAddressReg = false;
	m_bDeclareVSOPos = false;
	m_bDeclareVSOFog = false;
	m_dwTexCoordOutMask = 0;
	m_bOutputColorRegister[0] = false;
	m_bOutputColorRegister[1] = false;
	m_bOutputColorRegister[2] = false;
	m_bOutputColorRegister[3] = false;
	m_bOutputDepthRegister = false;
	m_dwTempUsageMask = 0x00000000;

	memset( m_dwAttribMap, 0xFF, sizeof(m_dwAttribMap) );
	
	m_pdwBaseToken = m_pdwNextToken = code;	 // Initialize dwToken pointers

	dwToken = GetNextToken();
	m_dwMajorVersion = D3DSHADER_VERSION_MAJOR( dwToken );
	m_dwMinorVersion = D3DSHADER_VERSION_MINOR( dwToken );

	// We only do vs_2_0 and ps_2_x
	if ( m_dwMajorVersion != 2 )
	{
		Debugger();
	}

	// If pixel shader
	if ( ( dwToken & 0xFFFF0000 ) == 0xFFFF0000 )
	{
		V_snprintf( pHeaderCode, nBufLen, "!!ARBfp1.0\n" );
		m_bVertexShader = false;
	}
	else // vertex shader
	{
		if (m_bDoUserClipPlanes)
		{
			// include "OPTION NV_vertex_program2;"
			V_snprintf( pHeaderCode, nBufLen, "!!ARBvp1.0\n#//ATTRIBMAP-xx-xx-xx-xx-xx-xx-xx-xx-xx-xx-xx-xx-xx-xx-xx-xx\nOPTION NV_vertex_program2;\n" );
		}
		else
		{
			// do not include "OPTION NV_vertex_program2;"
			V_snprintf( pHeaderCode, nBufLen, "!!ARBvp1.0\n#//ATTRIBMAP-xx-xx-xx-xx-xx-xx-xx-xx-xx-xx-xx-xx-xx-xx-xx-xx\n" );
		}

			// find that first 'x' which is where the attrib map will be written later.
		pAttribMapStart = strchr( pHeaderCode, 'x');
		
		m_bVertexShader = true;
	}

	*bVertexShader = m_bVertexShader;

	if ( m_bSpew )
	{
		printf("\n************* translating shader " );
	}
	
	int opcounter = 0;
	
	// Loop until we hit the end dwToken...note that D3DPS_END() == D3DVS_END() so this works for either
	while ( dwToken != D3DPS_END() )
	{
#ifdef POSIX
		int tokenIndex = m_pdwNextToken - code;
#endif
		int aluCodeLength0 = strlen( pALUCode );
		
		dwToken = GetNextToken();	// Get next dwToken in the stream
		inst = Opcode( dwToken ); // Mask out the instruction opcode

		if ( m_bSpew )
		{
#ifdef POSIX
			printf("\n** token# %04x inst# %04d  opcode %s (%08x)", tokenIndex, opcounter, GLMDecode(eD3D_SIO, inst), dwToken );
#endif
			opcounter++;
		}
		
		switch ( inst )
		{
			// -- No arguments at all -----------------------------------------------
			case D3DSIO_NOP:
			case D3DSIO_PHASE:
			case D3DSIO_RET:
			case D3DSIO_ELSE:
			case D3DSIO_ENDIF:
			case D3DSIO_ENDLOOP:
			case D3DSIO_ENDREP:
			case D3DSIO_BREAK:
				Assert(0);
				PrintOpcode( inst, buff, sizeof( buff ) );
				strcat_s( pALUCode, bufALUCode.Size(), buff );
				strcat_s( pALUCode, bufALUCode.Size(), ";\n" );
				break;

			// -- "Declarative" non dcl ops ----------------------------------------
			case D3DSIO_TEXDEPTH:
			case D3DSIO_TEXKILL:

				PrintOpcode( inst, buff, sizeof( buff ) );
				strcat_s( pALUCode, bufALUCode.Size(), buff );

				dwToken = GetNextToken();
				PrintParameterToString( dwToken, DST_REGISTER, buff, sizeof( buff ), false );
				strcat_s( pALUCode, bufALUCode.Size(), buff );
				strcat_s( pALUCode, bufALUCode.Size(), ";\n" );
				break;

			// -- Unary ops -------------------------------------------------
			case D3DSIO_BEM:
			case D3DSIO_TEXBEM:
			case D3DSIO_TEXBEML:
			case D3DSIO_TEXDP3:
			case D3DSIO_TEXDP3TEX:
			case D3DSIO_TEXM3x2DEPTH:
			case D3DSIO_TEXM3x2TEX:
			case D3DSIO_TEXM3x3:
			case D3DSIO_TEXM3x3PAD:
			case D3DSIO_TEXM3x3TEX:
			case D3DSIO_TEXM3x3VSPEC:
			case D3DSIO_TEXREG2AR:
			case D3DSIO_TEXREG2GB:
			case D3DSIO_TEXREG2RGB:
			case D3DSIO_LABEL:
			case D3DSIO_CALL:
			case D3DSIO_IF:
			case D3DSIO_LOOP:
			case D3DSIO_REP:
			case D3DSIO_BREAKP:
			case D3DSIO_DSX:
			case D3DSIO_DSY:
				Assert(0);
				break;

			case D3DSIO_NRM:

				m_bNeedsNRMTemp = true;

				dwToken = GetNextToken();
				PrintParameterToString( dwToken, DST_REGISTER, pDestReg, sizeof( pDestReg ), false );
				dwToken = GetNextToken();
				PrintParameterToString( dwToken, SRC_REGISTER, pSrc0Reg, sizeof( pSrc0Reg ), false );

				strcat_s( pALUCode, bufALUCode.Size(), "DP3 NRM_TEMP.w, " );
				strcat_s( pALUCode, bufALUCode.Size(), pSrc0Reg );
				strcat_s( pALUCode, bufALUCode.Size(), ", " );
				strcat_s( pALUCode, bufALUCode.Size(), pSrc0Reg );
				strcat_s( pALUCode, bufALUCode.Size(), ";\nRSQ NRM_TEMP.w, NRM_TEMP.w;\nMUL" );
				strcat_s( pALUCode, bufALUCode.Size(), pDestReg );
				strcat_s( pALUCode, bufALUCode.Size(), ", NRM_TEMP.w, " );
				strcat_s( pALUCode, bufALUCode.Size(), pSrc0Reg );
				strcat_s( pALUCode, bufALUCode.Size(), ";\n" );
				break;

			case D3DSIO_MOVA:

				m_bDeclareAddressReg = true;
	
				PrintOpcode( inst, buff, sizeof( buff ) );
				strcat_s( pALUCode, bufALUCode.Size(), buff );

				dwToken = GetNextToken();
				PrintParameterToString( dwToken, DST_REGISTER, buff, sizeof( buff ), false );
				strcat_s( pALUCode, bufALUCode.Size(), buff );
				strcat_s( pALUCode, bufALUCode.Size(), ", " );

				dwToken = GetNextToken();
				PrintParameterToString( dwToken, SRC_REGISTER, buff, sizeof( buff ), false );
				strcat_s( pALUCode, bufALUCode.Size(), buff );
				strcat_s( pALUCode, bufALUCode.Size(), ";\n" );

				break;
			case D3DSIO_MOV:
			case D3DSIO_RCP:
			case D3DSIO_RSQ:
			case D3DSIO_EXP:
			case D3DSIO_EXPP:
			case D3DSIO_LOG:
			case D3DSIO_LOGP:
			case D3DSIO_FRC:
			case D3DSIO_LIT:
			case D3DSIO_ABS:

				PrintOpcode( inst, buff, sizeof( buff ) );
				strcat_s( pALUCode, bufALUCode.Size(), buff );

				dwToken = GetNextToken();
				PrintParameterToString( dwToken, DST_REGISTER, buff, sizeof( buff ), false );
				strcat_s( pALUCode, bufALUCode.Size(), buff );
				strcat_s( pALUCode, bufALUCode.Size(), ", " );

				dwToken = GetNextToken();
				PrintParameterToString( dwToken, SRC_REGISTER, buff, sizeof( buff ), false );
				strcat_s( pALUCode, bufALUCode.Size(), buff );
				strcat_s( pALUCode, bufALUCode.Size(), ";\n" );
				break;

			// -- Binary ops -------------------------------------------------
			case D3DSIO_TEXM3x3SPEC:
			case D3DSIO_M4x4:
			case D3DSIO_M4x3:
			case D3DSIO_M3x4:
			case D3DSIO_M3x3:
			case D3DSIO_M3x2:
			case D3DSIO_CALLNZ:
			case D3DSIO_IFC:
			case D3DSIO_BREAKC:
			case D3DSIO_SETP:
			case D3DSIO_TEXLDL:
				Assert(0);
				break;

			case D3DSIO_ADD:
			case D3DSIO_SUB:
			case D3DSIO_MUL:
			case D3DSIO_DP3:
			case D3DSIO_DP4:
			case D3DSIO_MIN:
			case D3DSIO_MAX:
			case D3DSIO_DST:
			case D3DSIO_SLT:
			case D3DSIO_SGE:
			case D3DSIO_CRS:
			case D3DSIO_POW:

				// Print opcode and registers into separate buffers
				PrintOpcode( inst, buff, sizeof( buff ) );
				PrintParameterToString( GetNextToken(), DST_REGISTER, pDestReg, sizeof( pDestReg ), false );
				nARLComp0 = ARL_DEST_NONE;
				PrintParameterToString( GetNextToken(), SRC_REGISTER, pSrc0Reg, sizeof( pSrc0Reg ), false, &nARLComp0 );
				nARLComp1 = ARL_DEST_NONE;
  				PrintParameterToString( GetNextToken(), SRC_REGISTER, pSrc1Reg, sizeof( pSrc1Reg ), false, &nARLComp1 );

				// This optionally inserts a move from our dummy address register to the .x component of the real one
				InsertMoveFromAddressRegister( pALUCode, bufALUCode.Size(), nARLComp0, nARLComp1 );

				// Concat this instruction into instruction stream
				strcat_s( pALUCode, bufALUCode.Size(), buff );
				strcat_s( pALUCode, bufALUCode.Size(), pDestReg );
				strcat_s( pALUCode, bufALUCode.Size(), ", " );
				strcat_s( pALUCode, bufALUCode.Size(), pSrc0Reg );
				strcat_s( pALUCode, bufALUCode.Size(), ", " );
				strcat_s( pALUCode, bufALUCode.Size(), pSrc1Reg );
				strcat_s( pALUCode, bufALUCode.Size(), ";\n" );

				break;

			// -- Ternary ops -------------------------------------------------
			case D3DSIO_DP2ADD:
				m_bNeedsD2AddTemp = true;

				PrintParameterToString( GetNextToken(), DST_REGISTER, pDestReg, sizeof( pDestReg ), false );
				PrintParameterToString( GetNextToken(), SRC_REGISTER, pSrc0Reg, sizeof( pSrc0Reg ), false );
				PrintParameterToString( GetNextToken(), SRC_REGISTER, pSrc1Reg, sizeof( pSrc1Reg ), false );
				PrintParameterToString( GetNextToken(), SRC_REGISTER, pSrc2Reg, sizeof( pSrc2Reg ), false );

				strcat_s( pALUCode, bufALUCode.Size(), "MOV DP2A0, " );			// MOV DP2A0, src0;
				strcat_s( pALUCode, bufALUCode.Size(), pSrc0Reg );
				strcat_s( pALUCode, bufALUCode.Size(), ";\nMOV DP2A0.z, 1;\n" );// MOV DP2A0.z, 1;

				strcat_s( pALUCode, bufALUCode.Size(), "MOV DP2A1, " );			// MOV DP2A1, src1;
				strcat_s( pALUCode, bufALUCode.Size(), pSrc1Reg );
				strcat_s( pALUCode, bufALUCode.Size(), ";\nMOV DP2A1.z, " );	// MOV DP2A1.z, src2;

				strcat_s( pALUCode, bufALUCode.Size(), pSrc2Reg );				// DP3 dest, DP2A0, DP2A1;
				strcat_s( pALUCode, bufALUCode.Size(), ";\nDP3" );

				strcat_s( pALUCode, bufALUCode.Size(), pDestReg );
				strcat_s( pALUCode, bufALUCode.Size(), ", DP2A0, DP2A1;\n" );
				break;

			case D3DSIO_LRP:

				if ( !m_bVertexShader )
				{
					PrintOpcode( inst, buff, sizeof( buff ) );
					strcat_s( pALUCode, bufALUCode.Size(), buff );
					PrintParameterToString( GetNextToken(), DST_REGISTER, buff, sizeof( buff ), false );
					strcat_s( pALUCode, bufALUCode.Size(), buff );
					strcat_s( pALUCode, bufALUCode.Size(), ", " );
					PrintParameterToString( GetNextToken(), SRC_REGISTER, buff, sizeof( buff ), false );
					strcat_s( pALUCode, bufALUCode.Size(), buff );
					strcat_s( pALUCode, bufALUCode.Size(), ", " );
					PrintParameterToString( GetNextToken(), SRC_REGISTER, buff, sizeof( buff ), false );
					strcat_s( pALUCode, bufALUCode.Size(), buff );
					strcat_s( pALUCode, bufALUCode.Size(), ", " );
					PrintParameterToString( GetNextToken(), SRC_REGISTER, buff, sizeof( buff ), false );
					strcat_s( pALUCode, bufALUCode.Size(), buff );
					strcat_s( pALUCode, bufALUCode.Size(), ";\n" );
				}
				else // VS doesn't actually have a LRP instruction.  Emulate with a SUB and a MAD
				{
					m_bNeedsLerpTemp = true;

					// dest = src0 * (src1 - src2) + src2;
					PrintParameterToString( GetNextToken(), DST_REGISTER, pDestReg, sizeof( pDestReg ), false );
					PrintParameterToString( GetNextToken(), SRC_REGISTER, pSrc0Reg, sizeof( pSrc0Reg ), false );
					PrintParameterToString( GetNextToken(), SRC_REGISTER, pSrc1Reg, sizeof( pSrc1Reg ), false );
					PrintParameterToString( GetNextToken(), SRC_REGISTER, pSrc2Reg, sizeof( pSrc2Reg ), false );

					strcat_s( pALUCode, bufALUCode.Size(), "SUB LRP_TEMP, " );	// SUB LRP_TEMP, src1, src2;
					strcat_s( pALUCode, bufALUCode.Size(), pSrc1Reg );
					strcat_s( pALUCode, bufALUCode.Size(), ", " );
					strcat_s( pALUCode, bufALUCode.Size(), pSrc2Reg );
					strcat_s( pALUCode, bufALUCode.Size(), ";\n" );

					strcat_s( pALUCode, bufALUCode.Size(), "MAD" );				// MAD dst, src0, LRP_TEMP, src2;
					strcat_s( pALUCode, bufALUCode.Size(), pDestReg );
					strcat_s( pALUCode, bufALUCode.Size(), ", " );
					strcat_s( pALUCode, bufALUCode.Size(), pSrc0Reg );
					strcat_s( pALUCode, bufALUCode.Size(), ", LRP_TEMP, " );
					strcat_s( pALUCode, bufALUCode.Size(), pSrc2Reg );
					strcat_s( pALUCode, bufALUCode.Size(), ";\n" );
				}
				break;

			case D3DSIO_SGN:
				Assert( m_bVertexShader );
				Assert(0);		// TODO emulate with SLT etc
				break;
			case D3DSIO_CND:
				Assert(0);
				break;
			case D3DSIO_CMP:
				Assert( !m_bVertexShader );

				// In Direct3D, result = (src0 >= 0.0) ? src1 : src2
				// In OpenGL,	result = (src0 <  0.0) ? src1 : src2
				//
				// As a result, arguments are effectively in a different order than Direct3D!  !#$&*!%#$&
				PrintParameterToString( GetNextToken(), DST_REGISTER, pDestReg, sizeof( pDestReg ), false );
				PrintParameterToString( GetNextToken(), SRC_REGISTER, pSrc0Reg, sizeof( pSrc0Reg ), false );
				PrintParameterToString( GetNextToken(), SRC_REGISTER, pSrc1Reg, sizeof( pSrc1Reg ), false );
				PrintParameterToString( GetNextToken(), SRC_REGISTER, pSrc2Reg, sizeof( pSrc2Reg ), false );

				strcat_s( pALUCode, bufALUCode.Size(), "CMP" );
				strcat_s( pALUCode, bufALUCode.Size(), pDestReg );
				strcat_s( pALUCode, bufALUCode.Size(), ", " );
				strcat_s( pALUCode, bufALUCode.Size(), pSrc0Reg );
				strcat_s( pALUCode, bufALUCode.Size(), ", " );
				strcat_s( pALUCode, bufALUCode.Size(), pSrc2Reg );		// Src 2	|
				strcat_s( pALUCode, bufALUCode.Size(), ", " );			//			|--- Swap these guys from Direct3D's convention
				strcat_s( pALUCode, bufALUCode.Size(), pSrc1Reg );		// Src 1	|
				strcat_s( pALUCode, bufALUCode.Size(), ";\n" );
				break;

			case D3DSIO_SINCOS:

				// This is the code sequence recommended to IHVs by Microsoft in the DirectX 9 DDK:
				//
				// http://msdn.microsoft.com/en-us/library/ms800337.aspx
				//
				//				 MUL SC_TEMP.z, src, src;
				//				 MAD SC_TEMP.xy, SC_TEMP.z, scA, scA.wzyx;
				//				 MAD SC_TEMP.xy, SC_TEMP,   SC_TEMP.z, scB;
				//				 MAD SC_TEMP.xy, SC_TEMP,   SC_TEMP.z, scB.wzyx;
				//				 MUL SC_TEMP.x,  SC_TEMP.x, src;
				//				 MUL SC_TEMP.xy, SC_TEMP,   SC_TEMP.x;
				//				 ADD SC_TEMP.xy, SC_TEMP,   SC_TEMP;
				//				 ADD SC_TEMP.x, -SC_TEMP.x, scB.z;

				m_bNeedsSinCosDeclarations = true;

				PrintParameterToString( GetNextToken(), DST_REGISTER, pDestReg, sizeof( pDestReg ), false );
				PrintParameterToString( GetNextToken(), SRC_REGISTER, pSrc0Reg, sizeof( pSrc0Reg ), false );

				// Eat two more tokens since D3D defines similar Taylor series constants that we won't need
				SkipTokens( 2 );

				strcat_s( pALUCode, bufALUCode.Size(), "MUL SC_TEMP.z, " );		// MUL SC_TEMP.z, src, src;
				strcat_s( pALUCode, bufALUCode.Size(), pSrc0Reg );
				strcat_s( pALUCode, bufALUCode.Size(), ", " );
				strcat_s( pALUCode, bufALUCode.Size(), pSrc0Reg );
				strcat_s( pALUCode, bufALUCode.Size(), ";\n" );

				strcat_s( pALUCode, bufALUCode.Size(), "MAD SC_TEMP.xy, SC_TEMP.z, scA, scA.wzyx;\n" );
				strcat_s( pALUCode, bufALUCode.Size(), "MAD SC_TEMP.xy, SC_TEMP,   SC_TEMP.z, scB;\n" );
				strcat_s( pALUCode, bufALUCode.Size(), "MAD SC_TEMP.xy, SC_TEMP,   SC_TEMP.z, scB.wzyx;\n" );

				strcat_s( pALUCode, bufALUCode.Size(), "MUL SC_TEMP.x,  SC_TEMP.x, " );
				strcat_s( pALUCode, bufALUCode.Size(), pSrc0Reg );
				strcat_s( pALUCode, bufALUCode.Size(), ";\n" );
				strcat_s( pALUCode, bufALUCode.Size(), "MUL SC_TEMP.xy, SC_TEMP,   SC_TEMP.x;\n" );
				strcat_s( pALUCode, bufALUCode.Size(), "ADD SC_TEMP.xy, SC_TEMP,   SC_TEMP;\n" );
				strcat_s( pALUCode, bufALUCode.Size(), "ADD SC_TEMP.x, -SC_TEMP.x, scB.z;\n" );

				strcat_s( pALUCode, bufALUCode.Size(), "MOV" );
				strcat_s( pALUCode, bufALUCode.Size(), pDestReg );
				strcat_s( pALUCode, bufALUCode.Size(), ", SC_TEMP;\n" );
				break;

			case D3DSIO_MAD:
				// Print opcode and registers into separate buffers
				PrintOpcode( inst, buff, sizeof( buff ) );
				PrintParameterToString( GetNextToken(), DST_REGISTER, pDestReg, sizeof( pDestReg ), false );
				nARLComp0 = ARL_DEST_NONE;
				PrintParameterToString( GetNextToken(), SRC_REGISTER, pSrc0Reg, sizeof( pSrc0Reg ), false, &nARLComp0 );
				nARLComp1 = ARL_DEST_NONE;
				PrintParameterToString( GetNextToken(), SRC_REGISTER, pSrc1Reg, sizeof( pSrc1Reg ), false, &nARLComp1 );
				nARLComp2 = ARL_DEST_NONE;
				PrintParameterToString( GetNextToken(), SRC_REGISTER, pSrc2Reg, sizeof( pSrc2Reg ), false, &nARLComp2 );

				// This optionally inserts a move from our dummy address register to the .x component of the real one
				InsertMoveFromAddressRegister( pALUCode, bufALUCode.Size(), nARLComp0, nARLComp1, nARLComp2 );

				// Concat this instruction into instruction stream
				strcat_s( pALUCode, bufALUCode.Size(), buff );
				strcat_s( pALUCode, bufALUCode.Size(), pDestReg );
				strcat_s( pALUCode, bufALUCode.Size(), ", " );
				strcat_s( pALUCode, bufALUCode.Size(), pSrc0Reg );
				strcat_s( pALUCode, bufALUCode.Size(), ", " );
				strcat_s( pALUCode, bufALUCode.Size(), pSrc1Reg );
				strcat_s( pALUCode, bufALUCode.Size(), ", " );
				strcat_s( pALUCode, bufALUCode.Size(), pSrc2Reg );
				strcat_s( pALUCode, bufALUCode.Size(), ";\n" );

				break;

			// -- Quaternary op ------------------------------------------------
			case D3DSIO_TEXLDD:
				PrintOpcode( inst, buff, sizeof( buff ) );
				strcat_s( pALUCode, bufALUCode.Size(), buff );
				PrintParameterToString( GetNextToken(), DST_REGISTER, buff, sizeof( buff ), false );
				strcat_s( pALUCode, bufALUCode.Size(), buff );
				strcat_s( pALUCode, bufALUCode.Size(), ", " );
				PrintParameterToString( GetNextToken(), SRC_REGISTER, buff, sizeof( buff ), false );
				strcat_s( pALUCode, bufALUCode.Size(), buff );
				strcat_s( pALUCode, bufALUCode.Size(), ", " );
				PrintParameterToString( GetNextToken(), SRC_REGISTER, buff, sizeof( buff ), false );
				strcat_s( pALUCode, bufALUCode.Size(), buff );
				strcat_s( pALUCode, bufALUCode.Size(), ", " );
				PrintParameterToString( GetNextToken(), SRC_REGISTER, buff, sizeof( buff ), false );
				strcat_s( pALUCode, bufALUCode.Size(), buff );
				strcat_s( pALUCode, bufALUCode.Size(), ", " );
				PrintParameterToString( GetNextToken(), SRC_REGISTER, buff, sizeof( buff ), false );
				strcat_s( pALUCode, bufALUCode.Size(), buff );
				strcat_s( pALUCode, bufALUCode.Size(), ";\n" );

				break;
				
			// -- Special cases: texcoord vs texcrd	and	tex vs texld -----------
			case D3DSIO_TEXCOORD:
				Assert(0);

				// If ps_1_4, this is texcrd
				if ((m_dwMajorVersion == 1) && (m_dwMinorVersion == 4) && (!m_bVertexShader))
				{
					strcat_s( pALUCode, bufALUCode.Size(), "texcrd" );
				}
				else // else it's texcoord
				{
					Assert(0);
					strcat_s( pALUCode, bufALUCode.Size(), "texcoord" );
				}

				PrintParameterToString( GetNextToken(), DST_REGISTER, buff, sizeof( buff ), false );
				strcat_s( pALUCode, bufALUCode.Size(), buff );

				// If ps_1_4, texcrd also has a source parameter
				if ((m_dwMajorVersion == 1) && (m_dwMinorVersion == 4) && (!m_bVertexShader))
				{
					strcat_s( pALUCode, bufALUCode.Size(), ", " );
					PrintParameterToString( GetNextToken(), SRC_REGISTER, buff, sizeof( buff ), false );
					strcat_s( pALUCode, bufALUCode.Size(), buff );
				}

				strcat_s( pALUCode, bufALUCode.Size(), ";\n" );
				break;

			case D3DSIO_TEX:

				if( ( OpcodeSpecificData( dwToken ) << D3DSP_OPCODESPECIFICCONTROL_SHIFT ) == D3DSI_TEXLD_PROJECT )
				{
					strcat_s( pALUCode, bufALUCode.Size(), "TXP" );
				}
				else if( ( OpcodeSpecificData( dwToken ) << D3DSP_OPCODESPECIFICCONTROL_SHIFT) == D3DSI_TEXLD_BIAS )
				{
					strcat_s( pALUCode, bufALUCode.Size(), "TXB" );
				}
				else
				{
					strcat_s( pALUCode, bufALUCode.Size(), "TEX" );
				}

				// Destination
				dwToken = GetNextToken();
				PrintParameterToString( dwToken, DST_REGISTER, buff, sizeof( buff ), false );
				strcat_s( pALUCode, bufALUCode.Size(), buff );
				strcat_s( pALUCode, bufALUCode.Size(), ", " );

				// Source0
				dwToken = GetNextToken();
				PrintParameterToString( dwToken, SRC_REGISTER, buff, sizeof( buff ), false );
				strcat_s( pALUCode, bufALUCode.Size(), buff );
				strcat_s( pALUCode, bufALUCode.Size(), ", " );

				// Source1
				dwToken = GetNextToken();
				PrintParameterToString( dwToken, SRC_REGISTER, buff, sizeof( buff ), false );
				strcat_s( pALUCode, bufALUCode.Size(), buff );
				strcat_s( pALUCode, bufALUCode.Size(), ", " );

				// Syntax for shadow depth sampler
				if ( ( (int) ( dwToken & D3DSP_REGNUM_MASK ) ) == m_nShadowDepthSampler )
				{
					m_bDeclareShadowOption = true;

					strcat_s( pALUCode, bufALUCode.Size(), "SHADOW" ); // Should result in SHADOW2D target
					Assert( m_dwSamplerTypes[dwToken & D3DSP_REGNUM_MASK] == SAMPLER_TYPE_2D );
				}

				// Sampler dimension (2D, CUBE, 3D) determined by earlier declaration
				strcat_s( pALUCode, bufALUCode.Size(), g_pSamplerStrings[m_dwSamplerTypes[dwToken & D3DSP_REGNUM_MASK]] );
				strcat_s( pALUCode, bufALUCode.Size(), ";\n" );
				break;

			case D3DSIO_DCL:

				if ( m_bVertexShader )
				{
					dwToken = GetNextToken();			// What kind of dcl is this...
					dwRegToken = GetNextToken();		// Look ahead to register token

					strcat_s( pAttribCode, bufAttribCode.Size(), "ATTRIB" );
					PrintParameterToString( dwRegToken, DST_REGISTER, buff, sizeof( buff ), false );
					strcat_s( pAttribCode, bufAttribCode.Size(), buff );
					strcat_s( pAttribCode, bufAttribCode.Size(), " = " );

					// maintain attrib map
					// check that this reg index has not been used before - if it has, let Houston know
					uint regIndex = dwRegToken & D3DSP_REGNUM_MASK;
					if (m_dwAttribMap[ regIndex ] == 0xFFFFFFFF)
					{
						// log it
						// semantic/usage in the higher nibble
						// usage index in the low nibble
						
						uint usage		= dwToken & D3DSP_DCL_USAGE_MASK;
						uint usageindex	= ( dwToken & D3DSP_DCL_USAGEINDEX_MASK ) >> D3DSP_DCL_USAGEINDEX_SHIFT;
						
						m_dwAttribMap[ regIndex ] = (usage<<4) | usageindex;
						
						// avoid writing 0xBB since runtime code uses that for an 'unused' marker
						if (m_dwAttribMap[ regIndex ] == 0xBB)
						{
							Debugger();
						}
					}
					else
					{
						//not OK
						Debugger();
					}

					char temp[128];
					// regnum goes straight into the vertex.attrib[n] index
					sprintf( temp, "vertex.attrib[%-2d];	# %08x %08x\n", regIndex, dwToken, dwRegToken );
					strcat_s( pAttribCode, bufAttribCode.Size(), temp );
				}
				else
				{
					dwToken = GetNextToken();			// What kind of dcl is this...
					dwRegToken = GetNextToken();		// Look ahead to register token

					// If the register is a sampler, the dcl has a dimension decorator that we have to save for subsequent TEX instructions
					if ( GetRegType( dwRegToken ) == D3DSPR_SAMPLER )
					{
						switch ( TextureType( dwToken ) )
						{
							default:
							case D3DSTT_UNKNOWN:
							case D3DSTT_2D:
								m_dwSamplerTypes[dwRegToken & D3DSP_REGNUM_MASK] = SAMPLER_TYPE_2D;
								break;
							case D3DSTT_CUBE:
								m_dwSamplerTypes[dwRegToken & D3DSP_REGNUM_MASK] = SAMPLER_TYPE_CUBE;
								break;
							case D3DSTT_VOLUME:
								m_dwSamplerTypes[dwRegToken & D3DSP_REGNUM_MASK] = SAMPLER_TYPE_3D;
								break;
						}
					}
					else // Not a sampler, we're going to generate attribute declaration code
					{
						strcat_s( pAttribCode, bufAttribCode.Size(), "ATTRIB" );
						PrintParameterToString( dwRegToken, DST_REGISTER, buff, sizeof( buff ), false );
						strcat_s( pAttribCode, bufAttribCode.Size(), buff );
						strcat_s( pAttribCode, bufAttribCode.Size(), ";\n" );
					}
				}
				break;

			case D3DSIO_DEFB:
			case D3DSIO_DEFI:
				// Shouldn't be using bool or integer constants
				Assert(0);
				break;

			case D3DSIO_DEF:

				//
				// JasonM TODO: catch D3D's sincos-specific D3DSINCOSCONST1 and D3DSINCOSCONST2 constants and filter them out here
				//

				strcat_s( pParamCode, bufParamCode.Size(), "PARAM" );
				
				// Which register is being defined
				dwToken = GetNextToken();

				// Note that this constant was explicitly defined
				m_bConstantRegisterDefined[dwToken & D3DSP_REGNUM_MASK] = true;

				PrintParameterToString( dwToken, DST_REGISTER, buff, sizeof( buff ), false );
				strcat_s( pParamCode, bufParamCode.Size(), buff );
				strcat_s( pParamCode, bufParamCode.Size(), " = { " );

				// Run through the 4 floats
				for ( i=0; i<4; i++ )
				{
					float fConst = uint32ToFloat_ASM( GetNextToken() );
					V_snprintf( buff, sizeof( buff ), i != 3 ? "%g, " : "%g", fConst); // end with comma-space
					strcat_s( pParamCode, bufParamCode.Size(), buff );
				}

				strcat_s( pParamCode, bufParamCode.Size(), " };\n" );
				break;

			case D3DSIO_COMMENT:
				// Using OpcodeSpecificData() can fail here since the comments can be longer than 0xff dwords
				nNumTokensToSkip = ( dwToken & 0x0fff0000 ) >> 16;
				SkipTokens( nNumTokensToSkip );
				break;

			case D3DSIO_END:
				break;
		}
		
		if ( m_bSpew )
		{
			int aluCodeLength1 = strlen( pALUCode );
			if ( aluCodeLength1 != aluCodeLength0 )
			{
				// code was emitted
				printf( "\n    > %s", pALUCode + aluCodeLength0 );
				
				aluCodeLength0 = aluCodeLength1;
			}
		}
	}

	if ( m_bNeedsSinCosDeclarations )
	{
		// Note that this constant packing expects .wzyx swizzles in case we ever use the SINCOS code in a ps_2_x shader
		//
		// The Microsoft documentation on this is all kinds of broken and, strangely, these numbers don't
		// match the D3DSINCOSCONST1 and D3DSINCOSCONST2 constants used by the D3D assembly sincos instruction...
		strcat_s( pParamCode, bufParamCode.Size(), "PARAM scA = { -1.55009923e-6, -2.17013894e-5, 0.00260416674, 0.00026041668 };\n" );
		strcat_s( pParamCode, bufParamCode.Size(), "PARAM scB = { -0.020833334, -0.0625, 1.0, 0.5 };\n" );
	}

	// Just declare the whole constant store for non-def constants
	if ( m_bVertexShader )
	{
		if ( m_bUseEnvParams )
		{
			strcat_s( pParamCode, bufParamCode.Size(), "PARAM c[256] = { program.env[0..255] };\n" );
		}
		else
		{
			strcat_s( pParamCode, bufParamCode.Size(), "PARAM c[256] = { program.local[0..255] };\n" );
		}
	}
	else
	{
		if ( m_bUseEnvParams )
		{
			strcat_s( pParamCode, bufParamCode.Size(), "PARAM c[32] = { program.env[0..31] };\n" );
		}
		else
		{
			strcat_s( pParamCode, bufParamCode.Size(), "PARAM c[32] = { program.local[0..31] };\n" );
		}
	}

	if ( m_bDeclareAddressReg )
	{
		strcat_s( pParamCode, bufParamCode.Size(), "ADDRESS a0;\n" );
		strcat_s( pParamCode, bufParamCode.Size(), "TEMP VEC_ADDRESS_REG;\n" );
	}

	// Declare temps in Param code buffer
	for( int i=0; i<32; i++ )
	{
		char tempBuff[32];
		if ( m_dwTempUsageMask & ( 0x00000001 << i ) )
		{
			V_snprintf( tempBuff, sizeof( tempBuff ), "TEMP r%d;\n", i );
			strcat_s( pParamCode, bufParamCode.Size(), tempBuff );
		}
	}

	if ( m_bNeedsSinCosDeclarations )
	{
		strcat_s( pParamCode, bufParamCode.Size(), "TEMP SC_TEMP;\n" );
	}

	// Optional temps needed to emulate d2add instruction in DX pixel shaders
	if ( m_bNeedsD2AddTemp )
	{
		strcat_s( pParamCode, bufParamCode.Size(), "TEMP DP2A0;\nTEMP DP2A1;\n" );
	}

	// Optional temp needed to emulate lerp instruction in DX vertex shaders
	if ( m_bNeedsLerpTemp )
	{
		strcat_s( pParamCode, bufParamCode.Size(), "TEMP LRP_TEMP;\n" );
	}

	// Optional temp needed to emulate NRM instruction in DX shaders
	if ( m_bNeedsNRMTemp )
	{
		strcat_s( pParamCode, bufParamCode.Size(), "TEMP NRM_TEMP;\n" );
	}

	// Pixel shader color outputs (MRT support?...just declare MRT outputs as useless TEMPS)
	if ( !m_bVertexShader  )
	{
		if ( m_bOutputColorRegister[1] )
		{
			strcat_s( pParamCode, bufParamCode.Size(), "TEMP oC1;\n" );
		}
		if ( m_bOutputColorRegister[2] )
		{
			strcat_s( pParamCode, bufParamCode.Size(), "TEMP oC2;\n" );
		}
		if ( m_bOutputColorRegister[3] )
		{
			strcat_s( pParamCode, bufParamCode.Size(), "TEMP oC3;\n" );
		}
		if ( m_bOutputColorRegister[0] )
		{
			strcat_s( pParamCode, bufParamCode.Size(), "OUTPUT oC0 = result.color;\n" );
		}
	}

	// this looks stale - ax it ?
	//if (m_bVertexShader && !m_bDoFixupZ)
	//{
	//	static char foo = 1;
	//	if(foo) Debugger();
	//}
	
	if ( m_bDeclareVSOPos && m_bVertexShader )
	{
		if ( m_bDoFixupZ  || m_bDoFixupY)
		{
			// don't write to real reg - declare a temp and then declare a new output reg oPosGL
			strcat_s( pParamCode, bufParamCode.Size(), "TEMP oPos;\n" );
			strcat_s( pParamCode, bufParamCode.Size(), "OUTPUT oPosGL = result.position;\n" );
			
			// TODO: insert clip distance computation something like this:
			//
			// strcat_s( pALUCode, bufALUCode.Size(), "DP4 oCLP[0].x, oPos, c[215]; \n" );
			//

			if (m_bDoFixupZ)
			{
				// append insns to perform Z fixup
				// new Z = (old Z * 2.0) - W
				
				// negate Z, double it, then add the 'w'.
				// near: Z=0 -> Z' = +1.0.  this seems wrong....
				// far:  Z=1 -> Z' = -1.0	uh, this ain't right...
				// strcat_s( pALUCode, bufALUCode.Size(), "MAD r0.z, -oPos.z, c[0].z, oPos.w; # z' = (2*-z)+w \n" );
				
				// double Z, subtract 'w'.
				// near: Z=0 -> Z' = -1.0.
				// far:  Z=1 -> Z' = +1.0
				//strcat_s( pALUCode, bufALUCode.Size(), "MAD r0.z, oPos.z, c[0].z, -oPos.w; # z' = (2*z)-w \n" );
				strcat_s( pALUCode, bufALUCode.Size(), "MAD oPos.z, oPos.z, c[0].z, -oPos.w; # z' = (2*z)-w \n" );
			}

			if (m_bDoFixupY)
			{
				// append insns to flip Y over
				// new Y = -(old Y)
				strcat_s( pALUCode, bufALUCode.Size(), "MOV oPos.y, -oPos.y; # y' = -y \n" );
			}

			strcat_s( pALUCode, bufALUCode.Size(), "MOV oPosGL, oPos; \n" );			
		}
		else
		{
			strcat_s( pParamCode, bufParamCode.Size(), "OUTPUT oPos = result.position;\n" );

			// TODO: insert clip distance computation something like this:
			//
			// strcat_s( pALUCode, bufALUCode.Size(), "DP4 oCLP[0].x, oPos, c[215]; \n" );
			//
		}
	}

	if (m_bVertexShader && m_bDoUserClipPlanes)
	{
		// insert oCLP generation insts
		char	temp[256];

		if(0)
		{
			V_snprintf( temp, sizeof( temp ), "DP4 result.clip[0].x, oPos, c[%d];\n", DXABSTRACT_VS_CLIP_PLANE_BASE );		// ask GLM where to stash the secret params
			V_snprintf( temp, sizeof( temp ), "DP4 result.clip[1].x, oPos, c[%d];\n", DXABSTRACT_VS_CLIP_PLANE_BASE+1 );
		}

		if(0)
		{
			V_snprintf( temp, sizeof( temp ), "DP4 o[CLP0].x, oPos, c[%d];\n", DXABSTRACT_VS_CLIP_PLANE_BASE );		// ask GLM where to stash the secret params
			V_snprintf( temp, sizeof( temp ), "DP4 o[CLP1].x, oPos, c[%d];\n", DXABSTRACT_VS_CLIP_PLANE_BASE+1 );
		}

		if(1)
		{
			V_snprintf( temp, sizeof( temp ), "DP4 oClip0.x, oPos, c[%d];\n", DXABSTRACT_VS_CLIP_PLANE_BASE );		// ask GLM where to stash the secret params
			V_snprintf( temp, sizeof( temp ), "DP4 oClip1.x, oPos, c[%d];\n", DXABSTRACT_VS_CLIP_PLANE_BASE+1 );
		}

		strcat_s( pALUCode, bufALUCode.Size(), temp );			
	}

	if ( m_bDeclareVSOFog && m_bVertexShader )
	{
		strcat_s( pParamCode, bufParamCode.Size(), "OUTPUT oFog = result.fogcoord;\n" );
	}

	for ( int i=0; i<32; i++ )
	{
		char outTexCoordBuff[64];
		if ( m_dwTexCoordOutMask & ( 0x00000001 << i ) )
		{
			V_snprintf( outTexCoordBuff, sizeof( outTexCoordBuff ), "OUTPUT oT%d = result.texcoord[%d];\n", i, i );
			strcat_s( pParamCode, bufParamCode.Size(), outTexCoordBuff );
		}
	}

	if ( m_bOutputColorRegister[0] && m_bVertexShader )
	{
		strcat_s( pParamCode, bufParamCode.Size(), "OUTPUT oD0 = result.color;\n" );
	}

	if ( m_bOutputColorRegister[1] && m_bVertexShader )
	{
		strcat_s( pParamCode, bufParamCode.Size(), "OUTPUT oD1 = result.color.secondary;\n" );
	}

	if ( m_bOutputDepthRegister && !m_bVertexShader )
	{
		strcat_s( pParamCode, bufParamCode.Size(), "OUTPUT oDepth = result.depth;\n" );
	}

	if ( m_bDoUserClipPlanes && m_bVertexShader )
	{
		strcat_s( pParamCode, bufParamCode.Size(), "OUTPUT oClip0 = result.clip[0];\n" );
		strcat_s( pParamCode, bufParamCode.Size(), "OUTPUT oClip1 = result.clip[1];\n" );
	}
	// do some annotation at the end of the attrib block
	{
		char temp[1000];

		if (m_bVertexShader)
		{
			// write attrib map into the text starting at pAttribMapStart - two hex digits per attrib
			for( int i=0; i<16; i++)
			{
				if (m_dwAttribMap[i] != 0xFFFFFFFF)
				{
					V_snprintf( temp, sizeof(temp), "%02X", m_dwAttribMap[i] );
					memcpy( pAttribMapStart + (i*3), temp, 2 );
				}
			}
		}

		V_snprintf( temp, sizeof(temp), "#// trans#%d label:%s\n", g_translationCounter, debugLabel?debugLabel:"none" );
		strcat_s( pAttribCode, bufAttribCode.Size(), temp );

		g_translationCounter++;
	}

	// If we actually sample from a shadow depth sampler, we need to declare the shadow option at the top
	if ( m_bDeclareShadowOption )
	{
		strcat_s( pHeaderCode, nBufLen, "OPTION ARB_fragment_program_shadow;\n" );
	}

	// Put all of the strings together for final program ( pHeaderCode + pAttribCode + pParamCode + pALUCode )
	strcat_s( pHeaderCode, nBufLen, pAttribCode );
	strcat_s( pHeaderCode, nBufLen, pParamCode );
	strcat_s( pHeaderCode, nBufLen, pALUCode );
	strcat_s( pHeaderCode, nBufLen, "END\n\0" );

	if (m_bSpew)
	{
		printf("\n************* translation complete\n\n " );
	}

	return DISASM_OK;
}


#endif