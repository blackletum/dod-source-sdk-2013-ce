//========= Copyright © Valve LLC, All rights reserved. =======================
//
// Purpose:		
//
// $NoKeywords: $
//=============================================================================

#ifndef TF_BOT_H
#define TF_BOT_H
#ifdef _WIN32
#pragma once
#endif


#include "NextBot/Player/NextBotPlayer.h"
#include "dod/dod_player.h"
#include "dod/dod_gamerules.h"
#include "dod/weapon_dodbase.h"
#include "dod/dod_control_point.h"
#include "dod/dod_control_point_master.h"
#include "tf_path_follower.h"
#include "tf/nav_mesh\tf_nav_area.h"
//#include "team_control_point_master.h"
#include "dod/weapon_dodbasegun.h"

class CControlPoint;
class CCaptureFlag;
class CCaptureZone;
class CTFBotSquad;

class CClosestTFPlayer
{
public:
	CClosestTFPlayer( Vector start )
		: m_vecOrigin( start )
	{
		m_flMinDist = FLT_MAX;
		m_pPlayer = nullptr;
		m_iTeam = TEAM_ANY;
	}

	CClosestTFPlayer( Vector start, int teamNum )
		: m_vecOrigin( start ), m_iTeam( teamNum )
	{
		m_flMinDist = FLT_MAX;
		m_pPlayer = nullptr;
	}

	inline bool operator()( CBasePlayer *player )
	{
		if ( ( player->GetTeamNumber() == TEAM_ALLIES || player->GetTeamNumber() == TEAM_AXIS )
			 && ( m_iTeam == TEAM_ANY || player->GetTeamNumber() == m_iTeam ) )
		{
			float flDistance = ( m_vecOrigin - player->GetAbsOrigin() ).LengthSqr();
			if ( flDistance < m_flMinDist )
			{
				m_flMinDist = flDistance;
				m_pPlayer = (CDODPlayer *)player;
			}
		}

		return true;
	}

	Vector m_vecOrigin;
	float m_flMinDist;
	CDODPlayer *m_pPlayer;
	int m_iTeam;
};


#define TF_BOT_TYPE		1337

class CTFBot : public NextBotPlayer<CDODPlayer>, public CGameEventListener
{
	DECLARE_CLASS( CTFBot, NextBotPlayer<CDODPlayer> )
public:

	static CBasePlayer *AllocatePlayerEntity( edict_t *edict, const char *playerName );

	CTFBot( CDODPlayer *player=nullptr );
	virtual ~CTFBot();

	DECLARE_INTENTION_INTERFACE( CTFBot )
	ILocomotion *m_locomotor;
	IBody *m_body;
	IVision *m_vision;

	struct DelayedNoticeInfo
	{
		CHandle<CBaseEntity> m_hEnt;
		float m_flWhen;
	};
	void			DelayedThreatNotice( CHandle<CBaseEntity> ent, float delay );
	void			UpdateDelayedThreatNotices( void );

	struct SuspectedSpyInfo
	{
		CHandle<CDODPlayer> m_hSpy;
		CUtlVector<int> m_times;

		void Suspect()
		{
			this->m_times.AddToHead( (int)floor( gpGlobals->curtime ) );
		}

		bool IsCurrentlySuspected()
		{
			if ( this->m_times.IsEmpty() )
			{
				return false;
			}

			extern ConVar tf_bot_suspect_spy_forget_cooldown;
			return ( (float)this->m_times.Head() > ( gpGlobals->curtime - tf_bot_suspect_spy_forget_cooldown.GetFloat() ) );
		}

