//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: This is an entity that represents a vgui screen
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "vguiscreen.h"
#include "networkstringtable_gamedll.h"
#include "saverestore_stringtable.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// This is an entity that represents a vgui screen
//-----------------------------------------------------------------------------

IMPLEMENT_SERVERCLASS_ST(CVGuiScreen, DT_VGuiScreen)
	SendPropFloat(SENDINFO(m_flWidth),	0, SPROP_NOSCALE ),
	SendPropFloat(SENDINFO(m_flHeight),	0, SPROP_NOSCALE ),
	SendPropInt(SENDINFO(m_nAttachmentIndex), 5, SPROP_UNSIGNED ),
	SendPropInt(SENDINFO(m_nPanelName), MAX_VGUI_SCREEN_STRING_BITS, SPROP_UNSIGNED ),
	SendPropInt(SENDINFO(m_fScreenFlags), VGUI_SCREEN_MAX_BITS, SPROP_UNSIGNED ),
	SendPropInt(SENDINFO(m_nOverlayMaterial), MAX_MATERIAL_STRING_BITS, SPROP_UNSIGNED ),
	SendPropEHandle(SENDINFO(m_hPlayerOwner)),
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS( vgui_screen, CVGuiScreen );
LINK_ENTITY_TO_CLASS( vgui_screen_team, CVGuiScreen );
PRECACHE_REGISTER( vgui_screen );


//-----------------------------------------------------------------------------
// Save/load
//-----------------------------------------------------------------------------
BEGIN_DATADESC(CVGuiScreen)

DEFINE_CUSTOM_FIELD(m_nPanelName, &g_VguiScreenStringOps),
DEFINE_FIELD(m_nAttachmentIndex, FIELD_INTEGER),
//	DEFINE_FIELD( m_nOverlayMaterial, FIELD_INTEGER ),
DEFINE_FIELD(m_fScreenFlags, FIELD_INTEGER),
DEFINE_KEYFIELD(m_flWidth, FIELD_FLOAT, "width"),
DEFINE_KEYFIELD(m_flHeight, FIELD_FLOAT, "height"),
DEFINE_KEYFIELD(m_strOverlayMaterial, FIELD_STRING, "overlaymaterial"),
DEFINE_FIELD(m_hPlayerOwner, FIELD_EHANDLE),

DEFINE_INPUTFUNC(FIELD_VOID, "SetActive", InputSetActive),
DEFINE_INPUTFUNC(FIELD_VOID, "SetInactive", InputSetInactive),

END_DATADESC();


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CVGuiScreen::CVGuiScreen()
{
	m_nOverlayMaterial = OVERLAY_MATERIAL_INVALID_STRING;
	m_hPlayerOwner = INVALID_EHANDLE;
}


