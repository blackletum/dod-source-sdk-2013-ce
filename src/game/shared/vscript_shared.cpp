//========== Copyright ? 2008, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#include "cbase.h"
#include "vscript_shared.h"
#include "icommandline.h"
#include "tier1/utlbuffer.h"
#include "tier1/fmtstr.h"
#include "filesystem.h"
#include "characterset.h"
#include "isaverestore.h"
#include "gamerules.h"
#ifdef MAPBASE_VSCRIPT
#include "mapbase/vscript_singletons.h"
#endif

#if defined(CLIENT_DLL) && defined(PANORAMA_ENABLE)
#include "panorama/uijsregistration.h"
#endif

IScriptVM * g_pScriptVM;
extern ScriptClassDesc_t * GetScriptDesc( CBaseEntity * );

DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_VScript, "VScript", 0, LS_MESSAGE, Color( 245, 175, 238, 255 ) );

// #define VMPROFILE 1

#ifdef VMPROFILE

#define VMPROF_START double debugStartTime = Plat_FloatTime();
#define VMPROF_SHOW( funcname, funcdesc  ) DevMsg("***VSCRIPT PROFILE***: %s %s: %6.4f milliseconds\n", (##funcname), (##funcdesc), (Plat_FloatTime() - debugStartTime)*1000.0 );

#else // !VMPROFILE

#define VMPROF_START
#define VMPROF_SHOW

#endif // VMPROFILE

#ifdef CLIENT_DLL
/*class CPanoramaVScript
{
public:
	void RegisterVariable( const char *pszName, const char *pszInitialValue, const char *pszDesc )
	{
		KeyValues* pKey = m_KeyValues.CreateKey( pszName );
		pKey->SetStringValue(pszInitialValue);

		panorama::RegisterJSAccessorReadOnly(pszName, PANORAMA_DELEGATE(delegate), pszDesc);
	}

	void SetValue( const char *pszName, const char *pszValue )
	{
		m_KeyValues.SetBool( pszName, pszValue );
	}
private:
	KeyValues m_KeyValues;
};*/
#endif

#ifdef MAPBASE_VSCRIPT
// This is to ensure a dependency exists between the vscript library and the game DLLs
extern int vscript_token;
extern int vscript_debugger_port;
int vscript_token_hack = vscript_token;
#endif

static const char *pszExtensions[] =
{
	"",		// SL_NONE
	".gm",	// SL_GAMEMONKEY
	".nut",	// SL_SQUIRREL
	".lua", // SL_LUA
	".py",  // SL_PYTHON
};



HSCRIPT VScriptCompileScript( const char *pszScriptName, bool bWarnMissing )
{
	if ( !g_pScriptVM )
	{
		return NULL;
	}

	const char *pszVMExtension = pszExtensions[g_pScriptVM->GetLanguage()];
	const char *pszIncomingExtension = V_strrchr( pszScriptName , '.' );
	if ( pszIncomingExtension && V_strcmp( pszIncomingExtension, pszVMExtension ) != 0 )
	{
#ifdef MAPBASE_VSCRIPT
		CGWarning( 0, CON_GROUP_VSCRIPT, "Script file type (\"%s\", from \"%s\") does not match VM type (\"%s\")\n", pszIncomingExtension, pszScriptName, pszVMExtension );
#else
		Log_Warning( LOG_VScript, "Script file type does not match VM type\n" );
#endif
		return NULL;
	}

	CFmtStr scriptPath;
	if ( pszIncomingExtension )
	{
		scriptPath.sprintf( "scripts/vscripts/%s", pszScriptName );
	}
	else
	{	
		scriptPath.sprintf( "scripts/vscripts/%s%s", pszScriptName,  pszVMExtension );
	}

	const char *pBase;
	CUtlBuffer bufferScript;

	if ( g_pScriptVM->GetLanguage() == SL_PYTHON )
	{
		// python auto-loads raw or precompiled modules - don't load data here
		pBase = NULL;
	}
	else
	{
		bool bResult = filesystem->ReadFile( scriptPath, "GAME", bufferScript );

#ifdef MAPBASE_VSCRIPT
		if ( !bResult && bWarnMissing )
#else
		if( !bResult )
#endif
		{
#ifdef MAPBASE_VSCRIPT
			CGWarning( 0, CON_GROUP_VSCRIPT, "Script not found (%s) \n", scriptPath.operator const char *() );
#else
			Log_Warning( LOG_VScript, "Script not found (%s) \n", scriptPath.operator const char *() );
#endif
			Assert( "Error running script" );
		}

		pBase = (const char *) bufferScript.Base();

		if ( !pBase || !*pBase )
		{
			return NULL;
		}
	}


	const char *pszFilename = V_strrchr( scriptPath, '/' );
	pszFilename++;
	HSCRIPT hScript = g_pScriptVM->CompileScript( pBase, pszFilename );
	if ( !hScript )
	{
#ifdef MAPBASE_VSCRIPT
		CGWarning( 0, CON_GROUP_VSCRIPT, "FAILED to compile and execute script file named %s\n", scriptPath.operator const char *() );
#else
		Log_Warning( LOG_VScript, "FAILED to compile and execute script file named %s\n", scriptPath.operator const char *() );
#endif
		Assert( "Error running script" );
	}
	return hScript;
}

