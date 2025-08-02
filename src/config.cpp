#include "config.h"
#include <KeyValues.h>

CConfig::CConfig() : m_flHealDelay(0.0f) {}

bool CConfig::LoadConfig(IFileSystem *filesystem, const char *pszPath, char *error, unsigned long maxlen)
{
	KeyValues *pKv = new KeyValues("cfg");
	KeyValues::AutoDelete autoDelete(pKv);

	if(!pKv->LoadFromFile(filesystem, pszPath))
	{
		V_snprintf(error, maxlen, "Failed to load config: %s", pszPath);
		return false;
	}

	m_flHealDelay = pKv->GetFloat("heal_delay", 2.0);

	return true;
}
