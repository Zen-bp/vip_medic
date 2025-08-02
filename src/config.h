#pragma once

class IFileSystem;

class CConfig
{
public:
	CConfig();
	bool LoadConfig(IFileSystem* filesystem, const char* pszPath, char *error, unsigned long maxlen);
	inline float GetHealDelay() const { return m_flHealDelay; }
private:
	float m_flHealDelay;
};
