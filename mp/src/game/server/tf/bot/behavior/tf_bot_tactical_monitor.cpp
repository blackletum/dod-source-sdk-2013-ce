#include "cbase.h"
#include "NextBot/NavMeshEntities/func_nav_prerequisite.h"
#include "../tf_bot.h"
#include "dod/weapon_dodbase.h"
#include "tf_bot_tactical_monitor.h"
#include "dod/dod_gamerules.h"
#include "tf/nav_mesh/tf_nav_mesh.h"
#include "tf_bot_scenario_monitor.h"
#include "nav_entities/tf_bot_nav_ent_wait.h"
#include "nav_entities/tf_bot_nav_ent_move_to.h"
#include "tf_bot_taunt.h"
#include "tf_bot_seek_and_destroy.h"
#include "tf_bot_get_ammo.h"
#include "tf_bot_get_health.h"
#include "tf_bot_retreat_to_cover.h"
#include "tf_bot_use_teleporter.h"


ConVar tf_bot_force_jump( "tf_bot_force_jump", "0", FCVAR_CHEAT, "Force bots to continuously jump", true, 0.0f, true, 1.0f );

class DetonatePipebombsReply : public INextBotReply
{
	virtual void OnSuccess( INextBot *bot )
	{
		CTFBot *actor = ToTFBot( bot->GetEntity() );
		if ( actor->GetActiveWeapon() != actor->Weapon_GetSlot( 1 ) )
			actor->Weapon_Switch( actor->Weapon_GetSlot( 1 ) );

		actor->PressAltFireButton();
	}
};
static DetonatePipebombsReply detReply;

const char *CTFBotTacticalMonitor::GetName( void ) const
{
	return "TacticalMonitor";
}


ActionResult<CTFBot> CTFBotTacticalMonitor::OnStart( CTFBot *me, Action<CTFBot> *priorAction )
{
	return Action<CTFBot>::Continue();
}

ActionResult<CTFBot> CTFBotTacticalMonitor::Update( CTFBot *me, float dt )
{
	if ( tf_bot_force_jump.GetBool() && !me->GetLocomotionInterface()->IsClimbingOrJumping() )
		me->GetLocomotionInterface()->Jump();

	QueryResultType retreat = me->GetIntentionInterface()->ShouldRetreat( me );
	if ( retreat == ANSWER_YES )
		return BaseClass::SuspendFor( new CTFBotRetreatToCover, "Backing off" );

	if ( retreat != ANSWER_NO && me->m_iSkill >= CTFBot::HARD )
	{
		CWeaponDODBase *pWeapon = (CWeaponDODBase*)me->Weapon_GetSlot( 1 );
		if ( pWeapon && me->IsBarrageAndReloadWeapon( pWeapon ) )
		{
			if ( pWeapon->Clip1() <= 1 )
				return BaseClass::SuspendFor( new CTFBotRetreatToCover, "Moving to cover to reload" );
		}
	}

		m_checkUseTeleportTimer.Start( RandomFloat( 0.3f, 0.5f ) );

		bool bLowHealth = false;

		if ( ( me->GetTimeSinceWeaponFired() < 2.0f ) || (float)me->GetHealth() / (float)me->GetMaxHealth() < 0.2f )
		{
			bLowHealth = true;
		}

		if ( me->IsAmmoLow() && CTFBotGetAmmo::IsPossible( me ) )
			return Action<CTFBot>::SuspendFor( new CTFBotGetAmmo, "Grabbing nearby ammo" );

	MonitorArmedStickybombs( me );

	me->UpdateDelayedThreatNotices();

	return Action<CTFBot>::Continue();
}


Action<CTFBot> *CTFBotTacticalMonitor::InitialContainedAction( CTFBot *actor )
{
	return new CTFBotScenarioMonitor;
}


EventDesiredResult<CTFBot> CTFBotTacticalMonitor::OnOtherKilled( CTFBot *me, CBaseCombatCharacter *who, const CTakeDamageInfo& info )
{
	return Action<CTFBot>::TryContinue();
}