static int g_ScriptServerRunScriptDepth;

bool VScriptRunScript( const char *pszScriptName, HSCRIPT hScope, bool bWarnMissing )
{
	if ( !g_pScriptVM )
	{
		return false;
	}

	if ( !pszScriptName || !*pszScriptName )
	{
#ifdef MAPBASE_VSCRIPT
		Log_Warning( LOG_VScript, "Cannot run script: NULL script name\n" );
#else
		CGWarning( 0, CON_GROUP_VSCRIPT, "Cannot run script: NULL script name\n" );
#endif
		return false;
	}

	// Prevent infinite recursion in VM
	if ( g_ScriptServerRunScriptDepth > 16 )
	{
#ifdef MAPBASE_VSCRIPT
		CGWarning( 0, CON_GROUP_VSCRIPT, "IncludeScript stack overflow\n" );
#else
		Log_Warning( LOG_VScript, "IncludeScript stack overflow\n" );
#endif
		return false;
	}

	g_ScriptServerRunScriptDepth++;
	HSCRIPT	hScript = VScriptCompileScript( pszScriptName, bWarnMissing );
	bool bSuccess = false;
	if ( hScript )
	{
		// player is not yet spawned, this block is always skipped.
		// It is registered in CBasePlayer instead.
#ifndef MAPBASE
#ifdef GAME_DLL
		if ( gpGlobals->maxClients == 1 )
		{
			CBaseEntity *pPlayer = UTIL_GetLocalPlayer();
			if ( pPlayer )
			{
				g_pScriptVM->SetValue( "player", pPlayer->GetScriptInstance() );
			}
		}
#endif
#endif
		bSuccess = ( g_pScriptVM->Run( hScript, hScope ) != SCRIPT_ERROR );
		if ( !bSuccess )
		{
#ifdef MAPBASE_VSCRIPT
			Log_Warning( LOG_VScript, "Error running script named %s\n", pszScriptName );
#else
			Warning( "Error running script named %s\n", pszScriptName );
#endif
			Assert( "Error running script" );
		}
	}
	g_ScriptServerRunScriptDepth--;
	return bSuccess;
}


#ifdef MAPBASE_VSCRIPT

