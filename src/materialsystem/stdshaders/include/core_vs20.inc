// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH

#pragma once
#include "shaderlib/cshader.h"
class core_vs20_Static_Index
{
	unsigned int m_nMODEL : 2;
#ifdef _DEBUG
	bool m_bMODEL : 1;
#endif	// _DEBUG
public:
	void SetMODEL( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nMODEL = i;
#ifdef _DEBUG
		m_bMODEL = true;
#endif	// _DEBUG
	}

	core_vs20_Static_Index(  )
	{
		m_nMODEL = 0;
#ifdef _DEBUG
		m_bMODEL = false;
#endif	// _DEBUG
	}

	int GetIndex() const
	{
		Assert( m_bMODEL );
		return ( 4 * m_nMODEL ) + 0;
	}
};

#define shaderStaticTest_core_vs20 vsh_forgot_to_set_static_MODEL


class core_vs20_Dynamic_Index
{
	unsigned int m_nCOMPRESSED_VERTS : 2;
	unsigned int m_nSKINNING : 2;
#ifdef _DEBUG
	bool m_bCOMPRESSED_VERTS : 1;
	bool m_bSKINNING : 1;
#endif	// _DEBUG
public:
	void SetCOMPRESSED_VERTS( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCOMPRESSED_VERTS = i;
#ifdef _DEBUG
		m_bCOMPRESSED_VERTS = true;
#endif	// _DEBUG
	}

	void SetSKINNING( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSKINNING = i;
#ifdef _DEBUG
		m_bSKINNING = true;
#endif	// _DEBUG
	}

	core_vs20_Dynamic_Index(  )
	{
		m_nCOMPRESSED_VERTS = 0;
		m_nSKINNING = 0;
#ifdef _DEBUG
		m_bCOMPRESSED_VERTS = false;
		m_bSKINNING = false;
#endif	// _DEBUG
	}

	int GetIndex() const
	{
		Assert( m_bCOMPRESSED_VERTS && m_bSKINNING );
		return ( 1 * m_nCOMPRESSED_VERTS ) + ( 2 * m_nSKINNING ) + 0;
	}
};

#define shaderDynamicTest_core_vs20 vsh_forgot_to_set_dynamic_COMPRESSED_VERTS + vsh_forgot_to_set_dynamic_SKINNING

