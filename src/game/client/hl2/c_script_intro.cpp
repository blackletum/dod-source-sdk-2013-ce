//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "cbase.h"
#include "shareddefs.h"
#include "materialsystem/imesh.h"
#include "view.h"
#include "iviewrender.h"
#include "view_shared.h"
#include "viewrender.h"
#ifdef MAPBASE
#include "c_point_camera.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IntroData_t *g_pIntroData;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_ScriptIntro : public C_BaseEntity
{
	DECLARE_CLASS( C_ScriptIntro, C_BaseEntity );
public:
	DECLARE_CLIENTCLASS();

	C_ScriptIntro( void );
	~C_ScriptIntro( void );
	void	PostDataUpdate( DataUpdateType_t updateType );
	void	ClientThink( void );
	void	CalculateFOV( void );
	void	CalculateAlpha( void );

public:
	int		m_iNextFOV;
	int		m_iFOV;
	int		m_iPrevFOV;
	int		m_iStartFOV;
	float	m_flNextFOVBlendTime;
	float	m_flFOVBlendStartTime;
	bool	m_bAlternateFOV;

	// Our intro data block
	IntroData_t		m_IntroData;

private:
	Vector	m_vecCameraView;
	QAngle	m_vecCameraViewAngles;
	int		m_iBlendMode;
	int		m_iNextBlendMode;
	float	m_flNextBlendTime;
	float	m_flBlendStartTime;
	bool	m_bActive;
	EHANDLE	m_hCameraEntity;
#ifdef MAPBASE
	bool	m_bDrawSky;
	bool	m_bDrawSky2;
	bool	m_bUseEyePosition;
#endif

	// Fades
	float	m_flFadeColor[3];			// Server's desired fade color
	float	m_flFadeAlpha;				// Server's desired fade alpha
	float	m_flPrevServerFadeAlpha;	// Previous server's desired fade alpha
	float	m_flFadeDuration;			// Time it should take to reach the server's new fade alpha
	float	m_flFadeTimeStartedAt;		// Time at which we last recieved a new desired fade alpha
	float	m_flFadeAlphaStartedAt;		// Alpha at which we last received a new desired fade alpha
};

IMPLEMENT_CLIENTCLASS_DT( C_ScriptIntro, DT_ScriptIntro, CScriptIntro )
	RecvPropVector( RECVINFO( m_vecCameraView ) ),
	RecvPropVector( RECVINFO( m_vecCameraViewAngles ) ),
	RecvPropInt( RECVINFO( m_iBlendMode ) ),
	RecvPropInt( RECVINFO( m_iNextBlendMode ) ),
	RecvPropFloat( RECVINFO( m_flNextBlendTime ) ),
	RecvPropFloat( RECVINFO( m_flBlendStartTime ) ),
	RecvPropBool( RECVINFO( m_bActive ) ),
#ifdef MAPBASE
	RecvPropBool( RECVINFO( m_bDrawSky ) ),
	RecvPropBool( RECVINFO( m_bDrawSky2 ) ),
	RecvPropBool( RECVINFO( m_bUseEyePosition ) ),
