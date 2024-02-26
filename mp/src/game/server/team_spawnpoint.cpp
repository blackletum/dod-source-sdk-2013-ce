//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Team spawnpoint handling
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "entitylist.h"
#include "entityoutput.h"
#include "player.h"
#include "eventqueue.h"
#include "gamerules.h"
#include "team_spawnpoint.h"
#include "team.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//=============================================================================
//
// CTFTeamSpawn tables.
//
BEGIN_DATADESC(CTFTeamSpawn)

DEFINE_KEYFIELD(m_bDisabled, FIELD_BOOLEAN, "StartDisabled"),
DEFINE_KEYFIELD(m_iszControlPointName, FIELD_STRING, "controlpoint"),
DEFINE_KEYFIELD(m_iszRoundBlueSpawn, FIELD_STRING, "round_bluespawn"),
DEFINE_KEYFIELD(m_iszRoundRedSpawn, FIELD_STRING, "round_redspawn"),
DEFINE_KEYFIELD(m_nSpawnMode, FIELD_INTEGER, "SpawnMode"),
DEFINE_KEYFIELD(m_nMatchSummaryType, FIELD_INTEGER, "MatchSummary"),

// Inputs.
DEFINE_INPUTFUNC(FIELD_VOID, "Enable", InputEnable),
DEFINE_INPUTFUNC(FIELD_VOID, "Disable", InputDisable),
DEFINE_INPUTFUNC(FIELD_VOID, "RoundSpawn", InputRoundSpawn),

// Outputs.

END_DATADESC()

IMPLEMENT_AUTO_LIST(ITFTeamSpawnAutoList);

LINK_ENTITY_TO_CLASS(info_player_teamspawn, CTFTeamSpawn);

//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CTFTeamSpawn::CTFTeamSpawn()
{
	m_bDisabled = false;
	m_nMatchSummaryType = PlayerTeamSpawn_MatchSummary_None;
	m_bAlreadyUsedForMatchSummary = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTeamSpawn::Activate(void)
{
	BaseClass::Activate();

	Vector mins = VEC_HULL_MIN;
	Vector maxs = VEC_HULL_MAX;

	trace_t trace;
	UTIL_TraceHull(GetAbsOrigin(), GetAbsOrigin(), mins, maxs, MASK_PLAYERSOLID, NULL, COLLISION_GROUP_PLAYER_MOVEMENT, &trace);
	bool bClear = (trace.fraction == 1 && trace.allsolid != 1 && (trace.startsolid != 1));
	if (!bClear)
	{
		DevMsg("Spawnpoint at (%.2f %.2f %.2f) is not clear.\n", GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z);
		// m_debugOverlays |= OVERLAY_TEXT_BIT;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFTeamSpawn::InputEnable(inputdata_t& inputdata)
{
	m_bDisabled = false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFTeamSpawn::InputDisable(inputdata_t& inputdata)
{
	m_bDisabled = true;
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Input  :
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CTFTeamSpawn::DrawDebugTextOverlays(void)
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT)
	{
		char tempstr[512];
		Q_snprintf(tempstr, sizeof(tempstr), "TeamNumber: %d", GetTeamNumber());
		EntityText(text_offset, tempstr, 0);
		text_offset++;

		color32 teamcolor = {255, 255, 255, 0};
		teamcolor.a = 0;

		if (m_bDisabled)
		{
			Q_snprintf(tempstr, sizeof(tempstr), "DISABLED");
			EntityText(text_offset, tempstr, 0);
			text_offset++;

			teamcolor.a = 255;
		}

		// Make sure it's empty
		Vector mins = VEC_HULL_MIN;
		Vector maxs = VEC_HULL_MAX;

		Vector vTestMins = GetAbsOrigin() + mins;
		Vector vTestMaxs = GetAbsOrigin() + maxs;

		// First test the starting origin.
		if (UTIL_IsSpaceEmpty(NULL, vTestMins, vTestMaxs))
		{
			NDebugOverlay::Box(GetAbsOrigin(), mins, maxs, teamcolor.r, teamcolor.g, teamcolor.b, teamcolor.a, 0.1);
		}
		else
		{
			NDebugOverlay::Box(GetAbsOrigin(), mins, maxs, 0, 255, 0, 0, 0.1);
		}
	}
	return text_offset;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFTeamSpawn::InputRoundSpawn(inputdata_t& input)
{
}

LINK_ENTITY_TO_CLASS( info_player_teamspawn_DEPRECATED, CTeamSpawnPoint );

BEGIN_DATADESC( CTeamSpawnPoint )

	// keys
	DEFINE_KEYFIELD( m_iDisabled, FIELD_INTEGER, "StartDisabled" ),

	// input functions
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),

	// outputs
	DEFINE_OUTPUT( m_OnPlayerSpawn, "OnPlayerSpawn" ),

END_DATADESC()


//-----------------------------------------------------------------------------
// Purpose: Attach this spawnpoint to it's team
//-----------------------------------------------------------------------------
void CTeamSpawnPoint::Activate( void )
{
	BaseClass::Activate();
	if ( GetTeamNumber() > 0 && GetTeamNumber() <= MAX_TEAMS )
	{
		GetGlobalTeam( GetTeamNumber() )->AddSpawnpoint( this );
	}
	else
	{
		Warning( "info_player_teamspawn with invalid team number: %d\n", GetTeamNumber() );
		UTIL_Remove( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Is this spawnpoint ready for a player to spawn in?
//-----------------------------------------------------------------------------
bool CTeamSpawnPoint::IsValid( CBasePlayer *pPlayer )
{
	CBaseEntity *ent = NULL;
	for ( CEntitySphereQuery sphere( GetAbsOrigin(), 128 ); ( ent = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
	{
		// if ent is a client, don't spawn on 'em
		CBaseEntity *plent = ent;
		if ( plent && plent->IsPlayer() && plent != pPlayer )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTeamSpawnPoint::InputEnable( inputdata_t &inputdata )
{
	m_iDisabled = FALSE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTeamSpawnPoint::InputDisable( inputdata_t &inputdata )
{
	m_iDisabled = TRUE;
}


//===========================================================================================================
// VEHICLE SPAWNPOINTS
//===========================================================================================================
LINK_ENTITY_TO_CLASS( info_vehicle_groundspawn, CTeamVehicleSpawnPoint );

BEGIN_DATADESC( CTeamVehicleSpawnPoint )

	// outputs
	DEFINE_OUTPUT( m_OnVehicleSpawn, "OnVehicleSpawn" ),

END_DATADESC()


//-----------------------------------------------------------------------------
// Purpose: Is this spawnpoint ready for a vehicle to spawn in?
//-----------------------------------------------------------------------------
bool CTeamVehicleSpawnPoint::IsValid( void )
{
	CBaseEntity *ent = NULL;
	for ( CEntitySphereQuery sphere( GetAbsOrigin(), 128 ); ( ent = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
	{
		// if ent is a client, don't spawn on 'em
		CBaseEntity *plent = ent;
		if ( plent && plent->IsPlayer() )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Attach this spawnpoint to it's team
//-----------------------------------------------------------------------------
void CTeamVehicleSpawnPoint::Activate( void )
{
	BaseClass::Activate();
	if ( GetTeamNumber() > 0 && GetTeamNumber() <= MAX_TEAMS )
	{
		// Don't add vehicle spawnpoints to the team for now
		//GetGlobalTeam( GetTeamNumber() )->AddSpawnpoint( this );
	}
	else
	{
		Warning( "info_vehicle_groundspawn with invalid team number: %d\n", GetTeamNumber() );
		UTIL_Remove( this );
	}
}