//
// These functions are currently only used for "mapspawn_addon" scripts.
//
HSCRIPT VScriptCompileScriptAbsolute( const char *pszScriptName, bool bWarnMissing, const char *pszRootFolderName )
{
	if ( !g_pScriptVM )
	{
		return NULL;
	}

	const char *pszVMExtension = pszExtensions[g_pScriptVM->GetLanguage()];
	const char *pszIncomingExtension = V_strrchr( pszScriptName , '.' );
	if ( pszIncomingExtension && V_strcmp( pszIncomingExtension, pszVMExtension ) != 0 )
	{
		// Account for cases where there is no extension and the folder names just have dots (e.g. ".local")
		if ( strchr( pszIncomingExtension, CORRECT_PATH_SEPARATOR ) )
		{
			pszIncomingExtension = NULL;
		}
		else
		{
			CGWarning( 0, CON_GROUP_VSCRIPT, "Script file type (\"%s\", from \"%s\") does not match VM type (\"%s\")\n", pszIncomingExtension, pszScriptName, pszVMExtension );
			return NULL;
		}
	}

	CFmtStr scriptPath;
	if ( pszIncomingExtension )
	{
		scriptPath = pszScriptName;
	}
	else
	{	
		scriptPath.sprintf( "%s%s", pszScriptName,  pszVMExtension );
	}

	const char *pBase;
	CUtlBuffer bufferScript;

	if ( g_pScriptVM->GetLanguage() == SL_PYTHON )
	{
		// python auto-loads raw or precompiled modules - don't load data here
		pBase = NULL;
	}
	else
	{
		bool bResult = filesystem->ReadFile( scriptPath, NULL, bufferScript );

		if ( !bResult && bWarnMissing )
		{
			CGWarning( 0, CON_GROUP_VSCRIPT, "Script not found (%s) \n", scriptPath.operator const char *() );
			Assert( "Error running script" );
		}

		pBase = (const char *) bufferScript.Base();

		if ( !pBase || !*pBase )
		{
			return NULL;
		}
	}

	// Attach the folder to the script ID
	const char *pszFilename = V_strrchr( scriptPath, '/' );
	scriptPath.sprintf( "%s%s", pszRootFolderName, pszFilename );

	HSCRIPT hScript = g_pScriptVM->CompileScript( pBase, scriptPath );
	if ( !hScript )
	{
		CGWarning( 0, CON_GROUP_VSCRIPT, "FAILED to compile and execute script file named %s\n", scriptPath.operator const char *() );
		Assert( "Error running script" );
	}
	return hScript;
}

bool VScriptRunScriptAbsolute( const char *pszScriptName, HSCRIPT hScope, bool bWarnMissing, const char *pszRootFolderName )
{
	if ( !g_pScriptVM )
	{
		return false;
	}

	if ( !pszScriptName || !*pszScriptName )
	{
		CGWarning( 0, CON_GROUP_VSCRIPT, "Cannot run script: NULL script name\n" );
		return false;
	}

	// Prevent infinite recursion in VM
	if ( g_ScriptServerRunScriptDepth > 16 )
	{
		CGWarning( 0, CON_GROUP_VSCRIPT, "IncludeScript stack overflow\n" );
		return false;
	}

	g_ScriptServerRunScriptDepth++;
	HSCRIPT	hScript = VScriptCompileScriptAbsolute( pszScriptName, bWarnMissing, pszRootFolderName );
	bool bSuccess = false;
	if ( hScript )
	{
		bSuccess = ( g_pScriptVM->Run( hScript, hScope ) != SCRIPT_ERROR );
		if ( !bSuccess )
		{
			Warning( "Error running script named %s\n", pszScriptName );
			Assert( "Error running script" );
		}
	}
	g_ScriptServerRunScriptDepth--;
	return bSuccess;
}
#endif


#ifdef GAME_DLL
#define IsCommandIssuedByServerAdmin() UTIL_IsCommandIssuedByServerAdmin()
#else
#define IsCommandIssuedByServerAdmin() true
#endif

#ifdef CLIENT_DLL
CON_COMMAND( script_client, "Run the text as a script" )
#else
CON_COMMAND( script, "Run the text as a script" )
#endif
{
	if ( !IsCommandIssuedByServerAdmin() )
		return;

	if ( !*args[1] )
	{
#ifdef MAPBASE_VSCRIPT
		CGWarning( 0, CON_GROUP_VSCRIPT, "No function name specified\n" );
#else
		Log_Warning( LOG_VScript, "No function name specified\n" );
#endif
		return;
	}

	if ( !g_pScriptVM )
	{
#ifdef MAPBASE_VSCRIPT
		CGWarning( 0, CON_GROUP_VSCRIPT, "Scripting disabled or no server running\n" );
#else
		Log_Warning( LOG_VScript, "Scripting disabled or no server running\n" );
#endif
		return;
	}

	const char *pszScript = args.GetCommandString();

#ifdef CLIENT_DLL
	pszScript += 13;
#else
	pszScript += 6;
#endif
	
	while ( *pszScript == ' ' )
	{
		pszScript++;
	}

	if ( !*pszScript )
	{
		return;
	}

	if ( *pszScript != '\"' )
	{
		g_pScriptVM->Run( pszScript );
	}
	else
	{
		pszScript++;
		const char *pszEndQuote = pszScript;
		while ( *pszEndQuote !=  '\"' )
		{
			pszEndQuote++;
		}
		if ( !*pszEndQuote )
		{
			return;
		}
		*((char *)pszEndQuote) = 0;
		g_pScriptVM->Run( pszScript );
		*((char *)pszEndQuote) = '\"';
	}
}

