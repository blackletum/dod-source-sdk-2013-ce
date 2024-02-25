#include "cbase.h"
#include "../tf_bot.h"
#include "../tf_bot_squad.h"
#include "../tf_bot_manager.h"
#include "tf_bot_scenario_monitor.h"
#include "tf_bot_seek_and_destroy.h"
#include "tf_bot_roam.h"
#include "medic/tf_bot_medic_heal.h"
#include "spy/tf_bot_spy_infiltrate.h"
#include "sniper/tf_bot_sniper_lurk.h"
#include "engineer/tf_bot_engineer_build.h"
#include "scenario/capture_the_flag/tf_bot_fetch_flag.h"
#include "scenario/capture_the_flag/tf_bot_deliver_flag.h"
#include "scenario/capture_point/tf_bot_capture_point.h"
#include "scenario/capture_point/tf_bot_defend_point.h"


ConVar tf_bot_fetch_lost_flag_time( "tf_bot_fetch_lost_flag_time", "10", FCVAR_CHEAT, "How long busy TFBots will ignore the dropped flag before they give up what they are doing and go after it" );
ConVar tf_bot_flag_kill_on_touch( "tf_bot_flag_kill_on_touch", "0", FCVAR_CHEAT, "If nonzero, any bot that picks up the flag dies. For testing." );


const char *CTFBotScenarioMonitor::GetName( void ) const
{
	return "ScenarioMonitor";
}


ActionResult<CTFBot> CTFBotScenarioMonitor::OnStart( CTFBot *me, Action<CTFBot> *priorAction )
{
	m_fetchFlagDelay.Start( 20.0f );
	m_fetchFlagDuration.Invalidate();

	return BaseClass::Continue();
}

ActionResult<CTFBot> CTFBotScenarioMonitor::Update( CTFBot *me, float dt )
{
	return BaseClass::Continue();
}


Action<CTFBot> *CTFBotScenarioMonitor::InitialContainedAction( CTFBot *actor )
{
	return DesiredScenarioAndClassAction( actor );
}


Action<CTFBot> *CTFBotScenarioMonitor::DesiredScenarioAndClassAction( CTFBot *actor )
{
	if ( TheNavAreas.IsEmpty() )
		return nullptr;

	return new CTFBotSeekAndDestroy( -1.0f );
}