		bool TestForRealizing()
		{
			extern ConVar tf_bot_suspect_spy_touch_interval;
			int nCurTime = (int)floor( gpGlobals->curtime );
			int nMinTime = nCurTime - tf_bot_suspect_spy_touch_interval.GetInt();

			for ( int i=m_times.Count()-1; i >= 0; --i )
			{
				if ( m_times[i] <= nMinTime )
					m_times.Remove( i );
			}

			m_times.AddToHead( nCurTime );

			CUtlVector<bool> checks;

			checks.SetCount( tf_bot_suspect_spy_touch_interval.GetInt() );
			for ( int i=0; i < checks.Count(); ++i )
				checks[ i ] = false;

			for ( int i=0; i<m_times.Count(); ++i )
			{
				int idx = nCurTime - m_times[i];
				if ( checks.IsValidIndex( idx ) )
					checks[ idx ] = true;
			}

			for ( int i=0; i<checks.Count(); ++i )
			{
				if ( !checks[ i ] )
					return false;
			}

			return true;
		}
	};
	SuspectedSpyInfo *IsSuspectedSpy( CDODPlayer *spy );
	void			SuspectSpy( CDODPlayer *spy );
	void			StopSuspectingSpy( CDODPlayer *spy );
	bool			IsKnownSpy( CDODPlayer *spy ) const;
	void			RealizeSpy( CDODPlayer *spy );
	void			ForgetSpy( CDODPlayer *spy );

	virtual void	Spawn( void );
	virtual void	Event_Killed( const CTakeDamageInfo &info );
	virtual void	UpdateOnRemove( void ) override;
	virtual void	FireGameEvent( IGameEvent *event );
	virtual int		GetBotType() const { return TF_BOT_TYPE; }
	virtual int		DrawDebugTextOverlays( void );
	virtual void	PhysicsSimulate( void );
	virtual void	Touch( CBaseEntity *other );

	virtual bool	IsDormantWhenDead( void ) const { return false; }
	virtual bool	IsDebugFilterMatch( const char *name ) const;

	virtual void	PressFireButton( float duration = -1.0f );
	virtual void	PressAltFireButton( float duration = -1.0f );
	virtual void	PressSpecialFireButton( float duration = -1.0f );

	virtual void	AvoidPlayers( CUserCmd *pCmd );

	virtual CBaseCombatCharacter *GetEntity( void ) const;

	virtual bool	IsAllowedToPickUpFlag( void );

	void			DisguiseAsEnemy( void );

	bool			IsCombatWeapon( CWeaponDODBase *weapon = nullptr ) const;
	bool			IsQuietWeapon( CWeaponDODBase *weapon = nullptr ) const;
	bool			IsHitScanWeapon( CWeaponDODBase *weapon = nullptr ) const;
	bool			IsExplosiveProjectileWeapon( CWeaponDODBase *weapon = nullptr ) const;
	bool			IsContinuousFireWeapon( CWeaponDODBase *weapon = nullptr ) const;
	bool			IsBarrageAndReloadWeapon( CWeaponDODBase *weapon = nullptr ) const;

	float			GetMaxAttackRange( void ) const;
	float			GetDesiredAttackRange( void ) const;

	float			GetDesiredPathLookAheadRange( void ) const;

	CTFNavArea*		FindVantagePoint( float flMaxDist );

	virtual ILocomotion *GetLocomotionInterface( void ) const override { return m_locomotor; }
	virtual IBody *GetBodyInterface( void ) const override { return m_body; }
	virtual IVision *GetVisionInterface( void ) const override { return m_vision; }

	bool			IsLineOfFireClear( CBaseEntity *to );
	bool			IsLineOfFireClear( const Vector& to );
	bool			IsLineOfFireClear( const Vector& from, CBaseEntity *to );
	bool			IsLineOfFireClear( const Vector& from, const Vector& to );
	bool			IsAnyEnemySentryAbleToAttackMe( void ) const;
	bool			IsThreatAimingTowardsMe( CBaseEntity *threat, float dotTolerance = 0.8 ) const;
	bool			IsThreatFiringAtMe( CBaseEntity *threat ) const;
	bool			IsEntityBetweenTargetAndSelf( CBaseEntity *blocker, CBaseEntity *target ) const;

	bool			IsAmmoLow( void ) const;
	bool			IsAmmoFull( void ) const;

