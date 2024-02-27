#include "cbase.h"
#include "../tf_bot.h"
#include "tf_bot_melee_attack.h"

ConVar tf_bot_melee_attack_abandon_range( "tf_bot_melee_attack_abandon_range", "500", FCVAR_CHEAT, "If threat is farther away than this, bot will switch back to its primary weapon and attack" );


CTFBotMeleeAttack::CTFBotMeleeAttack( float flAbandonRange )
{
	if ( flAbandonRange < 0.0f )
	{
		this->m_flAbandonRange = tf_bot_melee_attack_abandon_range.GetFloat();
	}
	else
	{
		this->m_flAbandonRange = flAbandonRange;
	}
}

CTFBotMeleeAttack::~CTFBotMeleeAttack()
{
}


const char *CTFBotMeleeAttack::GetName() const
{
	return "MeleeAttack";
}


ActionResult<CTFBot> CTFBotMeleeAttack::OnStart( CTFBot *me, Action<CTFBot> *priorAction )
{
	m_ChasePath.SetMinLookAheadDistance( me->GetDesiredPathLookAheadRange() );

	return Action<CTFBot>::Continue();
}

ActionResult<CTFBot> CTFBotMeleeAttack::Update( CTFBot *me, float dt )
{
	const CKnownEntity *threat = me->GetVisionInterface()->GetPrimaryKnownThreat();
	if ( threat == nullptr )
	{
		return Action<CTFBot>::Done( "No threat" );
	}
	bool bPrimaryExists = false;
	if ( ( threat->GetLastKnownPosition() - me->GetAbsOrigin() ).LengthSqr() > Square( m_flAbandonRange ) )
	{
		return Action<CTFBot>::Done( "Threat is too far away for a melee attack" );
	}
	CBaseCombatWeapon* primary = me->Weapon_GetSlot(WPN_SLOT_PRIMARY);

	if (primary != nullptr)
	{
		if (!V_strcmp(primary->GetClassname(), "weapon_thompson") || !V_strcmp(primary->GetClassname(), "weapon_mp40")) {

			bPrimaryExists = true;

		}
	}
	CBaseCombatWeapon *melee = me->Weapon_GetSlot( WPN_SLOT_MELEE );
	if ( melee != nullptr && !bPrimaryExists)
	{
		me->Weapon_Switch( melee );
	}
	if (!bPrimaryExists) {

		me->PressFireButton();

	}
	else {

		me->PressAltFireButton();

	}

	CTFBotPathCost cost( me, FASTEST_ROUTE );
	m_ChasePath.Update( me, threat->GetEntity(), cost );

	return Action<CTFBot>::Continue();
}
