//========= Copyright � Valve LLC, All rights reserved. =======================
//
// Purpose:		
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "dod/dod_team.h"
#include "dod/dod_gamerules.h"
#include "tf_bot_manager.h"
#include "tf_bot.h"
#include "tier3/tier3.h"
#include "vgui/ILocalize.h"
#include "fmtstr.h"


extern ConVar tf_bot_difficulty;
extern ConVar tf_bot_prefix_name_with_difficulty;

ConVar tf_bot_quota("tf_bot_quota", "0", FCVAR_NONE, "Determines the total number of TF bots in the game.");
ConVar tf_bot_quota_mode("tf_bot_quota_mode", "normal", FCVAR_NONE, "Determines the type of quota. Allowed values: 'normal', 'fill', and 'match'. If 'fill', the server will adjust bots to keep N players in the game, where N is bot_quota. If 'match', the server will maintain a 1:N ratio of humans to bots, where N is bot_quota.");
ConVar tf_bot_join_after_player("tf_bot_join_after_player", "1", FCVAR_NONE, "If nonzero, bots wait until a player joins before entering the game.", true, 0.0f, true, 1.0f);
ConVar tf_bot_auto_vacate("tf_bot_auto_vacate", "1", FCVAR_NONE, "If nonzero, bots will automatically leave to make room for human players.", true, 0.0f, true, 1.0f);
ConVar tf_bot_offline_practice("tf_bot_offline_practice", "0", FCVAR_NONE, "Tells the server that it is in offline practice mode.", true, 0.0f, true, 1.0f);
ConVar tf_bot_melee_only("tf_bot_melee_only", "0", FCVAR_GAMEDLL, "If nonzero, TFBots will only use melee weapons", true, 0.0f, true, 1.0f);

// this whole function seems unnecessary,
// why not have them in a database that can be changed be the end user,
// and why do a loop to figure out how big you made this list,
// and this isn't even random, it starts at a random spot, but goes linearly thereafter
const char* GetRandomBotName(void)
{
	static const char* const nameList[] = {
		"Chucklenuts",
		"CryBaby",
		"WITCH",
		"ThatGuy",
		"Still Alive",
		"Hat-Wearing MAN",
		"Me",
		"Numnutz",
		"H@XX0RZ",
		"The G-Man",
		"Chell",
		"The Combine",
		"Totally Not A Bot",
		"Pow!",
		"Zepheniah Mann",
		"THEM",
		"LOS LOS LOS",
		"10001011101",
		"DeadHead",
		"ZAWMBEEZ",
		"MindlessElectrons",
		"TAAAAANK!",
		"The Freeman",
		"Black Mesa",
		"Soulless",
		"CEDA",
		"BeepBeepBoop",
		"NotMe",
		"CreditToTeam",
		"BoomerBile",
		"Someone Else",
		"Mann Co.",
		"Dog",
		"Kaboom!",
		"AmNot",
		"0xDEADBEEF",
		"HI THERE",
		"SomeDude",
		"GLaDOS",
		"Hostage",
		"Headful of Eyeballs",
		"CrySomeMore",
		"Aperture Science Prototype XR7",
		"Humans Are Weak",
		"AimBot",
		"C++",
		"GutsAndGlory!",
		"Nobody",
		"Saxton Hale",
		"RageQuit",
		"Screamin' Eagles",
		"Ze Ubermensch",
		"Maggot",
		"CRITRAWKETS",
		"Herr Doktor",
		"Gentlemanne of Leisure",
		"Companion Cube",
		"Target Practice",
		"One-Man Cheeseburger Apocalypse",
		"Crowbar",
		"Delicious Cake",
		"IvanTheSpaceBiker",
		"I LIVE!",
		"Cannon Fodder",
		"trigger_hurt",
		"Nom Nom Nom",
		"Divide by Zero",
		"GENTLE MANNE of LEISURE",
		"MoreGun",
		"Tiny Baby Man",
		"Big Mean Muther Hubbard",
		"Force of Nature",
		"Crazed Gunman",
		"Grim Bloody Fable",
		"Poopy Joe",
		"A Professional With Standards",
		"Freakin' Unbelievable",
		"SMELLY UNFORTUNATE",
		"The Administrator",
		"Mentlegen",
		"Archimedes!",
		"Ribs Grow Back",
		"It's Filthy in There!",
		"Mega Baboon",
		"Kill Me",
		"Glorified Toaster with Legs"
	};
	static int nameCount = 0;
	static int nameIndex = 0;

	if (nameCount == 0)
	{
		/*for (int i=0; nameList[i]; ++i)
			++nameCount;*/
		nameCount = ARRAYSIZE(nameList);
		nameIndex = RandomInt(0, nameCount);
	}

	const char* name = nameList[nameIndex];

	++nameIndex;
	if (nameIndex >= nameCount)
		nameIndex = 0;

	return name;
}

