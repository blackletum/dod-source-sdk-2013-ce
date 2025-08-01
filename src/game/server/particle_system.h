//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef PARTICLE_SYSTEM_H
#define PARTICLE_SYSTEM_H
#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"

//-----------------------------------------------------------------------------
// Purpose: An entity that spawns and controls a particle system
//-----------------------------------------------------------------------------
class CParticleSystem : public CBaseEntity
{
	DECLARE_CLASS( CParticleSystem, CBaseEntity );
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CParticleSystem();

	virtual void Precache( void );
	virtual void Spawn( void );
	virtual void Activate( void );
	virtual int  UpdateTransmitState(void);

	void		StartParticleSystem( void );
	void		StopParticleSystem( void );

	void		InputStart( inputdata_t &inputdata );
	void		InputStop( inputdata_t &inputdata );
#ifdef MAPBASE
	void		InputDestroyImmediately( inputdata_t &inputdata );
#endif
	void		StartParticleSystemThink( void );

	enum { kMAXCONTROLPOINTS = 63 }; ///< actually one less than the total number of cpoints since 0 is assumed to be me

	virtual bool UsesCoordinates( void ) { return false; }

protected:

	/// Load up and resolve the entities that are supposed to be the control points 
	void ReadControlPointEnts( void );

	bool				m_bStartActive;
	string_t			m_iszEffectName;
	
	CNetworkVar( bool,	m_bActive );
#ifdef MAPBASE
	CNetworkVar( bool, m_bDestroyImmediately );
#endif
	CNetworkVar( int,	m_iEffectIndex )
	CNetworkVar( float,	m_flStartTime );	// Time at which this effect was started.  This is used after restoring an active effect.

	string_t			m_iszControlPointNames[kMAXCONTROLPOINTS];
	CNetworkArray( EHANDLE, m_hControlPointEnts, kMAXCONTROLPOINTS );
	CNetworkArray( Vector, m_vControlPointVecs, kMAXCONTROLPOINTS );
	CNetworkArray( unsigned char, m_iControlPointParents, kMAXCONTROLPOINTS );
	CNetworkVar( bool,	m_bWeatherEffect );
#ifdef MAPBASE
	CNetworkVar( bool, m_bUsesCoordinates );
#endif
};

//-----------------------------------------------------------------------------
// Purpose: An entity that spawns and controls a particle system using coordinates. 
//-----------------------------------------------------------------------------
class CParticleSystemCoordinate : public CParticleSystem
{
	DECLARE_CLASS( CParticleSystemCoordinate, CParticleSystem );
public:
	virtual bool UsesCoordinates( void ) { return true; }
};

#endif // PARTICLE_SYSTEM_H