	bool			AreAllPointsUncontestedSoFar( void ) const;
	bool			IsNearPoint( CControlPoint *point ) const;
	void			ClearMyControlPoint( void ) { m_hMyControlPoint = nullptr; }
	CControlPoint *GetMyControlPoint( void );
	bool			IsAnyPointBeingCaptured( void ) const;
	bool			IsPointBeingContested( CControlPoint *point ) const;
	float			GetTimeLeftToCapture( void );
	CControlPoint *SelectPointToCapture( const CUtlVector<CControlPoint *> &candidates );
	CControlPoint *SelectPointToDefend( const CUtlVector<CControlPoint *> &candidates );
	CControlPoint *SelectClosestPointByTravelDistance( const CUtlVector<CControlPoint *> &candidates ) const;

	CCaptureZone*	GetFlagCaptureZone( void );
	CCaptureFlag*	GetFlagToFetch( void );

	float			TransientlyConsistentRandomValue( float duration, int seed ) const;

	bool			IsPointInRound(CControlPoint* pPoint, CControlPointMaster* pMaster);

	void			CollectCapturePoints(CUtlVector<CControlPoint*>* controlPointVector);
	void			CollectDefendPoints(CUtlVector<CControlPoint*>* controlPointVector);

	float			MedicGetChargeLevel(void);
	CBaseEntity*	MedicGetHealTarget(void);

	const Vector& EstimateProjectileImpactPosition(CWeaponDODBaseGun* weapon);
	const Vector& EstimateProjectileImpactPosition(float pitch, float yaw, float speed);
	const Vector& EstimateStickybombProjectileImpactPosition(float pitch, float yaw, float charge);

	bool			IsCapturingPoint(void);

	bool			IsTeleporterSendingPlayer(CBaseEntity* pTele) { return false; }

	void			UpdateLookingAroundForEnemies( void );
	void			UpdateLookingForIncomingEnemies( bool );

	bool			EquipBestWeaponForThreat( const CKnownEntity *threat );
	bool			EquipLongRangeWeapon( void );

	void			PushRequiredWeapon( CWeaponDODBase *weapon );
	bool			EquipRequiredWeapon( void );
	void			PopRequiredWeapon( void );

	CTFBotSquad*	GetSquad( void ) const { return m_pSquad; }
	bool			IsSquadmate( CDODPlayer *player ) const;
	void			JoinSquad( CTFBotSquad *squad );
	void			LeaveSquad();

	struct SniperSpotInfo
	{
		CTFNavArea *m_pHomeArea;
		Vector m_vecHome;
		CTFNavArea *m_pForwardArea;
		Vector m_vecForward;
		float m_flRange;
		float m_flIncursionDiff;
	};
	void			AccumulateSniperSpots( void );
	void			SetupSniperSpotAccumulation( void );
	void			ClearSniperSpots( void );

	void			SelectReachableObjects( CUtlVector<EHANDLE> const& append, CUtlVector<EHANDLE> *outVector, INextBotFilter const& func, CNavArea *pStartArea, float flMaxRange );
	CDODPlayer *		SelectRandomReachableEnemy( void );

	bool			CanChangeClass( void );
	int		GetNextSpawnClassname( void );

	friend class CTFBotSquad;
	CTFBotSquad *m_pSquad;
	float m_flFormationError;
	bool m_bIsInFormation;

	CTFNavArea *m_HomeArea;
	CUtlVector<SniperSpotInfo> m_sniperSpots;

	Vector m_vecLastHurtBySentry;

	enum DifficultyType
	{
		EASY   = 0,
		NORMAL = 1,
		HARD   = 2,
		EXPERT = 3,
		MAX
	}
	m_iSkill;

	enum class AttributeType : int
	{
		NONE                    = 0,

		REMOVEONDEATH           = (1 << 0),
		AGGRESSIVE              = (1 << 1),
		DONTLOOKAROUND			= (1 << 2),
		SUPPRESSFIRE            = (1 << 3),
		DISABLEDODGE            = (1 << 4),
		BECOMESPECTATORONDEATH  = (1 << 5),
		// 6?
		RETAINBUILDINGS         = (1 << 7),
		SPAWNWITHFULLCHARGE     = (1 << 8),
		ALWAYSCRIT              = (1 << 9),
		IGNOREENEMIES           = (1 << 10),
		HOLDFIREUNTILFULLRELOAD = (1 << 11),
		// 12?
		ALWAYSFIREWEAPON        = (1 << 13),
		TELEPORTTOHINT          = (1 << 14),
		MINIBOSS                = (1 << 15),
		USEBOSSHEALTHBAR        = (1 << 16),
		IGNOREFLAG              = (1 << 17),
		AUTOJUMP                = (1 << 18),
		AIRCHARGEONLY           = (1 << 19),
		VACCINATORBULLETS       = (1 << 20),
		VACCINATORBLAST         = (1 << 21),
		VACCINATORFIRE          = (1 << 22),
		BULLETIMMUNE            = (1 << 23),
		BLASTIMMUNE             = (1 << 24),
		FIREIMMUNE              = (1 << 25),
	}
	m_nBotAttrs;