EventDesiredResult<CTFBot> CTFBotTacticalMonitor::OnNavAreaChanged( CTFBot *me, CNavArea *area1, CNavArea *area2 )
{
	if ( area1 == nullptr )
	{
		return Action<CTFBot>::TryContinue();
	}

	/*FOR_EACH_VEC( area1->GetPrerequisiteVector(), i )
	{
		CFuncNavPrerequisite *pPrereq = area1->GetPrerequisiteVector()[i];
		if ( pPrereq == nullptr )
			continue;

		if ( pPrereq->IsTask( CFuncNavPrerequisite::TASK_WAIT ) )
			return Action<CTFBot>::TrySuspendFor( new CTFBotNavEntWait( pPrereq ), RESULT_IMPORTANT, "Prerequisite commands me to wait" );

		if ( pPrereq->IsTask( CFuncNavPrerequisite::TASK_MOVE_TO_ENTITY ) )
			return Action<CTFBot>::TrySuspendFor( new CTFBotNavEntMoveTo( pPrereq ), RESULT_IMPORTANT, "Prerequisite commands me to move to an entity" );
	}*/

	return Action<CTFBot>::TryContinue();
}

// This seems almost entirely used for the "tutorial"
EventDesiredResult<CTFBot> CTFBotTacticalMonitor::OnCommandString( CTFBot *me, const char *cmd )
{
	if ( FStrEq( cmd, "goto action point" ) )
	{
		//return Action<CTFBot>::TrySuspendFor( new CTFGotoActionPoint, RESULT_IMPORTANT, "Received command to go to action point" );
	}
	else if ( FStrEq( cmd, "despawn" ) )
	{
		//return Action<CTFBot>::TrySuspendFor( new CTFDespawn, RESULT_CRITICAL, "Received command to go to de-spawn" );
	}
	else if ( FStrEq( cmd, "build sentry at nearest sentry hint" ) )
	{
// TODO
	}
	else if ( FStrEq( cmd, "attack sentry at next action point" ) )
	{
// TODO
	}

	return Action<CTFBot>::TryContinue();
}


void CTFBotTacticalMonitor::AvoidBumpingEnemies( CTFBot *actor )
{
	if ( actor->m_iSkill > CTFBot::NORMAL )
	{
		CDODPlayer *pClosest = nullptr;
		float flClosest = Square( 200.0f );

		CUtlVector<CDODPlayer *> enemies;
		CollectPlayers( &enemies, GetEnemyTeam( actor ), true );
		for ( int i=0; i<enemies.Count(); ++i )
		{
			CDODPlayer *pPlayer = enemies[i];

			float flDistance = ( pPlayer->GetAbsOrigin() - actor->GetAbsOrigin() ).LengthSqr();
			if ( flDistance < flClosest )
			{
				flClosest = flDistance;
				pClosest = pPlayer;
			}
		}

		if ( pClosest )
		{
			if ( actor->GetIntentionInterface()->IsHindrance( actor, pClosest ) == ANSWER_UNDEFINED )
			{
				actor->ReleaseForwardButton();
				actor->ReleaseLeftButton();
				actor->ReleaseRightButton();
				actor->ReleaseBackwardButton();

				actor->GetLocomotionInterface()->Approach(
					actor->GetAbsOrigin() + actor->GetLocomotionInterface()->GetFeet() - pClosest->GetAbsOrigin()
				);
			}
		}
	}
}

CObjectTeleporter *CTFBotTacticalMonitor::FindNearbyTeleporter( CTFBot *actor )
{
	if ( !m_takeTeleporterTimer.IsElapsed() )
		return nullptr;

	m_takeTeleporterTimer.Start( RandomFloat( 1.0f, 2.0f ) );

	if ( !actor->GetLastKnownArea() )
		return nullptr;

	CUtlVector<CNavArea *> nearby;
	CollectSurroundingAreas( &nearby, actor->GetLastKnownArea(), 1000.0f );

	CUtlVector<CBaseObject *> objects;
	TFNavMesh()->CollectBuiltObjects( &objects, actor->GetTeamNumber() );

	return nullptr;
}

void CTFBotTacticalMonitor::MonitorArmedStickybombs( CTFBot *actor )
{
	if ( !m_stickyMonitorDelay.IsElapsed() )
		return;

	m_stickyMonitorDelay.Start( RandomFloat( 0.3f, 1.0f ) );
	
	CUtlVector<CKnownEntity> knowns;
	actor->GetVisionInterface()->CollectKnownEntities( &knowns );
}

bool CTFBotTacticalMonitor::ShouldOpportunisticallyTeleport( CTFBot *actor ) const
{
	return false;
}
