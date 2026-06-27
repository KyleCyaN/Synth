#include "Inject.h"
#include <windows.h>
#include <tlhelp32.h>
#include <Aclapi.h>
#include <sddl.h>
#include <string>
#include <QDir>
#include <QFileInfo>
#include <fstream>
#include <QApplication>
#include <algorithm>

#include "../../../Shared/shared.h"

bool Inject::m_isInjected = false;
std::wstring Inject::m_currentDllPath;
std::wstring Inject::m_currentTempDir;
HANDLE Inject::m_hInjectedModule = NULL;

void Log(const char* msg)
{
    OutputDebugStringA(msg);
}

bool SetAccessControl(std::wstring& ExecutableName, const wchar_t* AccessString)
{
    PSECURITY_DESCRIPTOR SecurityDescriptor = nullptr;
    EXPLICIT_ACCESSW ExplicitAccess = {0};
    ACL* AccessControlCurrent = nullptr;
    ACL* AccessControlNew = nullptr;
    SECURITY_INFORMATION SecurityInfo = DACL_SECURITY_INFORMATION;
    PSID SecurityIdentifier = nullptr;

    if (GetNamedSecurityInfoW(
        ExecutableName.c_str(),
        SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION,
        nullptr,
        nullptr,
        &AccessControlCurrent,
        nullptr,
        &SecurityDescriptor
    ) == ERROR_SUCCESS)
    {
        ConvertStringSidToSidW(AccessString, &SecurityIdentifier);
        if (SecurityIdentifier != nullptr)
        {
            ExplicitAccess.grfAccessPermissions = GENERIC_READ | GENERIC_EXECUTE;
            ExplicitAccess.grfAccessMode = SET_ACCESS;
            ExplicitAccess.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
            ExplicitAccess.Trustee.TrusteeForm = TRUSTEE_IS_SID;
            ExplicitAccess.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
            ExplicitAccess.Trustee.ptstrName = static_cast<wchar_t*>(SecurityIdentifier);

            if (SetEntriesInAclW(1, &ExplicitAccess, AccessControlCurrent, &AccessControlNew) == ERROR_SUCCESS)
            {
                SetNamedSecurityInfoW(
                    const_cast<wchar_t*>(ExecutableName.c_str()),
                    SE_FILE_OBJECT,
                    SecurityInfo,
                    nullptr,
                    nullptr,
                    AccessControlNew,
                    nullptr
                );
            }
            LocalFree(SecurityIdentifier);
        }
    }
    if (SecurityDescriptor)
        LocalFree(SecurityDescriptor);
    if (AccessControlNew)
        LocalFree(AccessControlNew);

    return true;
}