#endif
	
	// Fov & fov blends 
	RecvPropInt( RECVINFO( m_iFOV ) ),
	RecvPropInt( RECVINFO( m_iNextFOV ) ),
	RecvPropInt( RECVINFO( m_iStartFOV ) ),
	RecvPropFloat( RECVINFO( m_flNextFOVBlendTime ) ),
	RecvPropFloat( RECVINFO( m_flFOVBlendStartTime ) ),
	RecvPropBool( RECVINFO( m_bAlternateFOV ) ),

	// Fades
	RecvPropFloat( RECVINFO( m_flFadeAlpha ) ),
	RecvPropArray( RecvPropFloat( RECVINFO( m_flFadeColor[0] ) ), m_flFadeColor ),
	RecvPropFloat( RECVINFO( m_flFadeDuration ) ),
	RecvPropEHandle(RECVINFO(m_hCameraEntity)),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ScriptIntro::C_ScriptIntro( void )
{
	m_bActive = false;
	m_vecCameraView = vec3_origin;
	m_vecCameraViewAngles = vec3_angle;
	m_iBlendMode = 0;
	m_iNextBlendMode = 0;
	m_flNextBlendTime = 0;
	m_flBlendStartTime = 0;
	m_IntroData.m_playerViewFOV = 0;
	m_flFadeAlpha = 0;
	m_flPrevServerFadeAlpha = 0;
	m_flFadeDuration = 0;
	m_flFadeTimeStartedAt = 0;
	m_flFadeAlphaStartedAt = 0;
	m_hCameraEntity = NULL;
	m_iPrevFOV = 0;
	m_iStartFOV = 0;

	g_pIntroData = NULL;

	// Setup fade colors
	m_IntroData.m_flCurrentFadeColor[0] = m_flFadeColor[0];
	m_IntroData.m_flCurrentFadeColor[1] = m_flFadeColor[1];
	m_IntroData.m_flCurrentFadeColor[2] = m_flFadeColor[2];
	m_IntroData.m_flCurrentFadeColor[3] = m_flFadeAlpha;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ScriptIntro::~C_ScriptIntro( void )
{
	g_pIntroData = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ScriptIntro::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate( updateType );

	SetNextClientThink( CLIENT_THINK_ALWAYS );

	// Fill out the intro data
	m_IntroData.m_vecCameraView = m_vecCameraView;
	m_IntroData.m_vecCameraViewAngles = m_vecCameraViewAngles;
	m_IntroData.m_Passes.SetCount( 0 );

#ifdef MAPBASE
	m_IntroData.m_bDrawSky = m_bDrawSky;
#endif

	// Find/Create our first pass
	IntroDataBlendPass_t *pass1;
	if ( m_IntroData.m_Passes.Count() == 0 )
	{
		pass1 = &m_IntroData.m_Passes[m_IntroData.m_Passes.AddToTail()];
	}
	else
	{
		pass1 = &m_IntroData.m_Passes[0];
	}
	Assert(pass1);
	pass1->m_BlendMode = m_iBlendMode;
	pass1->m_Alpha = 1.0f;

	if ( m_vecCameraView == vec3_origin )
	{
		m_IntroData.m_bDrawPrimary = false;
	}
	else
	{
		m_IntroData.m_bDrawPrimary = true;
#ifdef MAPBASE
		m_IntroData.m_bDrawSky2 = m_bDrawSky2;

		// If it's a point_camera and it's ortho, send it to the intro data
		// Change this code if the purpose of m_hCameraEntity in intro data ever goes beyond ortho
		if ( m_hCameraEntity && Q_strncmp(m_hCameraEntity->GetClassname(), "point_camera", 12) == 0 )
		{
			C_PointCamera *pCamera = dynamic_cast<C_PointCamera*>(m_hCameraEntity.Get());
			if (pCamera && pCamera->IsOrtho())
			{
				m_IntroData.m_hCameraEntity = pCamera;
			}
		}
#endif
	}

	// If we're currently blending to a new mode, set the second pass
	if ( m_flNextBlendTime > gpGlobals->curtime )
	{
		IntroDataBlendPass_t *pass2;
		if ( m_IntroData.m_Passes.Count() < 2 )
		{
			pass2 = &m_IntroData.m_Passes[m_IntroData.m_Passes.AddToTail()];

			//Msg("STARTED BLEND: Mode %d to %d.\n", m_IntroData.m_Passes[0].m_BlendMode, m_iNextBlendMode );
		}
		else
		{
			pass2 = &m_IntroData.m_Passes[1];

			Assert( pass2->m_BlendMode == m_iNextBlendMode );
		}
		Assert(pass2);
		pass2->m_BlendMode = m_iNextBlendMode;
		pass2->m_Alpha = 0.0f;
	}
	else if ( m_IntroData.m_Passes.Count() == 2 )
	{
		C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
		if ( !player )
			return;

		//Msg("FINISHED BLEND.\n");
		m_IntroData.m_Passes.Remove(1);
	}

	// Set the introdata our data chunk
	if ( m_bActive )
	{
		g_pIntroData = &m_IntroData;
	}
	else if ( g_pIntroData == &m_IntroData )
	{
		g_pIntroData = NULL;
	}

	// Update the fade color
	m_IntroData.m_flCurrentFadeColor[0] = m_flFadeColor[0];
	m_IntroData.m_flCurrentFadeColor[1] = m_flFadeColor[1];
	m_IntroData.m_flCurrentFadeColor[2] = m_flFadeColor[2];

	// Started fading?
	if ( m_flFadeAlpha != m_flPrevServerFadeAlpha )
	{
		m_flFadeTimeStartedAt = gpGlobals->curtime;
		m_flFadeAlphaStartedAt = m_IntroData.m_flCurrentFadeColor[3];
		m_flPrevServerFadeAlpha = m_flFadeAlpha;

		if ( !m_flFadeDuration )
		{
			m_flFadeDuration = 0.01;
		}

		//Msg("STARTING NEW FADE: alpha %.2f, duration %.2f.\n", m_flFadeAlpha, m_flFadeDuration );
	}

	if ( m_iPrevFOV != m_iFOV )
	{
		m_IntroData.m_playerViewFOV = m_iFOV;
		m_iPrevFOV = m_iFOV;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ScriptIntro::ClientThink( void )
{
	Assert( m_IntroData.m_Passes.Count() <= 2 );

	if ( m_hCameraEntity )
	{
#ifdef MAPBASE
		if ( m_bUseEyePosition )
		{
			m_hCameraEntity->GetEyePosition( m_IntroData.m_vecCameraView, m_IntroData.m_vecCameraViewAngles );
		}
		else
		{
			m_IntroData.m_vecCameraView = m_hCameraEntity->GetAbsOrigin();
			m_IntroData.m_vecCameraViewAngles = m_hCameraEntity->GetAbsAngles();
		}
#else
		m_IntroData.m_vecCameraView = m_hCameraEntity->GetAbsOrigin();
		m_IntroData.m_vecCameraViewAngles = m_hCameraEntity->GetAbsAngles();
#endif
	}

	CalculateFOV();
	CalculateAlpha();	

	// Calculate the blend levels of each pass
	float flPerc = 1.0;
	if ( (m_flNextBlendTime - m_flBlendStartTime) != 0 )
	{
		flPerc = clamp( (gpGlobals->curtime - m_flBlendStartTime) / (m_flNextBlendTime - m_flBlendStartTime), 0, 1 );
	}

	// Detect when we're finished blending
	if ( flPerc >= 1.0 )
	{
		if ( m_IntroData.m_Passes.Count() == 2 )
		{
			// We're done blending
			m_IntroData.m_Passes[0].m_BlendMode = m_IntroData.m_Passes[1].m_BlendMode;
			m_IntroData.m_Passes[0].m_Alpha = 1.0;
			m_IntroData.m_Passes.Remove(1);

			//Msg("FINISHED BLEND.\n");
		}
		else
		{
			m_IntroData.m_Passes[0].m_Alpha = 1.0f;
		}
		return;
	}

	/*
	if ( m_flNextBlendTime >= gpGlobals->curtime )
	{
		Msg("INTRO BLENDING: Blending from mode %d to %d.\n", m_IntroData.m_Passes[0].m_BlendMode, m_IntroData.m_Passes[1].m_BlendMode );
		Msg("				 curtime %.2f    StartedAt %.2f    FinishAt: %.2f\n", gpGlobals->curtime, m_flBlendStartTime, m_flNextBlendTime );
		Msg("				 Perc:   %.2f\n", flPerc );
	}
	*/

	if ( m_IntroData.m_Passes.Count() == 2 )
	{
		m_IntroData.m_Passes[0].m_Alpha = 1.0 - flPerc;
		m_IntroData.m_Passes[1].m_Alpha = flPerc;
	}
	else
	{
		m_IntroData.m_Passes[0].m_Alpha = 1.0 - flPerc;
	}
}

extern float ScriptInfo_CalculateFOV( float flFOVBlendStartTime, float flNextFOVBlendTime, int nFOV, int nNextFOV, bool bSplineRamp );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ScriptIntro::CalculateFOV( void )
{
	// We're past our blending time so we're at our target
	if ( m_flNextFOVBlendTime >= gpGlobals->curtime )
	{
		// Calculate where we're at
		m_IntroData.m_playerViewFOV = ScriptInfo_CalculateFOV( m_flFOVBlendStartTime, m_flNextFOVBlendTime, m_iStartFOV, m_iNextFOV, m_bAlternateFOV );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ScriptIntro::CalculateAlpha( void )
{
	// Fill out the fade alpha
	float flNewAlpha = RemapValClamped( gpGlobals->curtime, m_flFadeTimeStartedAt, m_flFadeTimeStartedAt + m_flFadeDuration, m_flFadeAlphaStartedAt, m_flFadeAlpha );
	/*
	if ( m_IntroData.m_flCurrentFadeColor[3] != flNewAlpha )
	{
		Msg("INTRO FADING: curtime %.2f    StartedAt %.2f    Duration: %.2f\n", gpGlobals->curtime, m_flFadeTimeStartedAt, m_flFadeDuration );
		Msg("           TimePassed %.2f    Alpha:    %.2f\n", RemapValClamped( gpGlobals->curtime, m_flFadeTimeStartedAt, m_flFadeTimeStartedAt + m_flFadeDuration, 0.0, 1.0 ), m_IntroData.m_flCurrentFadeColor[3] );
	}
	*/

	m_IntroData.m_flCurrentFadeColor[3] = flNewAlpha;
}

#ifdef MAPBASE
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_PlayerViewProxy : public C_BaseEntity
{
	DECLARE_CLASS( C_PlayerViewProxy, C_BaseEntity );
public:
	DECLARE_CLIENTCLASS();

	Vector	EyePosition( void );			// position of eyes
	const QAngle &EyeAngles( void );		// Direction of eyes in world space
	void GetEyePosition( Vector &vecOrigin, QAngle &angAngles );
	const QAngle &LocalEyeAngles( void );	// Direction of eyes
	Vector	EarPosition( void );			// position of ears

#ifdef MAPBASE_MP
	C_BasePlayer	*GetPlayer() { return m_bEnabled ? (m_hPlayer.Get() ? m_hPlayer.Get() : C_BasePlayer::GetLocalPlayer()) : NULL; }
#else
	C_BasePlayer	*GetPlayer() { return m_bEnabled ? C_BasePlayer::GetLocalPlayer() : NULL; }
#endif

public:
#ifdef MAPBASE_MP
	CHandle<C_BasePlayer> m_hPlayer;
#endif

	bool	m_bEnabled;
};

IMPLEMENT_CLIENTCLASS_DT( C_PlayerViewProxy, DT_PlayerViewProxy, CPlayerViewProxy )
#ifdef MAPBASE_MP
	RecvPropEHandle( RECVINFO( m_hPlayer ) ),
#endif
	RecvPropBool( RECVINFO( m_bEnabled ) ),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Vector C_PlayerViewProxy::EyePosition( void )
{
	C_BasePlayer *pPlayer = GetPlayer();
	if (pPlayer)
	{
		//Vector vecPlayerOffset = m_hPlayer.Get()->EyePosition() - m_hPlayer.Get()->GetAbsOrigin();
		//return GetAbsOrigin() + vecPlayerOffset;

		Vector vecOrigin;
		QAngle angAngles;
		float fldummy;
		pPlayer->CalcView( vecOrigin, angAngles, fldummy, fldummy, fldummy );

		return GetAbsOrigin() + (vecOrigin - pPlayer->GetAbsOrigin());
	}
	else
		return BaseClass::EyePosition();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const QAngle &C_PlayerViewProxy::EyeAngles( void )
{
	C_BasePlayer *pPlayer = GetPlayer();
	if (pPlayer)
	{
		Vector vecOrigin;
		static QAngle angAngles;
		float fldummy;
		pPlayer->CalcView( vecOrigin, angAngles, fldummy, fldummy, fldummy );

		angAngles = GetAbsAngles() + (angAngles - pPlayer->GetAbsAngles());
		return angAngles;

		//return m_hPlayer.Get()->EyeAngles();
	}
	else
		return BaseClass::EyeAngles();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_PlayerViewProxy::GetEyePosition( Vector &vecOrigin, QAngle &angAngles )
{
	C_BasePlayer *pPlayer = GetPlayer();
	if (pPlayer)
	{
		float fldummy;
		pPlayer->CalcView( vecOrigin, angAngles, fldummy, fldummy, fldummy );

		vecOrigin = GetAbsOrigin() + (vecOrigin - pPlayer->GetAbsOrigin());
		angAngles = GetAbsAngles() + (angAngles - pPlayer->GetAbsAngles());
	}
	else
	{
		BaseClass::GetEyePosition( vecOrigin, angAngles );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const QAngle &C_PlayerViewProxy::LocalEyeAngles( void )
{
	C_BasePlayer *pPlayer = GetPlayer();
	if (pPlayer) {
		static QAngle angAngles;
		angAngles = GetAbsAngles() + (pPlayer->LocalEyeAngles() - pPlayer->GetAbsAngles());
		return angAngles;
	}
	else
		return BaseClass::LocalEyeAngles();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Vector C_PlayerViewProxy::EarPosition( void )
{
	C_BasePlayer *pPlayer = GetPlayer();
	if (pPlayer)
	{
		Vector vecPlayerOffset = pPlayer->GetAbsOrigin() - pPlayer->EarPosition();
		return GetAbsOrigin() + vecPlayerOffset;
	}
	else
		return BaseClass::EarPosition();
}
#endif