#ifdef CLIENT_DLL
CON_COMMAND( script_execute_client, "Run a vscript file" )
#else
CON_COMMAND( script_execute, "Run a vscript file" )
#endif
{
	if ( !IsCommandIssuedByServerAdmin() )
		return;

	if ( !*args[1] )
	{
#ifdef MAPBASE_VSCRIPT
		CGWarning( 0, CON_GROUP_VSCRIPT, "No script specified\n" );
#else
		Log_Warning( LOG_VScript, "No script specified\n" );
#endif
		return;
	}

	if ( !g_pScriptVM )
	{
#ifdef MAPBASE_VSCRIPT
		CGWarning( 0, CON_GROUP_VSCRIPT, "Scripting disabled or no server running\n" );
#else
		Log_Warning( LOG_VScript, "Scripting disabled or no server running\n" );
#endif
		return;
	}

	VScriptRunScript( args[1], true );
}

#ifdef CLIENT_DLL
CON_COMMAND( script_debug_client, "Connect the vscript VM to the script debugger" )
#else
CON_COMMAND( script_debug, "Connect the vscript VM to the script debugger" )
#endif
{
	if ( !IsCommandIssuedByServerAdmin() )
		return;

#ifdef MAPBASE_VSCRIPT
#ifdef GAME_DLL
	int port = 1212;
#else
	int port = 1213;
#endif
#endif

	if ( !g_pScriptVM )
	{
#ifdef MAPBASE_VSCRIPT
		vscript_debugger_port = port;
		CGMsg( 0, CON_GROUP_VSCRIPT, "VScript VM is not running, waiting for it to attach the debugger to port %d...\n", port );
#else
		Log_Warning( LOG_VScript, "Scripting disabled or no server running\n" );
#endif
		return;
	}

#ifdef MAPBASE_VSCRIPT
	g_pScriptVM->ConnectDebugger( port );
#else
	g_pScriptVM->ConnectDebugger();
#endif
}

#ifdef CLIENT_DLL
CON_COMMAND( script_help_client, "Output help for script functions, optionally with a search string" )
#else
CON_COMMAND( script_help, "Output help for script functions, optionally with a search string" )
#endif
{
	if ( !IsCommandIssuedByServerAdmin() )
		return;

	if ( !g_pScriptVM )
	{
#ifdef MAPBASE_VSCRIPT
		CGWarning( 0, CON_GROUP_VSCRIPT, "Scripting disabled or no server running\n" );
#else
		Log_Warning( LOG_VScript, "Scripting disabled or no server running\n" );
#endif
		return;
	}
	const char *pszArg1 = "*";
	if ( *args[1] )
	{
		pszArg1 = args[1];
	}

#ifdef MAPBASE_VSCRIPT
	g_pScriptVM->Run( CFmtStr( "__Documentation.PrintHelp( \"%s\" );", pszArg1 ) );
#else
	g_pScriptVM->Run( CFmtStr( "PrintHelp( \"%s\" );", pszArg1 ) );
#endif
}

#ifdef CLIENT_DLL
CON_COMMAND( script_dump_all_client, "Dump the state of the VM to the console" )
#else
CON_COMMAND( script_dump_all, "Dump the state of the VM to the console" )
#endif
{
	if ( !IsCommandIssuedByServerAdmin() )
		return;

	if ( !g_pScriptVM )
	{
#ifdef MAPBASE_VSCRIPT
		CGWarning( 0, CON_GROUP_VSCRIPT, "Scripting disabled or no server running\n" );
#else
		Log_Warning( LOG_VScript, "Scripting disabled or no server running\n" );
#endif
		return;
	}
	g_pScriptVM->DumpState();
}

//-----------------------------------------------------------------------------

#ifdef MAPBASE_VSCRIPT
void RunAddonScripts()
{
	char searchPaths[4096];
	filesystem->GetSearchPath( "ADDON", true, searchPaths, sizeof( searchPaths ) );

	for ( char *path = strtok( searchPaths, ";" ); path; path = strtok( NULL, ";" ) )
	{
		char folderName[MAX_PATH];
		Q_FileBase( path, folderName, sizeof( folderName ) );

		// mapspawn_addon
		char fullpath[MAX_PATH];
		Q_ComposeFileName( path, "scripts/vscripts/mapspawn_addon", fullpath, sizeof( fullpath ) );

		VScriptRunScriptAbsolute( fullpath, NULL, false, folderName );
	}
}