void Inject::CleanupTempFiles()
{
    if (!m_currentDllPath.empty())
    {
        if (!DeleteFileW(m_currentDllPath.c_str()))
        {
            MoveFileExW(m_currentDllPath.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
        }
    }
    if (!m_currentTempDir.empty())
    {
        for (int retry = 0; retry < 3; retry++)
        {
            if (RemoveDirectoryW(m_currentTempDir.c_str()))
            {
                break;
            }
            Sleep(200);
        }
        if (GetFileAttributesW(m_currentTempDir.c_str()) != INVALID_FILE_ATTRIBUTES)
        {
            MoveFileExW(m_currentTempDir.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
        }
    }
    m_currentDllPath.clear();
    m_currentTempDir.clear();
}

DWORD Inject::GetProcId(const char* procName)
{
    DWORD procId = 0;
    const HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32 procEntry;
        procEntry.dwSize = sizeof(procEntry);

        if (Process32First(hSnap, &procEntry))
        {
            do
            {
                char currentProcName[MAX_PATH];
                wcstombs(currentProcName, procEntry.szExeFile, MAX_PATH);

                if (!_stricmp(currentProcName, procName))
                {
                    procId = procEntry.th32ProcessID;
                    break;
                }
            }
            while (Process32Next(hSnap, &procEntry));
        }
    }
    CloseHandle(hSnap);
    return procId;
}

QString Inject::getFullDllPath(const QString& dllPath)
{
    const QFileInfo fileInfo(dllPath);
    if (fileInfo.isRelative())
        return QDir::current().absoluteFilePath(dllPath);
    return fileInfo.absoluteFilePath();
}

bool Inject::inject(DWORD processId, const QString& dllPath, HANDLE& hModule)
{
    CleanupTempFiles();

    if (m_isInjected)
    {
        if (HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId))
        {
            DWORD exitCode = 0;
            if (GetExitCodeProcess(hProcess, &exitCode) && exitCode == STILL_ACTIVE)
            {
                if (m_hInjectedModule)
                {
                    CloseHandle(hProcess);
                    hModule = m_hInjectedModule;
                    return true;
                }
            }
            CloseHandle(hProcess);
        }

        m_isInjected = false;
    }

    QString fullPath = getFullDllPath(dllPath);
    std::wstring wOriginalDllPath = fullPath.toStdWString();

    std::ifstream test(wOriginalDllPath.c_str());
    if (!test)
    {
        return false;
    }
    test.close();

    WCHAR tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);

    GUID guidFolder;
    CoCreateGuid(&guidFolder);
    WCHAR guidFolderStr[39];
    StringFromGUID2(guidFolder, guidFolderStr, 39);

    std::wstring folderName(guidFolderStr);
    std::erase(folderName, L'{');
    std::erase(folderName, L'}');
    std::erase(folderName, L'-');

    std::wstring tempDir = std::wstring(tempPath) + folderName;
    CreateDirectoryW(tempDir.c_str(), nullptr);
    SetFileAttributesW(tempDir.c_str(), FILE_ATTRIBUTE_HIDDEN);

    GUID guidDll;
    CoCreateGuid(&guidDll);
    WCHAR guidDllStr[39];
    StringFromGUID2(guidDll, guidDllStr, 39);

    std::wstring dllName(guidDllStr);
    std::erase(dllName, L'{');
    std::erase(dllName, L'}');
    std::erase(dllName, L'-');
    dllName += L".dll";

    std::wstring tempDllPath = tempDir + L"\\" + dllName;

    if (!CopyFileW(wOriginalDllPath.c_str(), tempDllPath.c_str(), FALSE))
    {
        RemoveDirectoryW(tempDir.c_str());
        return false;
    }

    m_currentDllPath = tempDllPath;
    m_currentTempDir = tempDir;

    SetAccessControl(tempDllPath, L"S-1-15-2-1");
    SetAccessControl(wOriginalDllPath, L"S-1-15-2-1");

    if (HANDLE hExistingMap = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, L"Local\\MySharedMemory"))
    {
        if (auto* pOldData = static_cast<SharedData*>(MapViewOfFile(hExistingMap, FILE_MAP_ALL_ACCESS, 0, 0,
                                                                    sizeof(SharedData))))
            UnmapViewOfFile(pOldData);
        CloseHandle(hExistingMap);
    }

    PSECURITY_DESCRIPTOR pSD = nullptr;
    const WCHAR* sddl = L"D:"
        L"(A;OICI;GA;;;WD)"
        L"(A;OICI;GA;;;AC)";

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, &pSD, nullptr))
    {
        CleanupTempFiles();
        return false;
    }

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = pSD;
    sa.bInheritHandle = FALSE;

    HANDLE hMapFile = CreateFileMappingW(
        INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0,
        sizeof(SharedData), L"Local\\MySharedMemory");
    LocalFree(pSD);

    if (!hMapFile)
    {
        CleanupTempFiles();
        return false;
    }

    auto* pShared = static_cast<SharedData*>(MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData)));
    if (!pShared)
    {
        CloseHandle(hMapFile);
        CleanupTempFiles();
        return false;
    }

    pShared->unloadRequested = 0;

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, 0, processId);

    if (!hProc || hProc == INVALID_HANDLE_VALUE)
    {
        HANDLE hToken = nullptr;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        {
            TOKEN_PRIVILEGES tkp;
            LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &tkp.Privileges[0].Luid);
            tkp.PrivilegeCount = 1;
            tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(tkp), nullptr, nullptr);
            CloseHandle(hToken);

            hProc = OpenProcess(PROCESS_ALL_ACCESS, 0, processId);
        }

        if (!hProc || hProc == INVALID_HANDLE_VALUE)
        {
            UnmapViewOfFile(pShared);
            CloseHandle(hMapFile);
            CleanupTempFiles();
            return false;
        }
    }

    void* loc = VirtualAllocEx(hProc, 0, MAX_PATH, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!loc)
    {
        CloseHandle(hProc);
        UnmapViewOfFile(pShared);
        CloseHandle(hMapFile);
        CleanupTempFiles();
        return false;
    }

    size_t pathLen = (tempDllPath.length() + 1) * sizeof(WCHAR);
    if (!WriteProcessMemory(hProc, loc, tempDllPath.c_str(), pathLen, 0))
    {
        VirtualFreeEx(hProc, loc, 0, MEM_RELEASE);
        CloseHandle(hProc);
        UnmapViewOfFile(pShared);
        CloseHandle(hMapFile);
        CleanupTempFiles();
        return false;
    }

    if (HANDLE hThread = CreateRemoteThread(hProc, 0, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(LoadLibraryW), loc, 0,
                                            0))
    {
        WaitForSingleObject(hThread, 5000);

        GetExitCodeThread(hThread, reinterpret_cast<LPDWORD>(&hModule));
        m_hInjectedModule = hModule;
        CloseHandle(hThread);
    }

    VirtualFreeEx(hProc, loc, 0, MEM_RELEASE);
    CloseHandle(hProc);
    UnmapViewOfFile(pShared);
    CloseHandle(hMapFile);

    if (hModule == nullptr)
    {
        CleanupTempFiles();
        m_isInjected = false;
        m_hInjectedModule = nullptr;
        return false;
    }

    m_isInjected = true;
    return true;
}

