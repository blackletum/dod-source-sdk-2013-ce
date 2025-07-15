#include "cbase.h"
#include "../../../tf_bot.h"
#include "dod/dod_gamerules.h"
#include "tf/nav_mesh/tf_nav_mesh.h"
#include "tf_bot_capture_point.h"
#include "tf_bot_defend_point.h"
#include "../../tf_bot_seek_and_destroy.h""
#include "tf/bot/behavior/tf_bot_melee_attack.h"


ConVar doc_bot_offense_must_push_time( "doc_bot_offense_must_push_time", "120", FCVAR_CHEAT, "If timer is less than this, bots will push hard to cap" );
ConVar doc_bot_capture_seek_and_destroy_min_duration( "doc_bot_capture_seek_and_destroy_min_duration", "15", FCVAR_CHEAT, "If a capturing bot decides to go hunting, this is the min duration he will hunt for before reconsidering" );
ConVar doc_bot_capture_seek_and_destroy_max_duration( "doc_bot_capture_seek_and_destroy_max_duration", "30", FCVAR_CHEAT, "If a capturing bot decides to go hunting, this is the max duration he will hunt for before reconsidering" );


CTFBotCapturePoint::CTFBotCapturePoint()
{
}

CTFBotCapturePoint::~CTFBotCapturePoint()
{
}


const char *CTFBotCapturePoint::GetName() const
{
	return "CapturePoint";
}


ActionResult<CTFBot> CTFBotCapturePoint::OnStart( CTFBot *me, Action<CTFBot> *priorAction )
{
	VPROF_BUDGET( "CTFBotCapturePoint::OnStart", "NextBot" );

	m_PathFollower.SetMinLookAheadDistance( me->GetDesiredPathLookAheadRange() );
	m_PathFollower.Invalidate();

	return Action<CTFBot>::Continue();
}

ActionResult<CTFBot> CTFBotCapturePoint::Update( CTFBot *me, float dt )
{
	CControlPoint *pPoint = me->GetMyControlPoint();
	if ( pPoint == nullptr )
		return Action<CTFBot>::SuspendFor( new CTFBotSeekAndDestroy( 10.0f ), "Seek and destroy until a point becomes available" );

	if ( pPoint->GetTeamNumber() == me->GetTeamNumber() )
		return Action<CTFBot>::ChangeTo( new CTFBotDefendPoint, "We need to defend our point(s)" );

	const CKnownEntity *threat = me->GetVisionInterface()->GetPrimaryKnownThreat();
	me->EquipBestWeaponForThreat( threat );

	if (threat != nullptr && threat->GetEntity()->IsAlive() && me->GetIntentionInterface()->ShouldAttack(me, threat))
	{
		if (threat->IsVisibleInFOVNow())
		{
			if (threat->GetLastKnownPosition().DistToSqr(me->GetAbsOrigin()) < Square(200.0f))
				return Action<CTFBot>::SuspendFor(new CTFBotMeleeAttack(1.25f * 200.0f), "Melee attacking nearby threat");
		}
	}
	if ( ( !me->IsPointBeingContested( pPoint ) || me->GetTimeSinceWeaponFired() < 2.0f ) &&
		 !me->IsCapturingPoint() &&
		 me->GetTimeLeftToCapture() >= doc_bot_offense_must_push_time.GetFloat() &&
		 !me->IsNearPoint( pPoint ) &&
		 threat && threat->IsVisibleRecently() )
	{
		float duration = RandomFloat( doc_bot_capture_seek_and_destroy_min_duration.GetFloat(),
									  doc_bot_capture_seek_and_destroy_max_duration.GetFloat() );

		return Action<CTFBot>::SuspendFor( new CTFBotSeekAndDestroy( duration ), "Too early to capture - hunting" );
	}

	if ( me->IsCapturingPoint() )
	{
		const CUtlVector<CTFNavArea *> &cpAreas = TFNavMesh()->GetControlPointAreas( pPoint->GetPointIndex() );
		if( cpAreas.IsEmpty() )
			return Action<CTFBot>::Continue();

		if ( !m_recomputePathTimer.IsElapsed() )
		{
			m_PathFollower.Update( me );
			return Action<CTFBot>::Continue();
		}

		m_recomputePathTimer.Start( RandomFloat( 0.5f, 1.0f ) );

		float flTotalArea = 0.0f;
		for ( int i=0; i<cpAreas.Count(); ++i )
		{
			CNavArea *area = cpAreas[i];
			flTotalArea += area->GetSizeX() * area->GetSizeY();
		}

		float flRand = RandomFloat( 0, flTotalArea - 1.0f );
		for ( int i=0; i<cpAreas.Count(); ++i )
		{
			CNavArea *area = cpAreas[i];
			if ( flRand - ( area->GetSizeX() * area->GetSizeY() ) <= 0.0f )
			{
				CTFBotPathCost cost( me );
				m_PathFollower.Compute( me, area->GetRandomPoint(), cost );

				return Action<CTFBot>::Continue();
			}
		}
	}

	if ( m_recomputePathTimer.IsElapsed() )
	{
		VPROF_BUDGET( "CTFBotCapturePoint::Update( repath )", "NextBot" );

		CTFBotPathCost cost( me, SAFEST_ROUTE );
		m_PathFollower.Compute( me, pPoint->GetAbsOrigin(), cost, 0.0f, true );

		m_recomputePathTimer.Start( RandomFloat( 2.0f, 3.0f ) );
	}

	m_PathFollower.Update(me);

	return Action<CTFBot>::Continue();
}