// UNDONE: "autorun" folder
/*
void RunAutorunScripts()
{
	FileFindHandle_t fileHandle;
	char szDirectory[MAX_PATH];
	char szFileName[MAX_PATH];
	char szPartialScriptPath[MAX_PATH];

	// TODO: Scanning for VM extension would make this more efficient
	Q_strncpy( szDirectory, "scripts/vscripts/autorun/*", sizeof( szDirectory ) );

	const char *pszScriptFile = filesystem->FindFirst( szDirectory, &fileHandle );
	while (pszScriptFile && fileHandle != FILESYSTEM_INVALID_FIND_HANDLE)
	{
		Q_FileBase( pszScriptFile, szFileName, sizeof( szFileName ) );
		Q_snprintf( szPartialScriptPath, sizeof( szPartialScriptPath ), "autorun/%s", szFileName );
		VScriptRunScript( szPartialScriptPath );

		pszScriptFile = filesystem->FindNext( fileHandle );
	}

	// Non-shared scripts
#ifdef CLIENT_DLL
	Q_strncpy( szDirectory, "scripts/vscripts/autorun/client/*", sizeof( szDirectory ) );
#else
	Q_strncpy( szDirectory, "scripts/vscripts/autorun/server/*", sizeof( szDirectory ) );
#endif

	pszScriptFile = filesystem->FindFirst( szDirectory, &fileHandle );
	while (pszScriptFile && fileHandle != FILESYSTEM_INVALID_FIND_HANDLE)
	{
		Q_FileBase( pszScriptFile, szFileName, sizeof( szFileName ) );
#ifdef CLIENT_DLL
		Q_snprintf( szPartialScriptPath, sizeof( szPartialScriptPath ), "autorun/client/%s", szFileName );
#else
		Q_snprintf( szPartialScriptPath, sizeof( szPartialScriptPath ), "autorun/server/%s", szFileName );
#endif
		VScriptRunScript( szPartialScriptPath );

		pszScriptFile = filesystem->FindNext( fileHandle );
	}
}
*/
#endif

//-----------------------------------------------------------------------------

static short VSCRIPT_SERVER_SAVE_RESTORE_VERSION = 3;

//-----------------------------------------------------------------------------

class CVScriptSaveRestoreBlockHandler : public CDefSaveRestoreBlockHandler
{
public:
	CVScriptSaveRestoreBlockHandler() :
		m_InstanceMap( DefLessFunc(const char *) )
	{
	}
	const char *GetBlockName()
	{
#ifdef CLIENT_DLL
		return "VScriptClient";
#else
		return "VScriptServer";
#endif
	}

	//---------------------------------

	void Save( ISave *pSave )
	{
		pSave->StartBlock();

		int temp = g_pScriptVM != NULL;
		pSave->WriteInt( &temp );
		if ( g_pScriptVM )
		{
			temp = g_pScriptVM->GetLanguage();
			pSave->WriteInt( &temp );
			CUtlBuffer buffer;
			g_pScriptVM->WriteState( &buffer );
			temp = buffer.TellPut();
			pSave->WriteInt( &temp );
			if ( temp > 0 )
			{
				pSave->WriteData( (const char *)buffer.Base(), temp );
			}
		}

		pSave->EndBlock();
	}

	//---------------------------------

	void WriteSaveHeaders( ISave *pSave )
	{
		pSave->WriteShort( &VSCRIPT_SERVER_SAVE_RESTORE_VERSION );
	}

	//---------------------------------

	void ReadRestoreHeaders( IRestore *pRestore )
	{
		// No reason why any future version shouldn't try to retain backward compatability. The default here is to not do so.
		short version;
		pRestore->ReadShort( &version );
		m_fDoLoad = ( version == VSCRIPT_SERVER_SAVE_RESTORE_VERSION );
	}

	//---------------------------------

