
//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================
#include "cbase.h"
#include "dod_viewmodel.h"

#ifdef CLIENT_DLL
#include "c_dod_player.h"
#include "prediction.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( dod_viewmodel, CDODViewModel );

IMPLEMENT_NETWORKCLASS_ALIASED( DODViewModel, DT_DODViewModel )

BEGIN_NETWORK_TABLE( CDODViewModel, DT_DODViewModel )
END_NETWORK_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
#ifdef CLIENT_DLL
CDODViewModel::CDODViewModel() : m_LagAnglesHistory("CDODViewModel::m_LagAnglesHistory")
{
	m_vLagAngles.Init();
	m_LagAnglesHistory.Setup( &m_vLagAngles, 0 );
	m_vLoweredWeaponOffset.Init();
}
#else
CDODViewModel::CDODViewModel()
{
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CDODViewModel::~CDODViewModel()
{
}

#ifdef CLIENT_DLL
ConVar cl_wpn_sway_interp( "cl_wpn_sway_interp", "0.1", FCVAR_CLIENTDLL );
ConVar cl_wpn_sway_scale( "cl_wpn_sway_scale", "2.6", FCVAR_CLIENTDLL );
#endif
extern float g_fMaxViewModelLag;
void CDODViewModel::CalcViewModelLag( Vector& origin, QAngle& angles, QAngle& original_angles )
{
#ifdef CLIENT_DLL
	if ( prediction->InPrediction() )
	{
		return;
	}

	float flSwayScale = cl_wpn_sway_scale.GetFloat();

	CWeaponDODBase *pWeapon = dynamic_cast<CWeaponDODBase*>(GetWeapon());

	if ( pWeapon )
	{
		flSwayScale *= pWeapon->GetViewModelSwayScale();
	}

	// Calculate our drift
	Vector	forward, right, up;
	AngleVectors( angles, &forward, &right, &up );

	// Add an entry to the history.
	m_vLagAngles = angles;
	m_LagAnglesHistory.NoteChanged( gpGlobals->curtime, cl_wpn_sway_interp.GetFloat(), false );

	// Interpolate back 100ms.
	m_LagAnglesHistory.Interpolate( gpGlobals->curtime, cl_wpn_sway_interp.GetFloat() );

	// Now take the 100ms angle difference and figure out how far the forward vector moved in local space.
	Vector vLaggedForward;
	QAngle angleDiff = m_vLagAngles - angles;
	AngleVectors( -angleDiff, &vLaggedForward, 0, 0 );
	Vector vForwardDiff = Vector(1,0,0) - vLaggedForward;

	// Now offset the origin using that.
	vForwardDiff *= flSwayScale;
	origin += forward*vForwardDiff.x + right*-vForwardDiff.y + up*vForwardDiff.z;

	Vector vOriginalOrigin = origin;
	QAngle vOriginalAngles = angles;


	if (gpGlobals->frametime != 0.0f)
	{
		Vector vDifference;
		VectorSubtract(forward, m_vecLastFacing, vDifference);

		float flSpeed = 5.0f;

		// If we start to lag too far behind, we'll increase the "catch up" speed.  Solves the problem with fast cl_yawspeed, m_yaw or joysticks
		//  rotating quickly.  The old code would slam lastfacing with origin causing the viewmodel to pop to a new position
		float flDiff = vDifference.Length();
		if ((flDiff > g_fMaxViewModelLag) && (g_fMaxViewModelLag > 0.0f))
		{
			float flScale = flDiff / g_fMaxViewModelLag;
			flSpeed *= flScale;
		}

		// FIXME:  Needs to be predictable?
		VectorMA(m_vecLastFacing, flSpeed * gpGlobals->frametime, vDifference, m_vecLastFacing);
		// Make sure it doesn't grow out of control!!!
		VectorNormalize(m_vecLastFacing);
		VectorMA(origin, 5.0f, vDifference * -1.0f, origin);

		Assert(m_vecLastFacing.IsValid());
	}

	AngleVectors(original_angles, &forward, &right, &up);

	float pitch = original_angles[PITCH];
	if (pitch > 180.0f)
		pitch -= 360.0f;
	else if (pitch < -180.0f)
		pitch += 360.0f;

	if (g_fMaxViewModelLag == 0.0f)
	{
		origin = vOriginalOrigin;
		angles = vOriginalAngles;
	}

	//FIXME: These are the old settings that caused too many exposed polys on some models
	VectorMA(origin, -pitch * 0.035f, forward, origin);
	VectorMA(origin, -pitch * 0.03f, right, origin);
	VectorMA(origin, -pitch * 0.02f, up, origin);
#endif
}

#ifdef CLIENT_DLL
ConVar cl_gunlowerangle( "cl_gunlowerangle", "30", FCVAR_CLIENTDLL );
ConVar cl_gunlowerspeed( "cl_gunlowerspeed", "2", FCVAR_CLIENTDLL );

ConVar cl_test_vm_offset( "cl_test_vm_offset", "0 0 0", FCVAR_CHEAT | FCVAR_CLIENTDLL );
#endif

void CDODViewModel::CalcViewModelView( CBasePlayer *owner, const Vector& eyePosition, const QAngle& eyeAngles )
{
#if defined( CLIENT_DLL )

	Vector vecNewOrigin = eyePosition;
	QAngle vecNewAngles = eyeAngles;

	// Check for lowering the weapon
	C_DODPlayer *pPlayer = ToDODPlayer( owner );

	Assert( pPlayer );

	bool bLowered = pPlayer->IsWeaponLowered();

	QAngle vecLoweredAngles(0,0,0);

	m_vLoweredWeaponOffset.x = Approach( bLowered ? cl_gunlowerangle.GetFloat() : 0, m_vLoweredWeaponOffset.x, cl_gunlowerspeed.GetFloat() );
	vecLoweredAngles.x += m_vLoweredWeaponOffset.x;

	vecNewAngles += vecLoweredAngles;

	Vector	forward, right, up;
	AngleVectors( vecNewAngles, &forward, &right, &up );

	Vector test;
	const char *szTestOffset = cl_test_vm_offset.GetString();
	sscanf( szTestOffset, " %f %f %f", &test[0], &test[1], &test[2] );

	// cvar cl_test_vm_offset overrides calculated view model offset
	if ( test.Length() > 0 )
	{
		vecNewOrigin += forward * test[0] + right * test[1] + up * test[2];
	}
	else
	{
		// Move the view model origin between the script standing and prone position
		// based on the current view height
		CWeaponDODBase *pWeapon = dynamic_cast<CWeaponDODBase*>(GetWeapon());

		if ( pWeapon )
		{
			Vector offset = pWeapon->GetDesiredViewModelOffset( pPlayer );

			// add our offset in the proper direction
			vecNewOrigin += forward * offset[0] + right * offset[1] + up * offset[2];
		}
	}

	BaseClass::CalcViewModelView( owner, vecNewOrigin, vecNewAngles );

#endif
}