bool UTIL_KickBotFromTeam(int teamNum)
{
	// a ForEachPlayer functor goes here

	// find a dead guy first, so we don't disrupt combat
	for (int i = 1; i < gpGlobals->maxClients; ++i)
	{
		CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);
		if (pPlayer == nullptr || !pPlayer->IsPlayer() || !pPlayer->IsConnected())
			continue;

		CTFBot* pBot = ToTFBot(pPlayer);
		if (pBot == nullptr)
			continue;

		if (pPlayer->GetTeamNumber() != teamNum || pPlayer->IsAlive())
			continue;

		engine->ServerCommand(UTIL_VarArgs("kickid %d\n", engine->GetPlayerUserId(pPlayer->edict())));
		return true;
	}

	// go with anyone otherwise
	for (int i = 1; i < gpGlobals->maxClients; ++i)
	{
		CBasePlayer* pPlayer = UTIL_PlayerByIndex(i);
		if (pPlayer == nullptr || !pPlayer->IsPlayer() || !pPlayer->IsConnected())
			continue;

		CTFBot* pBot = ToTFBot(pPlayer);
		if (pBot == nullptr)
			continue;

		if (pPlayer->GetTeamNumber() != teamNum)
			continue;

		engine->ServerCommand(UTIL_VarArgs("kickid %d\n", engine->GetPlayerUserId(pPlayer->edict())));
		return true;
	}

	return false;
}

const char* DifficultyToName(int iSkillLevel)
{
	switch (iSkillLevel)
	{
	case CTFBot::EASY:
		return "Easy ";
	case CTFBot::NORMAL:
		return "Normal ";
	case CTFBot::HARD:
		return "Hard ";
	case CTFBot::EXPERT:
		return "Expert ";
	}

	return "Undefined ";
}
int NameToDifficulty(const char* pszSkillName)
{
	if (!Q_stricmp(pszSkillName, "expert"))
		return CTFBot::EXPERT;
	else if (!Q_stricmp(pszSkillName, "hard"))
		return CTFBot::HARD;
	else if (!Q_stricmp(pszSkillName, "normal"))
		return CTFBot::NORMAL;
	else if (!Q_stricmp(pszSkillName, "easy"))
		return CTFBot::EASY;

	return -1;
}

void CreateBotName(int iTeamNum, int iClassIdx, int iSkillLevel, char* out, int outlen)
{
	char szName[64] = "", szRelationship[64] = "", szDifficulty[32] = "";
	Q_strncpy(szName, TheTFBots().GetRandomBotName(), sizeof(szName));

	if (tf_bot_prefix_name_with_difficulty.GetBool())
	{
		Q_strncpy(szDifficulty, DifficultyToName(iSkillLevel), sizeof(szDifficulty));
	}

	Q_strncpy(out, CFmtStr("%s%s%s", szDifficulty, szRelationship, szName), outlen);
}


CTFBotManager sTFBotManager;
CTFBotManager& TheTFBots()
{
	return sTFBotManager;
}


CTFBotManager::CTFBotManager()
{
	NextBotManager::SetInstance(this);

	m_flQuotaChangeTime = 0.0f;
}

CTFBotManager::~CTFBotManager()
{
	NextBotManager::SetInstance(NULL);
}


void CTFBotManager::Update()
{
	MaintainBotQuota();

	NextBotManager::Update();
}


void CTFBotManager::OnMapLoaded()
{
	ReloadBotNames();

	NextBotManager::OnMapLoaded();
}

void CTFBotManager::OnRoundRestart()
{
	NextBotManager::OnRoundRestart();
}

void CTFBotManager::OnLevelShutdown()
{
	m_BotNames.RemoveAll();
	m_flQuotaChangeTime = 0.0f;

	if (IsInOfflinePractice())
		RevertOfflinePracticeConvars();
}


bool CTFBotManager::IsInOfflinePractice() const
{
	return tf_bot_offline_practice.GetBool();
}

void CTFBotManager::SetIsInOfflinePractice(bool set)
{
	tf_bot_offline_practice.SetValue(set);
}

void CTFBotManager::RevertOfflinePracticeConvars()
{
	tf_bot_quota.Revert();
	tf_bot_quota_mode.Revert();
	tf_bot_auto_vacate.Revert();
	tf_bot_difficulty.Revert();
	tf_bot_offline_practice.Revert();
}