	bool m_bLookingAroundForEnemies;

	CountdownTimer m_cpChangedTimer;
	

private:
	bool m_bWantsToChangeClass;

	CountdownTimer m_lookForEnemiesTimer;

	CDODPlayer *m_controlling;

	CHandle<CControlPoint> m_hMyControlPoint;
	CountdownTimer m_myCPValidDuration;

	CHandle<CCaptureZone> m_hMyCaptureZone;

	CountdownTimer m_useWeaponAbilityTimer;

	CUtlVector< CHandle<CWeaponDODBase> > m_requiredEquipStack;

	CUtlVector<DelayedNoticeInfo> m_delayedThreatNotices;

	CUtlVectorAutoPurge<SuspectedSpyInfo *> m_suspectedSpies;
	CUtlVector< CHandle<CDODPlayer> > m_knownSpies;

	CUtlVector<CTFNavArea *> m_sniperStandAreas;
	CUtlVector<CTFNavArea *> m_sniperLookAreas;
	CBaseEntity *m_sniperGoalEnt;
	Vector m_sniperGoal;
	CountdownTimer m_sniperSpotTimer;
};

class CTFBotPathCost : public IPathCost
{
public:
	CTFBotPathCost( CTFBot *actor, RouteType routeType = DEFAULT_ROUTE );
	virtual ~CTFBotPathCost() { }

	virtual float operator()( CNavArea *area, CNavArea *fromArea, const CNavLadder *ladder, const CFuncElevator *elevator, float length ) const override;

private:
	CTFBot *m_Actor;
	RouteType m_iRouteType;
	float m_flStepHeight;
	float m_flMaxJumpHeight;
	float m_flDeathDropHeight;
};

DEFINE_ENUM_BITWISE_OPERATORS( CTFBot::AttributeType )
inline bool operator!( CTFBot::AttributeType const &rhs )
{
	return (int const &)rhs == 0; 
}
inline bool operator!=( CTFBot::AttributeType const &rhs, int const &lhs )
{
	return (int const &)rhs != lhs;
}

inline int GetEnemyTeam(CBaseEntity* ent)
{
	int myTeam = ent->GetTeamNumber();
	return (myTeam == TEAM_AXIS ? TEAM_ALLIES : (myTeam == TEAM_ALLIES ? TEAM_AXIS : TEAM_ANY));
}

inline CTFBot *ToTFBot( CBaseEntity *ent )
{
	CDODPlayer *player = ToDODPlayer( ent );
	if ( player == nullptr )
		return NULL;

	if ( !player->IsBotOfType( TF_BOT_TYPE ) )
		return NULL;

	Assert( dynamic_cast<CTFBot *>( ent ) );
	return static_cast<CTFBot *>( ent );
}

class CDODPlayerPathCost : public IPathCost
{
public:
	CDODPlayerPathCost(CDODPlayer* player)
		: m_pPlayer(player)
	{
		m_flStepHeight = 18.0f;
		m_flMaxJumpHeight = 72.0f;
		m_flDeathDropHeight = 200.0f;
	}

	virtual float operator()(CNavArea* area, CNavArea* fromArea, const CNavLadder* ladder, const CFuncElevator* elevator, float length) const;

private:
	CDODPlayer* m_pPlayer;
	float m_flStepHeight;
	float m_flMaxJumpHeight;
	float m_flDeathDropHeight;
};

bool IsPlayerClassName(const char* name);
int GetClassIndexFromString(const char* name, int maxClass = 6);
bool IsTeamName(const char* name);

#endif
