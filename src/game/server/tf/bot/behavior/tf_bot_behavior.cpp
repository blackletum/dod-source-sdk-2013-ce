//========= Copyright © Valve LLC, All rights reserved. =======================
//
// Purpose:		
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "../tf_bot.h"
#include "team.h"
#include "dod/dod_gamerules.h"
#include "tf/nav_mesh/tf_nav_area.h"
#include "tf_bot_behavior.h"
#include "tf_bot_dead.h"
#include "tf_bot_tactical_monitor.h"
#include "tf_bot_taunt.h"
#include "../tf_bot_manager.h"


ConVar tf_bot_sniper_aim_error( "tf_bot_sniper_aim_error", "0.01", FCVAR_CHEAT );
ConVar tf_bot_sniper_aim_steady_rate( "tf_bot_sniper_aim_steady_rate", "10", FCVAR_CHEAT );
ConVar tf_bot_fire_weapon_min_time( "tf_bot_fire_weapon_min_time", "1", FCVAR_CHEAT );
ConVar tf_bot_taunt_victim_chance( "tf_bot_taunt_victim_chance", "20", FCVAR_NONE );
ConVar tf_bot_notice_backstab_chance( "tf_bot_notice_backstab_chance", "25", FCVAR_CHEAT );
ConVar tf_bot_notice_backstab_min_chance( "tf_bot_notice_backstab_min_chance", "100", FCVAR_CHEAT );
ConVar tf_bot_notice_backstab_max_chance( "tf_bot_notice_backstab_max_chance", "750", FCVAR_CHEAT );
ConVar tf_bot_arrow_elevation_rate( "tf_bot_arrow_elevation_rate", "0.0001", FCVAR_CHEAT, "When firing arrows at far away targets, this is the degree/range slope to raise our aim" );
ConVar tf_bot_ballistic_elevation_rate( "tf_bot_ballistic_elevation_rate", "0.01", FCVAR_CHEAT, "When lobbing grenades at far away targets, this is the degree/range slope to raise our aim" );
ConVar tf_bot_hitscan_range_limit( "tf_bot_hitscan_range_limit", "1800", FCVAR_CHEAT );
ConVar tf_bot_always_full_reload( "tf_bot_always_full_reload", "0", FCVAR_CHEAT );
ConVar tf_bot_fire_weapon_allowed( "tf_bot_fire_weapon_allowed", "1", FCVAR_CHEAT, "If zero, TFBots will not pull the trigger of their weapons (but will act like they did)", true, 0.0, true, 1.0 );


const char *CTFBotMainAction::GetName( void ) const
{
	return "MainAction";
}


Action<CTFBot> *CTFBotMainAction::InitialContainedAction( CTFBot *actor )
{
	return new CTFBotTacticalMonitor;
}


ActionResult<CTFBot> CTFBotMainAction::OnStart( CTFBot *me, Action<CTFBot> *priorAction )
{
	m_flSniperAimError1 = 0;
	m_flSniperAimError2 = 0;
	m_flYawDelta = 0;
	m_flPreviousYaw = 0;
	m_iDesiredDisguise = 0;
	m_bReloadingBarrage = false;

	return BaseClass::Continue();
}

ActionResult<CTFBot> CTFBotMainAction::Update( CTFBot *me, float dt )
{
	VPROF_BUDGET( __FUNCTION__, "NextBot" );

	if ( me->GetTeamNumber() != TEAM_ALLIES && me->GetTeamNumber() != TEAM_AXIS )
		return BaseClass::Done( "Not on a playing team" );

	me->GetVisionInterface()->SetFieldOfView( me->GetFOV() );

	m_flYawDelta = me->EyeAngles()[ YAW ] - ( m_flPreviousYaw * dt + FLT_EPSILON );
	m_flPreviousYaw = me->EyeAngles()[ YAW ];

	if ( tf_bot_sniper_aim_steady_rate.GetFloat() <= m_flYawDelta )
		m_sniperSteadyInterval.Invalidate();
	else if ( !m_sniperSteadyInterval.HasStarted() )
		m_sniperSteadyInterval.Start();

	me->EquipRequiredWeapon();
	me->UpdateLookingAroundForEnemies();
	FireWeaponAtEnemy( me );
	Dodge( me );

	return BaseClass::Continue();
}


EventDesiredResult<CTFBot> CTFBotMainAction::OnContact( CTFBot *me, CBaseEntity *other, CGameTrace *trace )
{
	// some miniboss crush logic for MvM happens
	return BaseClass::TryContinue();
}