Inject::~Inject()
= default;

bool Inject::unload()
{
    if (!m_isInjected)
        return true;

    const HANDLE hMapFile = OpenFileMappingW(
        FILE_MAP_READ | FILE_MAP_WRITE,
        FALSE,
        L"Local\\MySharedMemory"
    );

    if (!hMapFile)
    {
        m_isInjected = false;
        m_hInjectedModule = nullptr;
        CleanupTempFiles();
        return true;
    }

    auto* pShared = static_cast<SharedData*>(MapViewOfFile(
        hMapFile, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(SharedData)));

    if (!pShared)
    {
        CloseHandle(hMapFile);
        return false;
    }

    if (pShared->unloadRequested == 2)
    {
        UnmapViewOfFile(pShared);
        CloseHandle(hMapFile);

        m_isInjected = false;
        m_hInjectedModule = nullptr;
        CleanupTempFiles();
        return true;
    }

    pShared->unloadRequested = 1;
    Log("Sent unload request to DLL");

    bool confirmed = false;
    for (int i = 0; i < 100; ++i)
    {
        if (pShared->unloadRequested == 2)
        {
            confirmed = true;
            break;
        }
        Sleep(100);
    }

    UnmapViewOfFile(pShared);
    CloseHandle(hMapFile);

    if (!confirmed)
    {
        Log("Unload timeout");
        return false;
    }

    m_isInjected = false;
    m_hInjectedModule = nullptr;
    CleanupTempFiles();
    return true;
}

bool Inject::isProcessRunning(const char* procName)
{
    return GetProcId(procName) != 0;
}

bool Inject::setDllPermissions(const QString& dllPath)
{
    QString fullPath = getFullDllPath(dllPath);
    std::wstring wPath = fullPath.toStdWString();

    // ALL APPLICATION PACKAGES PERMISSION
    SetAccessControl(wPath, L"S-1-15-2-1");

    return true;
}

HANDLE Inject::OpenProcessWithNtApi(DWORD processId)
{
    return nullptr;
}