void CTFBotManager::OnForceAddedBots(int count)
{
	tf_bot_quota.SetValue(Min(gpGlobals->maxClients, count + tf_bot_quota.GetInt()));
	m_flQuotaChangeTime = gpGlobals->curtime + 1.0f;
}

void CTFBotManager::OnForceKickedBots(int count)
{
	tf_bot_quota.SetValue(Max(0, tf_bot_quota.GetInt() - count));
	m_flQuotaChangeTime = gpGlobals->curtime + 2.0f;
}

bool CTFBotManager::IsMeleeOnly() const
{
	return tf_bot_melee_only.GetBool();
}

//----------------------------------------------------------------------------------------------------------------
CTFBot* CTFBotManager::GetAvailableBotFromPool()
{
	for (int i = 1; i <= gpGlobals->maxClients; ++i)
	{
		CDODPlayer* pPlayer = ToDODPlayer(UTIL_PlayerByIndex(i));
		CTFBot* pBot = dynamic_cast<CTFBot*>(pPlayer);

		if (pBot == NULL)
			continue;

		if ((pBot->GetFlags() & FL_FAKECLIENT) == 0)
			continue;

		if (pBot->GetTeamNumber() == TEAM_SPECTATOR || pBot->GetTeamNumber() == TEAM_UNASSIGNED)
		{
			return pBot;
		}
	}
	return NULL;
}


void CTFBotManager::MaintainBotQuota()
{
	if (TheNavMesh->IsGenerating())
		return;

	if (g_fGameOver)
		return;

	// new players can't spawn immediately after the round has been going for some time
	if (!DODGameRules())
		return;


	// if it is not time to do anything...
	if (gpGlobals->curtime < m_flQuotaChangeTime)
		return;

	// think every quarter second
	m_flQuotaChangeTime = gpGlobals->curtime + 0.25f;

	// don't add bots until local player has been registered, to make sure he's player ID #1
	if (!engine->IsDedicatedServer())
	{
		CBasePlayer* pPlayer = UTIL_GetListenServerHost();
		if (!pPlayer)
			return;
	}

	// We want to balance based on who's playing on game teams not necessary who's on team spectator, etc.
	int nConnectedClients = 0;
	int nTFBots = 0;
	int nTFBotsOnGameTeams = 0;
	int nNonTFBotsOnGameTeams = 0;
	int nSpectators = 0;
	for (int i = 1; i <= gpGlobals->maxClients; ++i)
	{
		CDODPlayer* pPlayer = ToDODPlayer(UTIL_PlayerByIndex(i));

		if (pPlayer == NULL)
			continue;

		if (FNullEnt(pPlayer->edict()))
			continue;

		if (!pPlayer->IsConnected())
			continue;

		CTFBot* pBot = dynamic_cast<CTFBot*>(pPlayer);
		if (pBot)
		{
			nTFBots++;
			if (pPlayer->GetTeamNumber() == TEAM_ALLIES || pPlayer->GetTeamNumber() == TEAM_AXIS)
			{
				nTFBotsOnGameTeams++;
			}
		}
		else
		{
			if (pPlayer->GetTeamNumber() == TEAM_ALLIES || pPlayer->GetTeamNumber() == TEAM_AXIS)
			{
				nNonTFBotsOnGameTeams++;
			}
			else if (pPlayer->GetTeamNumber() == TEAM_SPECTATOR)
			{
				nSpectators++;
			}
		}

		nConnectedClients++;
	}

	int desiredBotCount = tf_bot_quota.GetInt();
	int nTotalNonTFBots = nConnectedClients - nTFBots;

	if (FStrEq(tf_bot_quota_mode.GetString(), "fill"))
	{
		desiredBotCount = MAX(0, desiredBotCount - nNonTFBotsOnGameTeams);
	}
	else if (FStrEq(tf_bot_quota_mode.GetString(), "match"))
	{
		// If bot_quota_mode is 'match', we want the number of bots to be bot_quota * total humans
		desiredBotCount = (int)MAX(0, tf_bot_quota.GetFloat() * nNonTFBotsOnGameTeams);
	}

	// wait for a player to join, if necessary
	if (tf_bot_join_after_player.GetBool())
	{
		if ((nNonTFBotsOnGameTeams == 0) && (nSpectators == 0))
		{
			desiredBotCount = 0;
		}
	}

	// if bots will auto-vacate, we need to keep one slot open to allow players to join
	if (tf_bot_auto_vacate.GetBool())
	{
		desiredBotCount = MIN(desiredBotCount, gpGlobals->maxClients - nTotalNonTFBots - 1);
	}
	else
	{
		desiredBotCount = MIN(desiredBotCount, gpGlobals->maxClients - nTotalNonTFBots);
	}

	// add bots if necessary
	if (desiredBotCount > nTFBotsOnGameTeams)
	{
		// don't try to add a bot if it would unbalance
		if (!DODGameRules()->WouldChangeUnbalanceTeams(TEAM_AXIS, TEAM_UNASSIGNED) ||
			!DODGameRules()->WouldChangeUnbalanceTeams(TEAM_ALLIES, TEAM_UNASSIGNED))
		{
			CTFBot* pBot = GetAvailableBotFromPool();
			if (pBot == NULL)
			{
				char szBotName[64];
				V_strcpy_safe(szBotName, TheTFBots().GetRandomBotName());

				CTFBot* pBot = NextBotCreatePlayerBot<CTFBot>(szBotName);
			}
			if (pBot)
			{
				pBot->HandleCommand_JoinTeam(DODGameRules()->SelectDefaultTeam());

				pBot->HandleCommand_JoinClass(RandomInt(1, 5));

				// give the bot a proper name
				char name[256];
				CTFBot::DifficultyType skill = pBot->m_iSkill;
				CreateBotName(pBot->GetTeamNumber(), pBot->m_Shared.PlayerClass(), skill, name, sizeof(name));
				engine->SetFakeClientConVarValue(pBot->edict(), "name", name);
			}
		}
	}
	else if (desiredBotCount < nTFBotsOnGameTeams)
	{
		// kick a bot to maintain quota

		// first remove any unassigned bots
		if (UTIL_KickBotFromTeam(TEAM_UNASSIGNED))
			return;

		int kickTeam;

		CTeam* pRed = GetGlobalTeam(TEAM_ALLIES);
		CTeam* pBlue = GetGlobalTeam(TEAM_AXIS);

		// remove from the team that has more players
		if (pBlue->GetNumPlayers() > pRed->GetNumPlayers())
		{
			kickTeam = TEAM_AXIS;
		}
		else if (pBlue->GetNumPlayers() < pRed->GetNumPlayers())
		{
			kickTeam = TEAM_ALLIES;
		}
		// remove from the team that's winning
		else if (pBlue->GetScore() > pRed->GetScore())
		{
			kickTeam = TEAM_AXIS;
		}
		else if (pBlue->GetScore() < pRed->GetScore())
		{
			kickTeam = TEAM_ALLIES;
		}
		else
		{
			// teams and scores are equal, pick a team at random
			kickTeam = (RandomInt(0, 1) == 0) ? TEAM_AXIS : TEAM_ALLIES;
		}

		// attempt to kick a bot from the given team
		if (UTIL_KickBotFromTeam(kickTeam))
			return;

		// if there were no bots on the team, kick a bot from the other team
		UTIL_KickBotFromTeam(kickTeam == TEAM_AXIS ? TEAM_ALLIES : TEAM_AXIS);
	}

}

