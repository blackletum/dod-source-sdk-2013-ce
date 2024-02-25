//========= Copyright © Valve LLC, All rights reserved. =======================
//
// Purpose:		
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "particle_parse.h"
#include "tf/nav_mesh/tf_nav_area.h"
#include "dod/dod_gamerules.h"
#include "tf_bot.h"
#include "tf_bot_components.h"
#include "player.h"
#include "baseplayer_shared.h"


float CTFBotBody::GetHeadAimTrackingInterval( void ) const
{
	CTFBot *me = static_cast<CTFBot *>( GetBot() );

	switch( me->m_iSkill )
	{
		case CTFBot::NORMAL:
			return 0.30f;
		case CTFBot::HARD:
			return 0.10f;
		case CTFBot::EXPERT:
			return 0.05f;

		default:
			return 1.00f;
	}
}


void CTFBotLocomotion::Update( void )
{
	CTFBot *me = ToTFBot( GetEntity() );
	BaseClass::Update();

	if ( !IsOnGround() )
		me->PressCrouchButton( 0.3f );
}

void CTFBotLocomotion::Approach( const Vector &pos, float goalWeight )
{
	if ( !IsOnGround() && !IsClimbingOrJumping() )
		return;

	BaseClass::Approach( pos, goalWeight );
}

void CTFBotLocomotion::Jump( void )
{
	BaseClass::Jump();
}

float CTFBotLocomotion::GetMaxJumpHeight( void ) const
{
	return 72.0f;
}

float CTFBotLocomotion::GetDeathDropHeight( void ) const
{
	return 1000.0f;
}

float CTFBotLocomotion::GetRunSpeed( void ) const
{
	CTFBot *me = ToTFBot( GetEntity() );
	return me->GetPlayerMaxSpeed();
}

bool CTFBotLocomotion::IsAreaTraversable( const CNavArea *baseArea ) const
{
	if ( !BaseClass::IsAreaTraversable( baseArea ) )
		return false;

	const CTFNavArea *tfArea = static_cast<const CTFNavArea *>( baseArea );
	if ( tfArea == nullptr )
		return false;

	return true;
}

bool CTFBotLocomotion::IsEntityTraversable( CBaseEntity *ent, TraverseWhenType when ) const
{
	if ( ent )
	{
		// always assume we can walk through players, we'll try to avoid them if needed later
		if ( ent->IsPlayer() )
			return true;
	}

	return BaseClass::IsEntityTraversable( ent, when );
}


CTFBotVision::CTFBotVision( INextBot *bot )
	: BaseClass( bot )
{
	m_updateTimer.Start( RandomFloat( 2.0f, 4.0f ) );
}


CTFBotVision::~CTFBotVision()
{
	m_PVNPCs.RemoveAll();
}

void CTFBotVision::Reset( void )
{
	BaseClass::Reset();
	m_PVNPCs.RemoveAll();
}

void CTFBotVision::Update( void )
{
	VPROF_BUDGET( __FUNCTION__, "NextBot" );

	BaseClass::Update();

	CTFBot *me = ToTFBot( GetBot()->GetEntity() );
	if ( me == nullptr )
		return;

	CUtlVector<CDODPlayer *> enemies;
	CollectPlayers( &enemies, GetEnemyTeam( me ), true );
}

void CTFBotVision::CollectPotentiallyVisibleEntities( CUtlVector<CBaseEntity *> *ents )
{
	VPROF_BUDGET( __FUNCTION__, "NextBot" );

	ents->RemoveAll();

	for ( int i=0; i < gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer && pPlayer->IsConnected() && pPlayer->IsAlive() )
		{
			ents->AddToTail( pPlayer );
		}
	}

	UpdatePotentiallyVisibleNPCs();
	for ( int i=0; i < m_PVNPCs.Count(); ++i )
	{
		CBaseEntity *pEntity = m_PVNPCs[i];
		ents->AddToTail( pEntity );
	}
}

bool CTFBotVision::IsVisibleEntityNoticed( CBaseEntity *ent ) const
{
	CTFBot *me = ToTFBot( GetBot()->GetEntity() );

	CDODPlayer *pPlayer = ToDODPlayer( ent );
	if ( pPlayer == nullptr )
	{
		return true;
	}

	// we should always be aware of our "friends"
	if ( !me->IsEnemy( pPlayer ) )
	{
		return true;
	}

	return true;
}

bool CTFBotVision::IsIgnored( CBaseEntity *ent ) const
{
	CTFBot *me = ToTFBot( GetBot()->GetEntity() );
	if ( !me->IsEnemy( ent ) )
		return false;

	if ( ( ent->GetEffects() & EF_NODRAW ) != 0 )
		return true;

	return false;
}

float CTFBotVision::GetMaxVisionRange() const
{
	return 6000.0f;
}

float CTFBotVision::GetMinRecognizeTime( void ) const
{
	CTFBot *me = static_cast<CTFBot *>( GetBot() );

	switch ( me->m_iSkill )
	{
		case CTFBot::NORMAL:
			return 0.50f;
		case CTFBot::HARD:
			return 0.30f;
		case CTFBot::EXPERT:
			return 0.15f;

		default:
			return 1.00f;
	}
}

void CTFBotVision::UpdatePotentiallyVisibleNPCs()
{
	if ( !m_updatePVNPCsTimer.IsElapsed() )
		return;

	VPROF_BUDGET( __FUNCTION__, "NextBot" );

	m_updatePVNPCsTimer.Start( RandomFloat( 2.0f, 4.0f ) );

	m_PVNPCs.RemoveAll();

	CUtlVector<INextBot *> nextbots;
	TheNextBots().CollectAllBots( &nextbots );
	for ( INextBot *pBot : nextbots )
	{
		CBaseCombatCharacter *pEntity = pBot->GetEntity();
		if ( pEntity && !pEntity->IsPlayer() )
			m_PVNPCs.AddToTail( pEntity );
	}
}
