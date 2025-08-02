#include <stdio.h>
#include "mm_plugin.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"

#define MAXPLAYERS 64

CMMPlugin g_MMPlugin;
PLUGIN_EXPOSE(CMMPlugin, g_MMPlugin);

IVEngineServer2* g_pEngineServer2 = nullptr;
IVIPApi* g_pVIPCore = nullptr;
IUtilsApi* g_pUtils = nullptr;
CConfig* g_pConfig = nullptr;

const char* g_pszFeature = "medic";
const char* g_pszFeatureLimit = "medic_limit";
const char* g_pszFeatureDelay = "medic_delay";
const char* g_pszFeatureHp = "medic_hp";

int g_iUseCount[MAXPLAYERS]{};
float g_fLastUse[MAXPLAYERS]{};
CTimer* g_pHealTimers[MAXPLAYERS]{};

CGameEntitySystem* GameEntitySystem()
{
    return g_pUtils->GetCGameEntitySystem();
};

bool CMMPlugin::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pEngineServer2, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetServerFactory, g_pSource2Server, ISource2Server, SOURCE2SERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, g_pNetworkServerService, INetworkServerService, NETWORKSERVERSERVICE_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);

	g_pConfig = new CConfig();
	if(!g_pConfig->LoadConfig(g_pFullFileSystem, "addons/configs/vip/vip_medic.cfg", error, maxlen))
	{
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s", GetLogTag(), error);
		return false;
	}

	return true;
}

bool CMMPlugin::Unload(char *error, size_t maxlen)
{
	if(g_pUtils)
		g_pUtils->ClearAllHooks(g_PLID);

	if(g_pConfig)
		delete g_pConfig;

	return true;
}

void Medic(int iSlot)
{
	CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
	if(!pController || !pController->IsConnected())
		return;
	
	if(!g_pVIPCore->VIP_IsClientVIP(iSlot) || !g_pVIPCore->VIP_GetClientFeatureBool(iSlot, g_pszFeature))
	{
		g_pUtils->PrintToChat(iSlot, g_pVIPCore->VIP_GetTranslate("NotAccess"));
		return;
	}
	
	CCSPlayerPawn* pPlayerPawn = pController->m_hPlayerPawn().Get();
	if(!pPlayerPawn)
		return;
	
	if(pController->m_iTeamNum() < 2)
	{
		g_pUtils->PrintToChat(iSlot, g_pVIPCore->VIP_GetTranslate("medic_must_be_in_team"));
		return;
	}
	
	if(pPlayerPawn->m_lifeState() != LIFE_ALIVE)
	{
		g_pUtils->PrintToChat(iSlot, g_pVIPCore->VIP_GetTranslate("medic_must_be_alive"));
		return;
	}

	if(g_pHealTimers[iSlot])
	{
		g_pUtils->PrintToChat(iSlot, g_pVIPCore->VIP_GetTranslate("medic_heal_in_process"), g_fLastUse[iSlot] + g_pConfig->GetHealDelay() - g_pUtils->GetCGlobalVars()->curtime);
		return;
	}

	int limit = g_pVIPCore->VIP_GetClientFeatureInt(iSlot, g_pszFeatureLimit);
	if(limit && limit <= g_iUseCount[iSlot])
	{
		g_pUtils->PrintToChat(iSlot, g_pVIPCore->VIP_GetTranslate("medic_limit_reached"), limit);
		return;
	}

	int health = pPlayerPawn->m_iHealth();
	if(health >= 100)
	{
		g_pUtils->PrintToChat(iSlot, g_pVIPCore->VIP_GetTranslate("medic_max_health"));
		return;
	}

	float flDelay = g_pVIPCore->VIP_GetClientFeatureFloat(iSlot, g_pszFeatureDelay);
	if(g_fLastUse[iSlot] > 0.0 && g_fLastUse[iSlot] + flDelay > g_pUtils->GetCGlobalVars()->curtime)
	{
		g_pUtils->PrintToChat(iSlot, g_pVIPCore->VIP_GetTranslate("medic_delay"), g_fLastUse[iSlot] + flDelay - g_pUtils->GetCGlobalVars()->curtime);
		return;
	}

	g_pHealTimers[iSlot] = g_pUtils->CreateTimer(g_pConfig->GetHealDelay(), [hController = CHandle<CCSPlayerController>(pController), iSlot]()
	{
		g_pHealTimers[iSlot] = nullptr;

		CCSPlayerController* pController = hController.Get();
		if(!pController || !pController->IsConnected())
			return -1.0f;
		
		CCSPlayerPawn* pPlayerPawn = pController->m_hPlayerPawn().Get();
		if(!pPlayerPawn)
			return -1.0f;
		
		if(pController->m_iTeamNum() < 2 || pPlayerPawn->m_lifeState() != LIFE_ALIVE)
			return -1.0f;
		
		int heal = g_pVIPCore->VIP_GetClientFeatureInt(iSlot, g_pszFeatureHp);
		if(!heal)
			heal = 50;
		
		pPlayerPawn->m_iHealth() = Min(pPlayerPawn->m_iHealth() + heal, 100);
		g_pUtils->SetStateChanged(pPlayerPawn, "CBaseEntity", "m_iHealth");

		pPlayerPawn->m_flHealthShotBoostExpirationTime() = g_pUtils->GetCGlobalVars()->curtime + 2.0;
		g_pUtils->SetStateChanged(pPlayerPawn, "CCSPlayerPawn", "m_flHealthShotBoostExpirationTime");

		return -1.0f;
	});

	++g_iUseCount[iSlot];
	g_fLastUse[iSlot] = g_pUtils->GetCGlobalVars()->curtime;

	g_pUtils->PrintToChat(iSlot, g_pVIPCore->VIP_GetTranslate("medic_start_heal"));
}

