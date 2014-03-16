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
//------------------------------------------------------------------------------
// DX9AsmToGL.h
//------------------------------------------------------------------------------

#ifndef DX9_ASM_TO_GL_H
#define DX9_ASM_TO_GL_H

#define DISASM_OK      0
#define DISASM_ERROR   1

#define MAX_SHADER_CONSTANTS	512

// option bits
#define D3DToGL_OptionUseEnvParams		0x01
#define D3DToGL_OptionDoFixupZ			0x02			// add insts to put Z in the right interval for GL
#define D3DToGL_OptionDoFixupY			0x04			// add insts to flip the Y over for GL
#define D3DToGL_OptionDoUserClipPlanes	0x08			// include OPTION vertex_program_2 and append DP4's to write into oCLP[0] and oCLP[1]
#define D3DToGL_OptionSpew			0x80000000

class D3DToGL_ASM
{
private:
	// Pointers for dwToken stream management
	uint32* m_pdwBaseToken;
	uint32* m_pdwNextToken;

	// Vertex shader or pixel shader, and version (necessary because some opcodes alias)
	bool m_bVertexShader;
	uint32 m_dwMinorVersion;
	uint32 m_dwMajorVersion;
	
	// Option flags
	bool	m_bUseEnvParams;		// set D3DToGL_OptionUseEnvParams in 'options' to use
	bool	m_bDoFixupZ;			// set D3DToGL_OptionDoFixupZ
	bool	m_bDoFixupY;			// set D3DToGL_OptionDoFixupZ
	bool	m_bDoUserClipPlanes;	// set D3DToGL_OptionDoUserClipPlanes
	bool	m_bSpew;				// set D3DToGL_OptionSpew

	// Various scratch temps needed to handle mis-matches in instruction sets between D3D and OpenGL
	bool m_bNeedsD2AddTemp;
	bool m_bNeedsNRMTemp;
	bool m_bDeclareAddressReg;
	bool m_bNeedsLerpTemp;
	bool m_bNeedsSinCosDeclarations;

	// Keep track of which vs outputs are used so we can declare them
	bool m_bDeclareVSOPos;
	bool m_bDeclareVSOFog;
	uint32 m_dwTexCoordOutMask;

	// Keep track of which temps are used so they can be declared
	uint32 m_dwTempUsageMask;
	bool m_bOutputColorRegister[2];
	bool m_bOutputDepthRegister;

	// Track constants so we know how to declare them
	bool m_bConstantRegisterReferenced[MAX_SHADER_CONSTANTS];
	bool m_bConstantRegisterDefined[MAX_SHADER_CONSTANTS];

	// Track sampler types when declared so we can properly decorate TEX instructions
	uint32 m_dwSamplerTypes[32];

	// Track shadow sampler usage
	int m_nShadowDepthSampler;
	bool m_bDeclareShadowOption;

	// Track attribute references
	// init to 0xFFFFFFFF (unhit)
	// index by (dwRegToken & D3DSP_REGNUM_MASK) in VS DCL insns
	// fill with (usage<<4) | (usage index).
	uint32 m_dwAttribMap[16];	

	// GLSL does indentation for readability
	int m_NumIndentTabs;

	// Utilities to aid in decoding token stream
	uint32 GetNextToken( void );
	void SkipTokens( uint32 numToSkip );
	uint32 Opcode( uint32 dwToken );
	uint32 OpcodeSpecificData( uint32 dwToken );
	uint32 TextureType ( uint32 dwToken );
	uint32 GetRegType( uint32 dwRegToken );

	// Utilities for decoding tokens in to strings according to ASM syntax
	void PrintOpcode( uint32 inst, char* buff, int nBufLen );
	void PrintUsageAndIndexToString( uint32 dwToken, char* strUsageUsageIndexName, int nBufLen, bool bGLSL );
	void PrintParameterToString ( uint32 dwToken, uint32 dwSourceOrDest, char *pRegisterName, int nBufLen, bool bGLSL, int *pARLDestReg );
	void InsertMoveFromAddressRegister( char *pCode, int nCodeSize, int nARLComp0, int nARLComp1, int nARLComp2 );
	void InsertMoveInstruction( char *pCode, int nCodeSize, int nARLComponent );
	void FlagIndirectRegister( uint32 dwToken, int *pARLDestReg );

	// Utilities for decoding tokens in to strings according to GLSL syntax
	void OpenIntrinsic( uint32 inst, char* buff, int nBufLen );
	void PrintIndentation( char *pBuf, int nBufLen );

public:
	int TranslateShader( uint32* code, char *pDisassembledCode, int nBufLen, bool *bVertexShader, uint32 options, int32 nShadowDepthSampler, char *debugLabel );
};


#endif // DX9_ASM_TO_GL_H