const char* CTFBotManager::GetRandomBotName()
{
	static char szName[64];
	if (m_BotNames.Count() == 0)
		return ::GetRandomBotName();

	static int nameIndex = RandomInt(0, m_BotNames.Count() - 1);
	string_t iszName = m_BotNames[++nameIndex % m_BotNames.Count()];
	const char* pszName = STRING(iszName);
	V_strcpy_safe(szName, pszName);

	return szName;
}

void CTFBotManager::ReloadBotNames()
{
	m_BotNames.RemoveAll();
	LoadBotNames();
}

#define BOT_NAMES_FILE	"scripts/tf_bot_names.txt"
bool CTFBotManager::LoadBotNames()
{
	VPROF_BUDGET(__FUNCTION__, VPROF_BUDGETGROUP_OTHER_FILESYSTEM);

	if (g_pFullFileSystem == nullptr)
		return false;

	KeyValues* pBotNames = new KeyValues("BotNames");
	if (!pBotNames->LoadFromFile(g_pFullFileSystem, BOT_NAMES_FILE, "MOD"))
	{
		Warning("CTFBotManager: Could not load %s", BOT_NAMES_FILE);
		pBotNames->deleteThis();
		return false;
	}

	FOR_EACH_VALUE(pBotNames, pSubData)
	{
		if (FStrEq(pSubData->GetString(), ""))
			continue;

		string_t iName = AllocPooledString(pSubData->GetString());
		if (m_BotNames.Find(iName) == m_BotNames.InvalidIndex())
			m_BotNames[m_BotNames.AddToTail()] = iName;
	}

	pBotNames->deleteThis();
	return true;
}

void CC_ReloadBotNames(const CCommand& args)
{
	TheTFBots().ReloadBotNames();
}

ConCommand tf_bot_names_reload("tf_bot_names_reload", CC_ReloadBotNames, "Reload all names for TFBots.", FCVAR_CHEAT);