EventDesiredResult<CTFBot> CTFBotMainAction::OnStuck( CTFBot *me )
{
	me->GetLocomotionInterface()->ClearStuckStatus();

	UTIL_LogPrintf( "\"%s<%i><%s><%s>\" stuck (position \"%3.2f %3.2f %3.2f\") (duration \"%3.2f\") ",
					me->GetPlayerName(), me->entindex(), me->GetNetworkIDString(), me->GetTeam()->GetName(),
					VecToString( me->GetAbsOrigin() ),
					me->GetLocomotionInterface()->GetStuckDuration() );
	if ( me->GetCurrentPath() && me->GetCurrentPath()->IsValid() )
	{
		UTIL_LogPrintf( "   path_goal ( \"%3.2f %3.2f %3.2f\" )\n",
						me->GetCurrentPath()->GetEndPosition() );
	}
	else
	{
		UTIL_LogPrintf( "   path_goal ( \"NULL\" )\n" );
	}

	me->GetLocomotionInterface()->Stop();

	return BaseClass::TryContinue();
}

EventDesiredResult<CTFBot> CTFBotMainAction::OnInjured( CTFBot *me, const CTakeDamageInfo &info )
{
	me->GetVisionInterface()->AddKnownEntity( info.GetInflictor() );

	if ( info.GetInflictor() && info.GetInflictor()->GetTeamNumber() != me->GetTeamNumber() )
	{
			// I get the DMG_CRITICAL, but DMG_BURN?
			if ( info.GetDamageType() & ( DMG_BURN ) )
			{
				if ( me->IsRangeLessThan( info.GetAttacker(), tf_bot_notice_backstab_max_chance.GetFloat() ) )
					me->DelayedThreatNotice( info.GetAttacker(), 0.5 );
			}
	}

	return BaseClass::TryContinue();
}

EventDesiredResult<CTFBot> CTFBotMainAction::OnKilled( CTFBot *me, const CTakeDamageInfo &info )
{
	return BaseClass::TryChangeTo( new CTFBotDead, RESULT_CRITICAL, "I died!" );
}

EventDesiredResult<CTFBot> CTFBotMainAction::OnOtherKilled( CTFBot *me, CBaseCombatCharacter *who, const CTakeDamageInfo &info )
{
	me->GetVisionInterface()->ForgetEntity( who );


	return Action<CTFBot>::TryContinue();
}


QueryResultType CTFBotMainAction::ShouldHurry( const INextBot *me ) const
{
	return ANSWER_UNDEFINED;
}

QueryResultType CTFBotMainAction::ShouldRetreat( const INextBot *me ) const
{
	if ( TheTFBots().IsMeleeOnly() )
		return ANSWER_YES;

	return ANSWER_UNDEFINED;
}

QueryResultType CTFBotMainAction::ShouldAttack( const INextBot *me, const CKnownEntity *threat ) const
{
	return ANSWER_YES; 
}

QueryResultType CTFBotMainAction::IsPositionAllowed( const INextBot *me, const Vector &pos ) const
{
	return ANSWER_YES;
}


Vector CTFBotMainAction::SelectTargetPoint( const INextBot *me, const CBaseCombatCharacter *them ) const
{
	VPROF_BUDGET( __FUNCTION__, "NextBot" );

	CTFBot *actor = ToTFBot( me->GetEntity() );
	CWeaponDODBaseGun *pWeapon = (CWeaponDODBaseGun*)actor->GetActiveDODWeapon();
	if ( !pWeapon )
		return them->WorldSpaceCenter();

	if ( actor->m_iSkill != CTFBot::EASY )
	{
		// Try to aim for their feet for best damage
		if ( pWeapon->GetWeaponID() == WEAPON_BAZOOKA || pWeapon->GetWeaponID() == WEAPON_PSCHRECK )
		{
			if ( them->GetAbsOrigin().z - 30.0f > actor->GetAbsOrigin().z )
			{
				if ( actor->GetVisionInterface()->IsAbleToSee( them->GetAbsOrigin(), IVision::DISREGARD_FOV ) )
					return them->GetAbsOrigin();

				if ( actor->GetVisionInterface()->IsAbleToSee( them->WorldSpaceCenter(), IVision::DISREGARD_FOV ) )
					return them->WorldSpaceCenter();
			}
			else
			{
				if ( !them->GetGroundEntity() )
				{
					trace_t trace;
					UTIL_TraceLine( them->GetAbsOrigin(), them->GetAbsOrigin() - Vector( 0, 0, 200.0f ), MASK_SOLID, them, COLLISION_GROUP_NONE, &trace );

					if ( trace.DidHit() )
						return trace.endpos;
				}

				float flDistance = actor->GetRangeTo( them->GetAbsOrigin() );
				if ( flDistance > 150.0f )
				{
					float flRangeMod = flDistance * 0.00090909092; // Investigate for constant
					Vector vecVelocity = them->GetAbsVelocity() * flRangeMod;

					if ( actor->GetVisionInterface()->IsAbleToSee( them->GetAbsOrigin() + vecVelocity, IVision::DISREGARD_FOV ) )
						return them->GetAbsOrigin() + vecVelocity;

					return them->EyePosition() + vecVelocity;
				}
			}

			return them->EyePosition();
		}
	}

	return them->WorldSpaceCenter();
}


