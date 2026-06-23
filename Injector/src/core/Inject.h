#pragma once
#include <QString>
#include <windows.h>

class Inject
{
public:
	static DWORD GetProcId(const char* procName);
	static bool inject(DWORD processId, const QString& dllPath, HANDLE& hModule);
	~Inject();
	static bool unload();
	static bool isProcessRunning(const char* procName);
	static bool setDllPermissions(const QString& dllPath);

	static bool m_isInjected;
	static HANDLE m_hInjectedModule;
private:
	static QString getFullDllPath(const QString& dllPath);
	static HANDLE OpenProcessWithNtApi(DWORD processId);
	static void CleanupTempFiles();

	static std::wstring m_currentDllPath;
	static std::wstring m_currentTempDir;
};