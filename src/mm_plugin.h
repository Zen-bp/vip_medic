#pragma once

#include <ISmmPlugin.h>
#include <sh_vector.h>
#include <iserver.h>
#include <entity2/entitysystem.h>
#include <KeyValues.h>
#include "CGameRules.h"
class CBasePlayerController;
#include "CCSPlayerController.h"
#include "igameeventsystem.h"
#include "include/menus.h"
#include "include/vip.h"
#include "config.h"

class CMMPlugin : public ISmmPlugin
{
public:
	bool Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late);
	bool Unload(char *error, size_t maxlen);
	void AllPluginsLoaded();
public:
	const char *GetAuthor();
	const char *GetName();
	const char *GetDescription();
	const char *GetURL();
	const char *GetLicense();
	const char *GetVersion();
	const char *GetDate();
	const char *GetLogTag();
};

extern CMMPlugin g_MMPlugin;
PLUGIN_GLOBALVARS();