const CKnownEntity *CTFBotMainAction::SelectMoreDangerousThreat( const INextBot *me, const CBaseCombatCharacter *them, const CKnownEntity *threat1, const CKnownEntity *threat2 ) const
{
	CTFBot *actor = static_cast<CTFBot *>( me->GetEntity() );

	const CKnownEntity *result = SelectMoreDangerousThreatInternal( me, them, threat1, threat2 );
	if ( actor->m_iSkill == CTFBot::EASY )
		return result;

	if ( actor->TransientlyConsistentRandomValue( 10.0f, 0 ) >= 0.5f || actor->m_iSkill >= CTFBot::HARD )
		return GetHealerOfThreat( result );

	return result;
}


void CTFBotMainAction::Dodge( CTFBot *actor )
{
	if ( actor->m_iSkill == CTFBot::EASY )
		return;

	if ( ( actor->m_nBotAttrs & CTFBot::AttributeType::DISABLEDODGE ) != 0 )
		return;

	const CKnownEntity *threat = actor->GetVisionInterface()->GetPrimaryKnownThreat( false );
	if ( threat == nullptr || !threat->IsVisibleRecently() )
		return;

	if (!actor->IsLineOfFireClear(threat->GetLastKnownPosition()))
	{
		return;
	}

	Vector vecFwd;
	actor->EyeVectors( &vecFwd );

	Vector2D vecRight( -vecFwd.y, vecFwd.x );
	vecRight.NormalizeInPlace();

	int random = RandomInt( 0, 100 );
	if ( random < 33 )
	{
		const Vector strafe_left = actor->GetAbsOrigin() + Vector( 25.0f * vecRight.x, 25.0f * vecRight.y, 0.0f );
		if ( !actor->GetLocomotionInterface()->HasPotentialGap( actor->GetAbsOrigin(), strafe_left ) )
		{
			actor->PressLeftButton();
		}
	}
	else if ( random > 66 )
	{
		const Vector strafe_right = actor->GetAbsOrigin() - Vector( 25.0f * vecRight.x, 25.0f * vecRight.y, 0.0f );
		if ( !actor->GetLocomotionInterface()->HasPotentialGap( actor->GetAbsOrigin(), strafe_right ) )
		{
			actor->PressRightButton();
		}
	}
}

