

#include "cbase.h"
#include "tf/nav_mesh/tf_nav_mesh.h"
#include "tf/bot/tf_bot.h"
#include "tf_bot_sniper_lurk.h"
#include "tf/bot/behavior/tf_bot_melee_attack.h"
#include "dod/weapon_dodsniper.h"

ConVar doc_bot_debug_sniper( "doc_bot_debug_sniper", "0", FCVAR_CHEAT );
ConVar doc_bot_sniper_patience_duration( "doc_bot_sniper_patience_duration", "10", FCVAR_CHEAT, "How long a Sniper bot will wait without seeing an enemy before picking a new spot" );
ConVar doc_bot_sniper_target_linger_duration( "doc_bot_sniper_target_linger_duration", "2", FCVAR_CHEAT, "How long a Sniper bot will keep toward at a target it just lost sight of" );
ConVar doc_bot_sniper_allow_opportunistic( "doc_bot_sniper_allow_opportunistic", "1", FCVAR_NONE, "If set, Snipers will stop on their way to their preferred lurking spot to snipe at opportunistic targets" );
extern ConVar doc_bot_sniper_melee_range;


CTFBotSniperLurk::CTFBotSniperLurk()
{
}

CTFBotSniperLurk::~CTFBotSniperLurk()
{
}


const char *CTFBotSniperLurk::GetName() const
{
	return "SniperLurk";
}


ActionResult<CTFBot> CTFBotSniperLurk::OnStart( CTFBot *me, Action<CTFBot> *priorAction )
{
	m_patienceDuration.Start( RandomFloat( 0.9f, 1.1f ) * doc_bot_sniper_patience_duration.GetFloat() );

	m_vecHome = me->GetAbsOrigin();
	m_bHasHome = false;
	m_bNearHome = false;
	unused = 0;
	m_bOpportunistic = doc_bot_sniper_allow_opportunistic.GetBool();

	CBaseEntity *pEntity = nullptr;
	while ( ( pEntity = gEntList.FindEntityByClassname( pEntity, "func_tfbot_hint" ) ) != nullptr )
	{
		CTFBotHint *pHint = static_cast<CTFBotHint *>( pEntity );

		if ( pHint->m_hint == CTFBotHint::SNIPER_SPOT )
		{
			m_Hints.AddToTail( pHint );

			if ( me->IsSelf( pHint->GetOwnerEntity() ) )
				pHint->SetOwnerEntity( nullptr );
		}
	}

	m_hHint = nullptr;

	return Action<CTFBot>::Continue();
}

ActionResult<CTFBot> CTFBotSniperLurk::Update( CTFBot *me, float dt )
{
	me->AccumulateSniperSpots();

	if ( !m_bHasHome )
		FindNewHome( me );

	bool bWantsToZoom = false;

	const CKnownEntity *threat = me->GetVisionInterface()->GetPrimaryKnownThreat();
	if ( threat != nullptr && threat->GetEntity()->IsAlive() && me->GetIntentionInterface()->ShouldAttack( me, threat ) )
	{
		if ( threat->IsVisibleInFOVNow() )
		{
			unused = 0;

			if ( threat->GetLastKnownPosition().DistToSqr( me->GetAbsOrigin() ) < Square( doc_bot_sniper_melee_range.GetFloat() ) )
				return Action<CTFBot>::SuspendFor( new CTFBotMeleeAttack( 1.25f * doc_bot_sniper_melee_range.GetFloat() ), "Melee attacking nearby threat" );
		}

		if ( threat->GetTimeSinceLastSeen() < doc_bot_sniper_target_linger_duration.GetFloat() && me->IsLineOfFireClear( threat->GetEntity() ) )
		{
			if ( m_bOpportunistic )
			{
				CBaseCombatWeapon *pPrimary = me->Weapon_GetSlot( 0 );
				if ( pPrimary != nullptr )
				{
					me->Weapon_Switch( pPrimary );
				}

				m_patienceDuration.Reset();

				bWantsToZoom = true;

				if ( !m_bHasHome )
				{
					m_vecHome = me->GetAbsOrigin();

					m_patienceDuration.Start( RandomFloat( 0.9f, 1.1f ) * doc_bot_sniper_patience_duration.GetFloat() );
				}
			}
			else
			{
				CBaseCombatWeapon *pSecondary = me->Weapon_GetSlot( 1 );
				if ( pSecondary != nullptr )
				{
					me->Weapon_Switch( pSecondary );
				}
			}
		}
	}

	float flDistToHome = ( me->GetAbsOrigin().AsVector2D() - m_vecHome.AsVector2D() ).LengthSqr();
	m_bNearHome = ( flDistToHome < Square( 25.0f ) );

	if ( m_bNearHome )
	{
		bWantsToZoom = true;
		m_bOpportunistic = doc_bot_sniper_allow_opportunistic.GetBool();

		if ( m_patienceDuration.IsElapsed() )
		{
			++unused;

			if ( FindNewHome( me ) )
			{
				me->VoiceCommand(17);

				m_patienceDuration.Start( RandomFloat( 0.9f, 1.1f ) * doc_bot_sniper_patience_duration.GetFloat() );
			}
			else
			{
				m_patienceDuration.Start( 1.0f );
			}
		}
	}
	else
	{
		m_patienceDuration.Reset();

		if ( !bWantsToZoom )
		{
			if ( m_recomputePathTimer.IsElapsed() )
			{
				m_recomputePathTimer.Start( RandomFloat( 1.0f, 2.0f ) );

				CTFBotPathCost cost( me, SAFEST_ROUTE );
				m_PathFollower.Compute( me, m_vecHome, cost );
			}

			m_PathFollower.Update( me );

			CDODSniperWeapon* pPrimary = (CDODSniperWeapon*)me->Weapon_GetSlot(0);

			if (pPrimary != nullptr && pPrimary->IsZoomed())
			{
				me->PressAltFireButton();
			}

			return Action<CTFBot>::Continue();
		}
	}

	CBaseCombatWeapon *pPrimary = me->Weapon_GetSlot( 0 );
	if ( pPrimary != nullptr )
	{
		me->Weapon_Switch( pPrimary );

		CDODSniperWeapon* pPrimary = (CDODSniperWeapon*)me->Weapon_GetSlot(0);

		if (pPrimary != nullptr && !pPrimary->IsZoomed())
		{
			me->PressAltFireButton();
		}
	}

	return Action<CTFBot>::Continue();
}