ActionResult<CTFBot> CTFBotCapturePoint::OnResume( CTFBot *me, Action<CTFBot> *priorAction )
{
	m_recomputePathTimer.Invalidate();

	return Action<CTFBot>::Continue();
}


EventDesiredResult<CTFBot> CTFBotCapturePoint::OnMoveToSuccess( CTFBot *me, const Path *path )
{
	return Action<CTFBot>::TryContinue();
}

EventDesiredResult<CTFBot> CTFBotCapturePoint::OnMoveToFailure( CTFBot *me, const Path *path, MoveToFailureType fail )
{
	m_recomputePathTimer.Invalidate();

	return Action<CTFBot>::TryContinue();
}

EventDesiredResult<CTFBot> CTFBotCapturePoint::OnStuck( CTFBot *me )
{
	m_recomputePathTimer.Invalidate();
	me->GetLocomotionInterface()->ClearStuckStatus();

	return Action<CTFBot>::TryContinue();
}

EventDesiredResult<CTFBot> CTFBotCapturePoint::OnTerritoryContested( CTFBot *me, int territoryID )
{
	return Action<CTFBot>::TryContinue();
}

EventDesiredResult<CTFBot> CTFBotCapturePoint::OnTerritoryCaptured( CTFBot *me, int territoryID )
{
	m_recomputePathTimer.Invalidate();

	return Action<CTFBot>::TryContinue();
}

EventDesiredResult<CTFBot> CTFBotCapturePoint::OnTerritoryLost( CTFBot *me, int territoryID )
{
	return Action<CTFBot>::TryContinue();
}


QueryResultType CTFBotCapturePoint::ShouldHurry( const INextBot *me ) const
{
	CTFBot *actor = static_cast<CTFBot *>( me->GetEntity() );

	if ( actor->GetTimeLeftToCapture() < doc_bot_offense_must_push_time.GetFloat() )
	{
		return ANSWER_YES;
	}

	return ANSWER_UNDEFINED;
}

QueryResultType CTFBotCapturePoint::ShouldRetreat( const INextBot *me ) const
{
	CTFBot *actor = static_cast<CTFBot *>( me->GetEntity() );

	if ( actor->GetTimeLeftToCapture() < doc_bot_offense_must_push_time.GetFloat() )
	{
		return ANSWER_NO;
	}

	return ANSWER_UNDEFINED;
}
