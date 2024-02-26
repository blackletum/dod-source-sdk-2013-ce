//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Team spawnpoint entity
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_TEAMSPAWNPOINT_H
#define TF_TEAMSPAWNPOINT_H
#pragma once

#include "baseentity.h"
#include "entityoutput.h"

class CTeam;

//-----------------------------------------------------------------------------
// Purpose: points at which the player can spawn, restricted by team
//-----------------------------------------------------------------------------
class CTeamSpawnPoint : public CPointEntity
{
public:
	DECLARE_CLASS( CTeamSpawnPoint, CPointEntity );

	void	Activate( void );
	virtual bool	IsValid( CBasePlayer *pPlayer );

	COutputEvent m_OnPlayerSpawn;

protected:	
	int		m_iDisabled;

	// Input handlers
	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );

	DECLARE_DATADESC();
};

//-----------------------------------------------------------------------------
// Purpose: points at which vehicles can spawn, restricted by team
//-----------------------------------------------------------------------------
class CTeamVehicleSpawnPoint : public CTeamSpawnPoint
{
	DECLARE_CLASS( CTeamVehicleSpawnPoint, CTeamSpawnPoint );
public:
	void	Activate( void );
	bool	IsValid( void );

	COutputEvent m_OnVehicleSpawn;

	DECLARE_DATADESC();
};

//=============================================================================
//
// TF team spawning entity.
//

enum PlayerTeamSpawnMode_t
{
	PlayerTeamSpawnMode_Normal = 0,
	PlayerTeamSpawnMode_Triggered = 1,
};

enum PlayerTeamSpawn_MatchSummary_t
{
	PlayerTeamSpawn_MatchSummary_None = 0,
	PlayerTeamSpawn_MatchSummary_Loser = 1,
	PlayerTeamSpawn_MatchSummary_Winner = 2,
};

DECLARE_AUTO_LIST(ITFTeamSpawnAutoList);

class CTFTeamSpawn : public CPointEntity, public ITFTeamSpawnAutoList
{
public:
	DECLARE_CLASS(CTFTeamSpawn, CPointEntity);

	CTFTeamSpawn();

	void Activate(void);

	bool IsDisabled(void) { return m_bDisabled; }
	void SetDisabled(bool bDisabled) { m_bDisabled = bDisabled; }

	PlayerTeamSpawnMode_t GetTeamSpawnMode(void) { return m_nSpawnMode; }

	// Inputs/Outputs.
	void InputEnable(inputdata_t& inputdata);
	void InputDisable(inputdata_t& inputdata);
	void InputRoundSpawn(inputdata_t& inputdata);

	int DrawDebugTextOverlays(void);

	PlayerTeamSpawn_MatchSummary_t GetMatchSummaryType(void) { return m_nMatchSummaryType; }
	bool AlreadyUsedForMatchSummary(void) { return m_bAlreadyUsedForMatchSummary; }
	void SetAlreadyUsedForMatchSummary(void) { m_bAlreadyUsedForMatchSummary = true; }

private:
	bool							m_bDisabled;		// Enabled/Disabled?
	PlayerTeamSpawnMode_t			m_nSpawnMode;		// How are players allowed to spawn here?

	string_t						m_iszControlPointName;
	string_t						m_iszRoundBlueSpawn;
	string_t						m_iszRoundRedSpawn;

	PlayerTeamSpawn_MatchSummary_t	m_nMatchSummaryType;		// is this a spawn location for a match summary?
	bool m_bAlreadyUsedForMatchSummary;

	DECLARE_DATADESC();
};


#endif // TF_TEAMSPAWNPOINT_H