	void Restore( IRestore *pRestore, bool createPlayers )
	{
		if ( !m_fDoLoad && g_pScriptVM )
		{
			return;
		}
#ifdef CLIENT_DLL
		C_BaseEntity *pEnt = ClientEntityList().FirstBaseEntity();
#else
		CBaseEntity *pEnt = gEntList.FirstEnt();
#endif
		while ( pEnt )
		{
			if ( pEnt->m_iszScriptId != NULL_STRING )
			{
#ifndef MAPBASE_VSCRIPT
				g_pScriptVM->RegisterClass( pEnt->GetScriptDesc() );
#endif
				m_InstanceMap.Insert( STRING( pEnt->m_iszScriptId ), pEnt );
			}
#ifdef CLIENT_DLL
			pEnt = ClientEntityList().NextBaseEntity( pEnt );
#else
			pEnt = gEntList.NextEnt( pEnt );
#endif
		}

		pRestore->StartBlock();
		if ( pRestore->ReadInt() && pRestore->ReadInt() == g_pScriptVM->GetLanguage() )
		{
			int nBytes = pRestore->ReadInt();
			if ( nBytes > 0 )
			{
				CUtlBuffer buffer;
				buffer.EnsureCapacity( nBytes );
				pRestore->ReadData( (char *)buffer.AccessForDirectRead( nBytes ), nBytes, 0 );
				g_pScriptVM->ReadState( &buffer );
			}
		}
		pRestore->EndBlock();
	}

	void PostRestore( void )
	{
		for ( int i = m_InstanceMap.FirstInorder(); i != m_InstanceMap.InvalidIndex(); i = m_InstanceMap.NextInorder( i ) )
		{
			CBaseEntity *pEnt = m_InstanceMap[i];
			if ( pEnt->m_hScriptInstance )
			{
				ScriptVariant_t variant;
				if ( g_pScriptVM->GetValue( STRING(pEnt->m_iszScriptId), &variant ) && variant.GetType() == FIELD_HSCRIPT)
				{
					pEnt->m_ScriptScope.Init( variant.Get<HSCRIPT>(), false);
#ifdef MAPBASE_VSCRIPT
					g_pScriptVM->SetValue( pEnt->m_ScriptScope, "self", pEnt->m_hScriptInstance );
#endif
#ifndef CLIENT_DLL
					pEnt->RunPrecacheScripts();
#endif
				}
			}
			else
			{
				// Script system probably has no internal references
				pEnt->m_iszScriptId = NULL_STRING;
			}
		}
		m_InstanceMap.Purge();

#ifdef MAPBASE_VSCRIPT
		GetScriptHookManager().OnRestore();
#endif

#if defined(MAPBASE_VSCRIPT) && defined(CLIENT_DLL)
		VScriptSaveRestoreUtil_OnVMRestore();
#endif
	}


	CUtlMap<const char *, CBaseEntity *> m_InstanceMap;

private:
	bool m_fDoLoad;
};

//-----------------------------------------------------------------------------

CVScriptSaveRestoreBlockHandler g_VScriptSaveRestoreBlockHandler;

//-------------------------------------

ISaveRestoreBlockHandler *GetVScriptSaveRestoreBlockHandler()
{
	return &g_VScriptSaveRestoreBlockHandler;
}

//-----------------------------------------------------------------------------

bool CBaseEntityScriptInstanceHelper::ToString( void *p, char *pBuf, int bufSize )	
{
	CBaseEntity *pEntity = (CBaseEntity *)p;
#ifdef CLIENT_DLL
	if ( pEntity->GetEntityName() && pEntity->GetEntityName()[0] )
#else
	if ( pEntity->GetEntityName() != NULL_STRING )
#endif
	{
		V_snprintf( pBuf, bufSize, "([%d] %s: %s)", pEntity->entindex(), pEntity->GetClassname(), STRING( pEntity->GetEntityName() ) );
	}
	else
	{
		V_snprintf( pBuf, bufSize, "([%d] %s)", pEntity->entindex(), pEntity->GetClassname() );
	}
	return true; 
}

void *CBaseEntityScriptInstanceHelper::BindOnRead( HSCRIPT hInstance, void *pOld, const char *pszId )
{
	int iEntity = g_VScriptSaveRestoreBlockHandler.m_InstanceMap.Find( pszId );
	if ( iEntity != g_VScriptSaveRestoreBlockHandler.m_InstanceMap.InvalidIndex() )
	{
		CBaseEntity *pEnt = g_VScriptSaveRestoreBlockHandler.m_InstanceMap[iEntity];
		pEnt->m_hScriptInstance = hInstance;
		return pEnt;
	}
	return NULL;
}


CBaseEntityScriptInstanceHelper g_BaseEntityScriptInstanceHelper;