//-----------------------------------------------------------------------------
// Read in worldcraft data...
//-----------------------------------------------------------------------------
bool CVGuiScreen::KeyValue( const char *szKeyName, const char *szValue ) 
{
	//!! temp hack, until worldcraft is fixed
	// strip the # tokens from (duplicate) key names
	char *s = (char *)strchr( szKeyName, '#' );
	if ( s )
	{
		*s = '\0';
	}

#ifdef MAPBASE
	// Named command outputs
	if (szKeyName[0] == '~' && szKeyName[1])
	{
		const char* pszOutputName = szKeyName + 1;
		int i = m_PanelOutputs.Find(pszOutputName);
		if (!m_PanelOutputs.IsValidIndex(i))
		{
			auto pMem = new COutputEvent;
			V_memset(pMem, 0, sizeof(COutputEvent));
			i = m_PanelOutputs.Insert(pszOutputName, pMem);
		}

		m_PanelOutputs[i]->ParseEventAction(szValue);
		return true;
	}
#endif // MAPBASE

	if ( FStrEq( szKeyName, "panelname" ))
	{
		SetPanelName( szValue );
		return true;
	}

	// NOTE: Have to do these separate because they set two values instead of one
	if( FStrEq( szKeyName, "angles" ) )
	{
		Assert( GetMoveParent() == NULL );
		QAngle angles;
		UTIL_StringToVector( angles.Base(), szValue );

		// Because the vgui screen basis is strange (z is front, y is up, x is right)
		// we need to rotate the typical basis before applying it
		VMatrix mat, rotation, tmp;
		MatrixFromAngles( angles, mat );
		MatrixBuildRotationAboutAxis( rotation, Vector( 0, 1, 0 ), 90 );
		MatrixMultiply( mat, rotation, tmp );
		MatrixBuildRotateZ( rotation, 90 );
		MatrixMultiply( tmp, rotation, mat );
		MatrixToAngles( mat, angles );
		SetAbsAngles( angles );

		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}


//-----------------------------------------------------------------------------
// Precache...
//-----------------------------------------------------------------------------
void CVGuiScreen::Precache()
{
	BaseClass::Precache();
	if ( m_strOverlayMaterial != NULL_STRING )
	{
		PrecacheMaterial( STRING(m_strOverlayMaterial) );
	}
}


//-----------------------------------------------------------------------------
// Spawn...
//-----------------------------------------------------------------------------
void CVGuiScreen::Spawn()
{
	Precache();

	// This has no model, but we want it to transmit if it's in the PVS anyways
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
	m_nAttachmentIndex = 0;
	SetSolid( SOLID_OBB );
	AddSolidFlags( FSOLID_NOT_SOLID );
	SetActualSize( m_flWidth, m_flHeight );
	m_fScreenFlags.Set( VGUI_SCREEN_ACTIVE );

	m_takedamage = DAMAGE_NO;
	AddFlag( FL_NOTARGET );
}

//-----------------------------------------------------------------------------
// Spawn...
//-----------------------------------------------------------------------------
void CVGuiScreen::Activate()
{
	BaseClass::Activate();

	if ( m_nOverlayMaterial == OVERLAY_MATERIAL_INVALID_STRING && m_strOverlayMaterial != NULL_STRING )
	{
		SetOverlayMaterial( STRING(m_strOverlayMaterial) );
	}
}

void CVGuiScreen::OnRestore()
{
	UpdateTransmitState();

	BaseClass::OnRestore();
}

#ifdef MAPBASE
CVGuiScreen::~CVGuiScreen()
{
	m_PanelOutputs.PurgeAndDeleteElements();
}

int CVGuiScreen::Save(ISave& save)
{
#if MAPBASE_VER_INT < 8000
	// HACKHACK: Until v8.0, mark this screen as using the new save system to prevent existing saves with vgui_screen from crashing
	AddContext( "uses_new_save", "1" );
#endif

	int status = BaseClass::Save(save);
	if (!status)
		return 0;

	const int iCount = m_PanelOutputs.Count();
	save.WriteInt(&iCount);
	for (int i = 0; i < iCount; i++)
	{
		CBaseEntityOutput* output = m_PanelOutputs[i];
		const int nElems = output->NumberOfElements();
		save.WriteString(m_PanelOutputs.GetElementName(i));
		save.WriteInt(&nElems);
		if (!output->Save(save))
			return 0;
	}

	return status;
}

int CVGuiScreen::Restore(IRestore& restore)
{
	int status = BaseClass::Restore(restore);
	if (!status)
		return 0;

#if MAPBASE_VER_INT < 8000
	// HACKHACK: Until v8.0, mark this screen as using the new save system to prevent existing saves with vgui_screen from crashing
	if (!HasContext( "uses_new_save", "1" ))
		return status;
#endif

	const int iCount = restore.ReadInt();
	m_PanelOutputs.EnsureCapacity(iCount);
	for (int i = 0; i < iCount; i++)
	{
		char cName[MAX_KEY];
		restore.ReadString(cName, MAX_KEY, 0);
		const int iIndex = m_PanelOutputs.Insert(cName, new COutputEvent);
		const int nElems = restore.ReadInt();
		if (!m_PanelOutputs[iIndex]->Restore(restore, nElems))
			return 0;
	}

	return status;
}

// Handle a command from the client-side vgui panel.
bool CVGuiScreen::HandleEntityCommand(CBasePlayer* pClient, KeyValues* pKeyValues)
{
#if defined(HL2MP) // Enable this in multiplayer.
	// Restrict to commands from our owning player.
	if ((m_fScreenFlags & VGUI_SCREEN_ONLY_USABLE_BY_OWNER) && pClient != m_hPlayerOwner.Get())
		return false;
#endif
	
	// Give the owning entity a chance to handle the command.
	if (GetOwnerEntity() && GetOwnerEntity()->HandleEntityCommand(pClient, pKeyValues))
		return true;

	// See if we have an output for this command.
	const int i = m_PanelOutputs.Find(pKeyValues->GetString());
	if (m_PanelOutputs.IsValidIndex(i))
	{
		variant_t Val;
		Val.Set(FIELD_VOID, NULL);
		m_PanelOutputs[i]->FireOutput(Val, pClient, this);
		return true;
	}

	return false;
}

CBaseEntityOutput* CVGuiScreen::FindNamedOutput(const char* pszOutput)
{
	if (pszOutput && pszOutput[0] == '~' && pszOutput[1])
	{
		const int i = m_PanelOutputs.Find(pszOutput + 1);
		if (m_PanelOutputs.IsValidIndex(i))
			return m_PanelOutputs[i];

		return NULL;
	}

	return BaseClass::FindNamedOutput(pszOutput);
}
#endif // MAPBASE

void CVGuiScreen::SetAttachmentIndex( int nIndex )
{
	m_nAttachmentIndex = nIndex;
}

void CVGuiScreen::SetOverlayMaterial( const char *pMaterial )
{
	int iMaterial = GetMaterialIndex( pMaterial );

	if ( iMaterial == 0 )
	{
		m_nOverlayMaterial = OVERLAY_MATERIAL_INVALID_STRING;
	}
	else
	{
		m_nOverlayMaterial = iMaterial;
	}
}

bool CVGuiScreen::IsActive() const 
{ 
	return (m_fScreenFlags & VGUI_SCREEN_ACTIVE) != 0; 
}

void CVGuiScreen::SetActive( bool bActive )
{
	if (bActive != IsActive())
	{
		if (!bActive)
		{
			m_fScreenFlags &= ~VGUI_SCREEN_ACTIVE;
		}
		else
		{
			m_fScreenFlags.Set(  m_fScreenFlags | VGUI_SCREEN_ACTIVE );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CVGuiScreen::IsAttachedToViewModel() const
{
	return (m_fScreenFlags & VGUI_SCREEN_ATTACHED_TO_VIEWMODEL) != 0; 
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bAttached - 
//-----------------------------------------------------------------------------
void CVGuiScreen::SetAttachedToViewModel( bool bAttached )
{
	if (bAttached != IsActive())
	{
		if (!bAttached)
		{
			m_fScreenFlags &= ~VGUI_SCREEN_ATTACHED_TO_VIEWMODEL;
		}
		else
		{
			m_fScreenFlags.Set( m_fScreenFlags | VGUI_SCREEN_ATTACHED_TO_VIEWMODEL );

			// attached screens have different transmit rules
			DispatchUpdateTransmitState();
		}

		// attached screens have different transmit rules
		DispatchUpdateTransmitState();
	}
}

void CVGuiScreen::SetTransparency( bool bTransparent )
{
	if (!bTransparent)
	{
		m_fScreenFlags &= ~VGUI_SCREEN_TRANSPARENT;
	}
	else
	{
		m_fScreenFlags.Set( m_fScreenFlags | VGUI_SCREEN_TRANSPARENT );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVGuiScreen::InputSetActive( inputdata_t &inputdata )
{
	SetActive( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVGuiScreen::InputSetInactive( inputdata_t &inputdata )
{
	SetActive( false );
}

bool CVGuiScreen::IsVisibleOnlyToTeammates() const 
{ 
	return (m_fScreenFlags & VGUI_SCREEN_VISIBLE_TO_TEAMMATES) != 0; 
}

void CVGuiScreen::MakeVisibleOnlyToTeammates( bool bActive )
{
	if (bActive != IsVisibleOnlyToTeammates())
	{
		if (!bActive)
		{
			m_fScreenFlags &= ~VGUI_SCREEN_VISIBLE_TO_TEAMMATES;
		}
		else
		{
			m_fScreenFlags.Set(  m_fScreenFlags | VGUI_SCREEN_VISIBLE_TO_TEAMMATES );
		}
	}
}

bool CVGuiScreen::IsVisibleToTeam( int nTeam )
{
	// FIXME: Should this maybe go into a derived class of some sort?
	// Don't bother with screens on the wrong team
	if ( IsVisibleOnlyToTeammates() && (nTeam > 0) )
	{
		// Hmmm... sort of a hack...
		CBaseEntity *pOwner = GetOwnerEntity();
		if ( pOwner && (nTeam != pOwner->GetTeamNumber()) )
			return false;
	}
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Screens attached to view models only go to client if viewmodel is being sent, too.
// Input  : *recipient - 
//			*pvs - 
//			clientArea - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
int CVGuiScreen::UpdateTransmitState()
{
	if ( IsAttachedToViewModel() )
	{
		// only send to the owner, or someone spectating the owner.
		return SetTransmitState( FL_EDICT_FULLCHECK );
	}
	else if ( GetMoveParent() )
	{
		// Let the parent object trigger the send. This is more efficient than having it call CBaseEntity::ShouldTransmit
		// for all the vgui screens in the map.
		return SetTransmitState( FL_EDICT_PVSCHECK );
	}
	else
	{
		return BaseClass::UpdateTransmitState();
	}
}

int CVGuiScreen::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	Assert( IsAttachedToViewModel() );

	CBaseEntity *pViewModel = GetOwnerEntity();

	if ( pViewModel )
	{
		return pViewModel->ShouldTransmit( pInfo );
	}

	return BaseClass::ShouldTransmit( pInfo );
}

//-----------------------------------------------------------------------------
// Convert the panel name into an integer
//-----------------------------------------------------------------------------
void CVGuiScreen::SetPanelName( const char *pPanelName )
{
	m_nPanelName = g_pStringTableVguiScreen->AddString( CBaseEntity::IsServer(), pPanelName );
}

const char *CVGuiScreen::GetPanelName() const
{
	return g_pStringTableVguiScreen->GetString( m_nPanelName );
}


//-----------------------------------------------------------------------------
// Sets the screen size + resolution
//-----------------------------------------------------------------------------
void CVGuiScreen::SetActualSize( float flWidth, float flHeight )
{
	m_flWidth = flWidth;
	m_flHeight = flHeight;

	Vector mins, maxs;
	mins.Init( 0.0f, 0.0f, -0.1f );
	maxs.Init( 0.0f, 0.0f, 0.1f );
	if (flWidth > 0)
		maxs.x = flWidth;
	else
		mins.x = flWidth;
	if (flHeight > 0)
		maxs.y = flHeight;
	else
		mins.y = flHeight;

	UTIL_SetSize( this, mins, maxs );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CVGuiScreen::SetPlayerOwner( CBasePlayer *pPlayer, bool bOwnerOnlyInput /* = false */ )
{
	m_hPlayerOwner = pPlayer;

	if ( bOwnerOnlyInput )
	{
		m_fScreenFlags.Set( VGUI_SCREEN_ONLY_USABLE_BY_OWNER );
	}
}


//-----------------------------------------------------------------------------
// Precaches a vgui screen
//-----------------------------------------------------------------------------
void PrecacheVGuiScreen( const char *pScreenType )
{
	g_pStringTableVguiScreen->AddString( CBaseEntity::IsServer(), pScreenType );
}


//-----------------------------------------------------------------------------
// Creates a vgui screen, attaches it to another player
//-----------------------------------------------------------------------------
CVGuiScreen *CreateVGuiScreen( const char *pScreenClassname, const char *pScreenType, CBaseEntity *pAttachedTo, CBaseEntity *pOwner, int nAttachmentIndex )
{
	Assert( pAttachedTo );
	CVGuiScreen *pScreen = (CVGuiScreen *)CBaseEntity::Create( pScreenClassname, vec3_origin, vec3_angle, pAttachedTo );

	pScreen->SetPanelName( pScreenType );
	pScreen->FollowEntity( pAttachedTo );
	pScreen->SetOwnerEntity( pOwner );
	pScreen->SetAttachmentIndex( nAttachmentIndex );

	return pScreen;
}

void DestroyVGuiScreen( CVGuiScreen *pVGuiScreen )
{
	if (pVGuiScreen)
	{
		UTIL_Remove( pVGuiScreen );
	}
}