void CTFBotSniperLurk::OnEnd( CTFBot *me, Action<CTFBot> *newAction )
{
	CDODSniperWeapon* pPrimary = (CDODSniperWeapon*)me->Weapon_GetSlot(0);

	if (pPrimary != nullptr && pPrimary->IsZoomed())
	{
		me->PressAltFireButton();
	}

	if ( m_hHint )
	{
		m_hHint->SetOwnerEntity( nullptr );

		if (doc_bot_debug_sniper.GetBool() )
		{
			DevMsg( "%3.2f: %s: Releasing hint.\n", gpGlobals->curtime, me->GetPlayerName() );
		}
	}
}

ActionResult<CTFBot> CTFBotSniperLurk::OnSuspend( CTFBot *me, Action<CTFBot> *newAction )
{
	CDODSniperWeapon* pPrimary = (CDODSniperWeapon*)me->Weapon_GetSlot(0);

	if (pPrimary != nullptr && pPrimary->IsZoomed())
	{
		me->PressAltFireButton();
	}

	if ( m_hHint )
	{
		m_hHint->SetOwnerEntity( nullptr );

		if (doc_bot_debug_sniper.GetBool() )
		{
			DevMsg( "%3.2f: %s: Releasing hint.\n", gpGlobals->curtime, me->GetPlayerName() );
		}
	}

	return Action<CTFBot>::Continue();
}

ActionResult<CTFBot> CTFBotSniperLurk::OnResume( CTFBot *me, Action<CTFBot> *priorAction )
{
	m_recomputePathTimer.Invalidate();

	m_hHint = nullptr;
	FindNewHome( me );

	return Action<CTFBot>::Continue();
}


QueryResultType CTFBotSniperLurk::ShouldRetreat( const INextBot *me ) const
{
	return ANSWER_UNDEFINED;
}

QueryResultType CTFBotSniperLurk::ShouldAttack( const INextBot *me, const CKnownEntity *threat ) const
{
	return ANSWER_YES;
}