bool OnSelect(int iSlot, const char* szFeature)
{
	Medic(iSlot);
	return false;
}

std::string OnDisplay(int iSlot, const char* szFeature)
{
	const char* szName = g_pVIPCore->VIP_GetTranslate("medic");
	if(!g_pVIPCore->VIP_IsClientVIP(iSlot) || !g_pVIPCore->VIP_GetClientFeatureBool(iSlot, g_pszFeature))
		return std::string(szName) + " [" + g_pVIPCore->VIP_GetTranslate("medic_noaccess") + "]";
	
	int limit = g_pVIPCore->VIP_GetClientFeatureInt(iSlot, g_pszFeatureLimit);
	if(!limit)
		return std::string(szName);

	return std::string(szName) + " [" + std::to_string(g_iUseCount[iSlot]) + "/" + std::to_string(limit) + "]";
}

void OnRoundPreStart(const char* szName, IGameEvent* pEvent, bool bDontBroadcast)
{
	for(int i = 0; i < MAXPLAYERS; ++i)
	{
		if(g_pHealTimers[i])
		{
			g_pUtils->RemoveTimer(g_pHealTimers[i]);
			g_pHealTimers[i] = nullptr;
		}
	}
	for(int slot = 0; slot < g_pUtils->GetCGlobalVars()->maxClients; ++slot)
	{
		g_iUseCount[slot] = 0;
		g_fLastUse[slot] = 0.0;
		g_pHealTimers[slot] = nullptr;
	}
}

void OnPlayerDeath(const char* szName, IGameEvent* pEvent, bool bDontBroadcast)
{
	int iSlot = pEvent->GetInt("userid");
	CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
	if(!pController)
		return;
	
	if(g_pHealTimers[iSlot])
	{
		g_pUtils->RemoveTimer(g_pHealTimers[iSlot]);
		g_pHealTimers[iSlot] = nullptr;
	}
}

bool Cmd_Medic(int iSlot, const char* szContent)
{
	Medic(iSlot);
	return false;
}

void CMMPlugin::AllPluginsLoaded()
{
	int ret;
	g_pVIPCore = (IVIPApi*)g_SMAPI->MetaFactory(VIP_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		ConColorMsg(Color(255, 0, 0, 255), "[%s] Failed to lookup vip core. Aborting\n", GetLogTag());
		std::string sBuffer = "meta unload " + std::to_string(g_PLID);
		g_pEngineServer2->ServerCommand(sBuffer.c_str());
		return;
	}

	g_pUtils = (IUtilsApi*)g_SMAPI->MetaFactory(Utils_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		ConColorMsg(Color(255, 0, 0, 255), "[%s] Failed to lookup utils api. Aborting\n", GetLogTag());
		std::string sBuffer = "meta unload " + std::to_string(g_PLID);
		g_pEngineServer2->ServerCommand(sBuffer.c_str());
		return;
	}
	
	g_pVIPCore->VIP_RegisterFeature(g_pszFeature, VIP_BOOL, SELECTABLE, OnSelect, nullptr, OnDisplay);
	g_pVIPCore->VIP_RegisterFeature(g_pszFeatureLimit, VIP_INT, HIDE);
	g_pVIPCore->VIP_RegisterFeature(g_pszFeatureDelay, VIP_FLOAT, HIDE);
	g_pVIPCore->VIP_RegisterFeature(g_pszFeatureHp, VIP_INT, HIDE);

	g_pUtils->HookEvent(g_PLID, "round_prestart", OnRoundPreStart);
	g_pUtils->HookEvent(g_PLID, "player_death", OnPlayerDeath);
	g_pUtils->RegCommand(g_PLID, { "m"}, { "!m" }, Cmd_Medic);
}

const char *CMMPlugin::GetAuthor()
{
	return "RageM6nkey";
}

const char *CMMPlugin::GetName()
{
	return "[VIP] Medic!";
}

const char *CMMPlugin::GetDescription()
{
	return "Call a medic to heal you";
}

const char *CMMPlugin::GetURL()
{
	return "discord: ragem6nkey";
}

const char *CMMPlugin::GetLicense()
{
	return "Private License";
}

const char *CMMPlugin::GetVersion()
{
	return "1.0.0";
}

const char *CMMPlugin::GetDate()
{
	return __DATE__;
}

const char *CMMPlugin::GetLogTag()
{
	return "VIP Medic!";
}
