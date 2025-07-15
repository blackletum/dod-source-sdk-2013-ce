#include "cbase.h"
#include "../tf_bot.h"
#include "tf_bot_seek_and_destroy.h"
#include "tf_bot_attack.h"
#include "tf/nav_mesh/tf_nav_mesh.h"
#include "dod/dod_gamerules.h"
#include "tf_bot_melee_attack.h"


ConVar doc_bot_debug_seek_and_destroy( "doc_bot_debug_seek_and_destroy", "0", FCVAR_CHEAT, "", true, 0.0f, true, 1.0f );


CTFBotSeekAndDestroy::CTFBotSeekAndDestroy( float duration )
{
	if ( duration > 0.0f )
	{
		m_actionDuration.Start( duration );
	}
}

CTFBotSeekAndDestroy::~CTFBotSeekAndDestroy()
{
}


const char *CTFBotSeekAndDestroy::GetName() const
{
	return "SeekAndDestroy";
}


ActionResult<CTFBot> CTFBotSeekAndDestroy::OnStart( CTFBot *me, Action<CTFBot> *priorAction )
{
	m_PathFollower.SetMinLookAheadDistance( me->GetDesiredPathLookAheadRange() );

	RecomputeSeekPath( me );

	/* start the countdown timer back to the beginning */
	if ( m_actionDuration.HasStarted() )
		m_actionDuration.Reset();

	return Action<CTFBot>::Continue();
}

ActionResult<CTFBot> CTFBotSeekAndDestroy::Update( CTFBot *me, float dt )
{
	if ( me->IsCapturingPoint() )
		return Action<CTFBot>::Done( "Keep capturing point I happened to stumble upon" );

	/*extern ConVar doc_bot_offense_must_push_time;
	if ( TFGameRules()->State_Get() != GR_STATE_TEAM_WIN &&
		 me->GetTimeLeftToCapture() < doc_bot_offense_must_push_time.GetFloat() )
	{
		return Action<CTFBot>::Done( "Time to push for the objective" );
	}*/

	const CKnownEntity *threat = me->GetVisionInterface()->GetPrimaryKnownThreat();
	if ( threat != nullptr )
	{
		if (threat->IsVisibleInFOVNow())
		{
			if (threat->GetLastKnownPosition().DistToSqr(me->GetAbsOrigin()) < Square(200.0f))
				return Action<CTFBot>::SuspendFor(new CTFBotMeleeAttack(1.25f * 200.0f), "Melee attacking nearby threat");
		}
		if ( me->IsRangeLessThan( threat->GetLastKnownPosition(), 1000.0f ) )
		{
			return Action<CTFBot>::SuspendFor( new CTFBotAttack(), "Going after an enemy" );
		}
	}

	if ( m_actionDuration.HasStarted() && m_actionDuration.IsElapsed() )
		return Action<CTFBot>::Done( "Behavior duration elapsed" );

	m_PathFollower.Update( me );

	if ( !m_PathFollower.IsValid() && m_recomputeTimer.IsElapsed() )
	{
		m_recomputeTimer.Start( 1.0f );
		RecomputeSeekPath( me );
	}

	return Action<CTFBot>::Continue();
}

ActionResult<CTFBot> CTFBotSeekAndDestroy::OnResume( CTFBot *me, Action<CTFBot> *interruptingAction )
{
	RecomputeSeekPath( me );

	return Action<CTFBot>::Continue();
}


EventDesiredResult<CTFBot> CTFBotSeekAndDestroy::OnMoveToSuccess( CTFBot *me, const Path *path )
{
	RecomputeSeekPath( me );

	return Action<CTFBot>::TryContinue();
}

EventDesiredResult<CTFBot> CTFBotSeekAndDestroy::OnMoveToFailure( CTFBot *me, const Path *path, MoveToFailureType reason )
{
	RecomputeSeekPath( me );

	return Action<CTFBot>::TryContinue();
}

EventDesiredResult<CTFBot> CTFBotSeekAndDestroy::OnStuck( CTFBot *me )
{
	RecomputeSeekPath( me );

	return Action<CTFBot>::TryContinue();
}

EventDesiredResult<CTFBot> CTFBotSeekAndDestroy::OnTerritoryContested( CTFBot *me, int pointID )
{
	return Action<CTFBot>::TryDone( RESULT_IMPORTANT, "Defending the point" );
}

EventDesiredResult<CTFBot> CTFBotSeekAndDestroy::OnTerritoryCaptured( CTFBot *me, int pointID )
{
	return Action<CTFBot>::TryDone( RESULT_IMPORTANT, "Giving up due to point capture" );
}

EventDesiredResult<CTFBot> CTFBotSeekAndDestroy::OnTerritoryLost( CTFBot *me, int pointID )
{
	return Action<CTFBot>::TryDone( RESULT_IMPORTANT, "Giving up due to point lost" );
}


QueryResultType CTFBotSeekAndDestroy::ShouldHurry( const INextBot *me ) const
{
	return ANSWER_UNDEFINED;
}

QueryResultType CTFBotSeekAndDestroy::ShouldRetreat( const INextBot *me ) const
{
	return ANSWER_UNDEFINED;
}


CNavArea *CTFBotSeekAndDestroy::ChooseGoalArea( CTFBot *actor )
{
	CUtlVector<CNavArea*> areas;
	CUtlVector< CTFNavArea* > goalVector;

	TheTFNavMesh()->CollectSpawnRoomThresholdAreas(&goalVector, GetEnemyTeam(actor));

	CControlPoint* point = actor->GetMyControlPoint();
	if (point)
	{
		// add current control point as a seek goal
		const CUtlVector< CTFNavArea* >* controlPointAreas = TheTFNavMesh()->GetControlPointAreas2(point->GetPointIndex());
		if (controlPointAreas && controlPointAreas->Count() > 0)
		{
			goalVector.AddToTail(controlPointAreas->Element(RandomInt(0, controlPointAreas->Count() - 1)));
		}
	}
	else {

		FOR_EACH_VEC(TheNavAreas, it)
		{
			CNavArea* area = TheNavAreas[it];
			areas.AddToHead(area);
		}

	}

	if (doc_bot_debug_seek_and_destroy.GetBool() || !point)
	{
		for (int i = 0; i < goalVector.Count(); ++i)
		{
			TheNavMesh->AddToSelectedSet(goalVector[i]);
		}
	}

	// pick a new goal
	if (goalVector.Count() > 0)
	{
		return goalVector[RandomInt(0, goalVector.Count() - 1)];
	}

	return NULL;
}

void CTFBotSeekAndDestroy::RecomputeSeekPath( CTFBot *actor )
{
	if ( ( m_GoalArea = ChooseGoalArea( actor ) ) == nullptr )
	{
		m_PathFollower.Invalidate();
		return;
	}

	CTFBotPathCost func( actor, SAFEST_ROUTE );
	m_PathFollower.Compute( actor, m_GoalArea->GetCenter(), func );
}