bool CTFBotSniperLurk::FindHint( CTFBot *actor )
{
	CUtlVector<CTFBotHint *> hints;
	for ( CTFBotHint *pHint : m_Hints )
	{
		if ( !pHint->IsFor( actor ) )
			continue;

		hints.AddToTail( pHint );
	}

	if ( m_hHint )
	{
		m_hHint->SetOwnerEntity( NULL );
		if (doc_bot_debug_sniper.GetBool() )
			DevMsg( "%3.2f: %s: Releasing hint.", gpGlobals->curtime, actor->GetPlayerName() );
	}

	if ( hints.IsEmpty() )
		return false;

	CTFBotHint *pSelected = nullptr;
	if ( !m_hHint || m_Hints.Count() > 1 )
	{
		CUtlVector<CDODPlayer *> enemies;
		CollectPlayers( &enemies, GetEnemyTeam( actor ), true );

		CUtlVector<CTFBotHint *> emptyHints;
		CUtlVector<CTFBotHint *> dangerHints;
		for ( CTFBotHint *pHint : hints )
		{
			if ( pHint->GetOwnerEntity() == nullptr )
			{
				emptyHints.AddToTail( pHint );

				for ( CDODPlayer *pPlayer : enemies )
				{
					if ( !pPlayer->IsLineOfSightClear( pHint->WorldSpaceCenter(), CBaseCombatCharacter::IGNORE_ACTORS ) )
						continue;

					dangerHints.AddToTail( pHint );
				}
			}
		}

		if ( !dangerHints.IsEmpty() )
		{
			pSelected = dangerHints.Random();
		}
		else
		{
			if ( !emptyHints.IsEmpty() )
			{
				pSelected = emptyHints.Random();
			}
			else
			{
				pSelected = hints.Random();

				if (doc_bot_debug_sniper.GetBool() )
					DevMsg( "%3.2f: %s: No un-owned hints available! Doubling up.\n", gpGlobals->curtime, actor->GetPlayerName() );
			}
		}

		if ( pSelected )
		{
			Vector vecMins, vecMaxs;
			pSelected->GetCollideable()->WorldSpaceSurroundingBounds( &vecMins, &vecMaxs );

			Vector vecRandomBounds;
			vecRandomBounds.x = RandomFloat( vecMins.x, vecMaxs.x );
			vecRandomBounds.y = RandomFloat( vecMins.y, vecMaxs.y );
			vecRandomBounds.z = ( vecMaxs.z + vecMins.z ) / 2.0f;

			TheNavMesh->GetSimpleGroundHeight( vecRandomBounds, &vecRandomBounds.z );

			m_bHasHome = true;
			m_vecHome = vecRandomBounds;
			m_hHint = pSelected;
			pSelected->SetOwnerEntity( actor );

			return true;
		}

		return false;
	}

	CUtlVector<CTFBotHint *> backupHints;
	if ( hints.IsEmpty() )
	{
		++unused;
		return false;
	}

	for ( CTFBotHint *pHint : hints )
	{
		if ( m_hHint == pHint )
			continue;

		if ( m_hHint->WorldSpaceCenter().DistToSqr( pHint->WorldSpaceCenter() ) > Square( 500.0f ) )
			continue;

		backupHints.AddToTail( pHint );
	}

	if ( backupHints.IsEmpty() )
		return false;

	pSelected = backupHints.Random();

	Vector vecMins, vecMaxs;
	pSelected->GetCollideable()->WorldSpaceSurroundingBounds( &vecMins, &vecMaxs );

	Vector vecRandomBounds;
	vecRandomBounds.x = RandomFloat( vecMins.x, vecMaxs.x );
	vecRandomBounds.y = RandomFloat( vecMins.y, vecMaxs.y );
	vecRandomBounds.z = ( vecMaxs.z + vecMins.z ) / 2.0f;

	TheNavMesh->GetSimpleGroundHeight( vecRandomBounds, &vecRandomBounds.z );

	m_bHasHome = true;
	m_vecHome = vecRandomBounds;
	m_hHint = pSelected;
	pSelected->SetOwnerEntity( actor );

	return true;
}

bool CTFBotSniperLurk::FindNewHome( CTFBot *actor )
{
	if ( !m_findHomeTimer.IsElapsed() )
		return false;

	m_findHomeTimer.Start( RandomFloat( 1.0f, 2.0f ) );

	if (actor->m_sniperSpots.IsEmpty())
	{
		m_bHasHome = false;

		CUtlVector<CNavArea*> areas;

		if (areas.IsEmpty())
		{

			FOR_EACH_VEC(TheNavAreas, it)
			{
				CNavArea* area = TheNavAreas[it];
				areas.AddToHead(area);
			}
			CNavArea* area = areas.Random();
			m_vecHome = area->GetRandomPoint();
			m_bHasHome = true;
			return true;
		}
		else
		{
			CNavArea* area = areas.Random();
			m_vecHome = area->GetRandomPoint();
			m_bHasHome = true;
			return true;
		}

		return false;
	}

	return false;
}