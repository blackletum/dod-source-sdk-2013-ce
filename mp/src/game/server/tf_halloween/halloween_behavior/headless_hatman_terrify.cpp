//========= Copyright Valve Corporation, All rights reserved. ============//
// headless_hatman_terrify.cpp
// The Halloween Boss leans over and yells "Boo!", terrifying nearby victims
// Michael Booth, October 2010

#include "cbase.h"

#include "dod_player.h"
#include "dod_gamerules.h"
#include "dod_shareddefs.h"

#include "../headless_hatman.h"
#include "headless_hatman_terrify.h"


//---------------------------------------------------------------------------------------------
ActionResult< CHeadlessHatman >	CHeadlessHatmanTerrify::OnStart( CHeadlessHatman *me, Action< CHeadlessHatman > *priorAction )
{
	me->AddGesture( ACT_MP_GESTURE_VC_HANDMOUTH_ITEM1 );

	m_booTimer.Start( 0.25f );
	m_scareTimer.Start( 0.75f );
	m_timer.Start( 1.25f );

	return Continue();
}


//---------------------------------------------------------------------------------------------
ActionResult< CHeadlessHatman >	CHeadlessHatmanTerrify::Update( CHeadlessHatman *me, float interval )
{
	if ( m_timer.IsElapsed() )
	{
		return Done();
	}

	if ( m_booTimer.HasStarted() && m_booTimer.IsElapsed() )
	{
		m_booTimer.Invalidate();
		me->EmitSound( "Halloween.HeadlessBossBoo" );
	}

	if ( m_scareTimer.IsElapsed() )
	{
		CUtlVector< CDODPlayer * > playerVector;
		CollectPlayers( &playerVector, TEAM_ALLIES, COLLECT_ONLY_LIVING_PLAYERS );
		CollectPlayers( &playerVector, TEAM_AXIS, COLLECT_ONLY_LIVING_PLAYERS, APPEND_PLAYERS );
	}

	return Continue();
}
