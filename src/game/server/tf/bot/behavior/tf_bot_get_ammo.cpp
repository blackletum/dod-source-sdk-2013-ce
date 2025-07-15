#include "cbase.h"
#include "../tf_bot.h"
#include "dod_gamerules.h"
#include "tf/nav_mesh/tf_nav_mesh.h"
#include "tf_bot_get_ammo.h"


ConVar tf_bot_ammo_search_range( "tf_bot_ammo_search_range", "5000", FCVAR_CHEAT, "How far bots will search to find ammo around them" );
ConVar tf_bot_debug_ammo_scavanging( "tf_bot_debug_ammo_scavanging", "0", FCVAR_CHEAT );


static CHandle<CBaseEntity> s_possibleAmmo;
static CTFBot *s_possibleBot;
static int s_possibleFrame;


CTFBotGetAmmo::CTFBotGetAmmo()
{
	m_PathFollower.Invalidate();
	m_hAmmo = nullptr;
	m_bUsingDispenser = false;
}

CTFBotGetAmmo::~CTFBotGetAmmo()
{
}


const char *CTFBotGetAmmo::GetName() const
{
	return "GetAmmo";
}


ActionResult<CTFBot> CTFBotGetAmmo::OnStart( CTFBot *me, Action<CTFBot> *priorAction )
{
	VPROF_BUDGET( __FUNCTION__, "NextBot" );

	m_PathFollower.SetMinLookAheadDistance( me->GetDesiredPathLookAheadRange() );

	if ( ( gpGlobals->framecount != s_possibleFrame || s_possibleBot != me ) && ( !IsPossible( me ) || !s_possibleAmmo ) )
		return Action<CTFBot>::Done( "Can't get ammo" );

	m_hAmmo = s_possibleAmmo;
	m_bUsingDispenser = s_possibleAmmo->ClassMatches( "obj_dispenser*" );

	CTFBotPathCost cost( me, FASTEST_ROUTE );
	if ( !m_PathFollower.Compute( me, m_hAmmo->WorldSpaceCenter(), cost ) )
		return Action<CTFBot>::Done( "No path to ammo" );

	return Action<CTFBot>::Continue();
}

ActionResult<CTFBot> CTFBotGetAmmo::Update( CTFBot *me, float dt )
{
	if ( me->IsAmmoFull() )
		return Action<CTFBot>::Done( "My ammo is full" );

	if ( !m_hAmmo )
		return Action<CTFBot>::Done( "Ammo I was going for has been taken" );

	if ( m_bUsingDispenser && 
		( me->GetAbsOrigin() - m_hAmmo->GetAbsOrigin() ).LengthSqr() < Square( 75.0f ) &&
		me->GetVisionInterface()->IsLineOfSightClearToEntity(m_hAmmo) )
	{
		if ( me->IsAmmoFull() )
			return Action<CTFBot>::Done( "Ammo refilled by the Dispenser" );

		if ( !me->IsAmmoLow() )
		{
			if ( me->GetVisionInterface()->GetPrimaryKnownThreat() )
				return Action<CTFBot>::Done( "No time to wait for more ammo, I must fight" );
		}
	}

	if ( !m_PathFollower.IsValid() )
		return Action<CTFBot>::Done( "My path became invalid" );

	const CKnownEntity *threat = me->GetVisionInterface()->GetPrimaryKnownThreat();
	me->EquipBestWeaponForThreat( threat );

	m_PathFollower.Update( me );

	return Action<CTFBot>::Continue();
}


EventDesiredResult<CTFBot> CTFBotGetAmmo::OnContact( CTFBot *me, CBaseEntity *other, CGameTrace *result )
{
	return Action<CTFBot>::TryContinue();
}

EventDesiredResult<CTFBot> CTFBotGetAmmo::OnMoveToSuccess( CTFBot *me, const Path *path )
{
	return Action<CTFBot>::TryContinue();
}

EventDesiredResult<CTFBot> CTFBotGetAmmo::OnMoveToFailure( CTFBot *me, const Path *path, MoveToFailureType fail )
{
	return Action<CTFBot>::TryDone( RESULT_CRITICAL, "Failed to reach ammo" );
}

EventDesiredResult<CTFBot> CTFBotGetAmmo::OnStuck( CTFBot *me )
{
	return Action<CTFBot>::TryDone( RESULT_CRITICAL, "Stuck trying to reach ammo" );
}


QueryResultType CTFBotGetAmmo::ShouldHurry( const INextBot *me ) const
{
	return ANSWER_YES;
}


bool CTFBotGetAmmo::IsPossible( CTFBot *actor )
{
	VPROF_BUDGET( __FUNCTION__, "NextBot" );

	CUtlVector<EHANDLE> ammos;
	for ( CBaseEntity *pEntity = gEntList.FirstEnt(); pEntity; pEntity = gEntList.NextEnt( pEntity ) )
	{
		if ( pEntity->ClassMatches( "tf_ammo_pack" ) )
		{
			EHANDLE hndl( pEntity );
			ammos.AddToTail( hndl );
		}
	}

	return false;
}