void CTFBotMainAction::FireWeaponAtEnemy( CTFBot *actor )
{
	if ( !actor->IsAlive() )
		return;

	if ( ( actor->m_nBotAttrs & (CTFBot::AttributeType::SUPPRESSFIRE|CTFBot::AttributeType::IGNOREENEMIES) ) != 0 )
		return;

	if ( !tf_bot_fire_weapon_allowed.GetBool() )
		return;

	CWeaponDODBase *pWeapon = actor->GetActiveDODWeapon();
	if ( pWeapon == nullptr )
		return;

	if ( actor->IsBarrageAndReloadWeapon() && tf_bot_always_full_reload.GetBool() )
	{
		if ( pWeapon->Clip1() <= 0 )
		{
			m_bReloadingBarrage = true;
		}

		if ( m_bReloadingBarrage )
		{
			if ( pWeapon->Clip1() < pWeapon->GetMaxClip1() )
				return;

			m_bReloadingBarrage = false;
		}
	}

	if ( !actor->IsAmmoLow() )
	{
		if ( !actor->GetIntentionInterface()->ShouldHurry( actor ) )
		{
			if ( actor->GetVisionInterface()->GetTimeSinceVisible( GetEnemyTeam( actor ) ) < 3.0f )
			{
				actor->PressAltFireButton();
			}
		}
	}

	const CKnownEntity *threat = actor->GetVisionInterface()->GetPrimaryKnownThreat();
	if ( threat == nullptr || threat->GetEntity() == nullptr || !threat->IsVisibleRecently() )
		return;

	if ( !actor->IsLineOfFireClear( threat->GetEntity()->EyePosition() ) &&
		 !actor->IsLineOfFireClear( threat->GetEntity()->WorldSpaceCenter() ) &&
		 !actor->IsLineOfFireClear( threat->GetEntity()->GetAbsOrigin() ) )
	{
		return;
	}

	if ( !actor->GetIntentionInterface()->ShouldAttack( actor, threat ) )
		return;

	if ( !actor->GetBodyInterface()->IsHeadAimingOnTarget() )
		return;

	if ( pWeapon->IsMeleeWeapon() )
	{
		if ( actor->IsRangeLessThan( threat->GetEntity(), 250.0f ) )
			actor->PressFireButton();

		return;
	}

	const Vector vecToThreat = ( threat->GetEntity()->GetAbsOrigin() - actor->GetAbsOrigin() );
	float flDistToThreat = vecToThreat.Length();
	if ( flDistToThreat >= actor->GetMaxAttackRange() )
		return;

	if ( actor->IsContinuousFireWeapon() )
	{
		actor->PressFireButton( tf_bot_fire_weapon_min_time.GetFloat() );
		return;
	}

	if ( actor->IsExplosiveProjectileWeapon() )
	{
		Vector vecFwd;
		actor->EyeVectors( &vecFwd );
		vecFwd.NormalizeInPlace();

		vecFwd *= ( 1.1f * flDistToThreat );

		trace_t trace;
		UTIL_TraceLine( actor->EyePosition(),
						actor->EyePosition() + vecFwd,
						MASK_SHOT,
						actor,
						COLLISION_GROUP_NONE,
						&trace );

		if ( ( trace.fraction * ( 1.1f * flDistToThreat ) ) < 146.0f && ( trace.m_pEnt == nullptr || !trace.m_pEnt->IsCombatCharacter() ) )
			return;
	}

	actor->PressFireButton();
}

const CKnownEntity *CTFBotMainAction::GetHealerOfThreat( const CKnownEntity *threat ) const
{
	if ( threat == nullptr || threat->GetEntity() == nullptr )
		return nullptr;

	CDODPlayer *pPlayer = ToDODPlayer( threat->GetEntity() );
	if ( pPlayer == nullptr )
		return threat;

	const CKnownEntity *knownHealer = threat;

	return knownHealer;
}

bool CTFBotMainAction::IsImmediateThreat( const CBaseCombatCharacter *who, const CKnownEntity *threat ) const
{
	CTFBot *actor = GetActor();
	if ( actor == nullptr )
		return false;

	if ( !actor->IsSelf( who ) )
		return false;

	if ( actor->InSameTeam( threat->GetEntity() ) )
		return false;

	if ( !threat->GetEntity()->IsAlive() )
		return false;

	if ( !threat->IsVisibleRecently() )
		return false;

	if ( !actor->IsLineOfFireClear( threat->GetEntity() ) )
		return false;

	Vector vecToActor = actor->GetAbsOrigin() - threat->GetLastKnownPosition();
	float flDistance = vecToActor.Length();

	if ( flDistance < 500.0f )
		return true;

	if ( actor->IsThreatFiringAtMe( threat->GetEntity() ) )
		return true;

	vecToActor.NormalizeInPlace();

	Vector vecThreatFwd;

	return false;
}

const CKnownEntity *CTFBotMainAction::SelectCloserThreat( CTFBot *actor, const CKnownEntity *threat1, const CKnownEntity *threat2 ) const
{
	if ( actor->GetRangeSquaredTo( threat1->GetEntity() ) < actor->GetRangeSquaredTo( threat2->GetEntity() ) )
		return threat1;
	else
		return threat2;
}

const CKnownEntity *SelectClosestSpyToMe( CTFBot *actor, const CKnownEntity *known1, const CKnownEntity *known2 )
{
	return nullptr;
}

const CKnownEntity *CTFBotMainAction::SelectMoreDangerousThreatInternal( const INextBot *me, const CBaseCombatCharacter *them, const CKnownEntity *threat1, const CKnownEntity *threat2 ) const
{
	CTFBot *actor = static_cast<CTFBot *>( me->GetEntity() );

	const CKnownEntity *closer = this->SelectCloserThreat( actor, threat1, threat2 );

	return closer;
}
