#include "WinService.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

const char* kServiceName = "InjectorCLI";
const char* kServiceDisplay = "Injector CLI";
const char* kServiceDesc = "Watches for configured processes and injects payload DLLs.";

SERVICE_STATUS_HANDLE g_statusHandle = nullptr;
SERVICE_STATUS g_status{};
HANDLE g_stopEvent = nullptr;
std::vector<HANDLE> g_children;
HANDLE g_logFile = INVALID_HANDLE_VALUE;
std::string g_configPath;
std::string g_serviceName = kServiceName;

std::string GetExePath()
{
	char buf[MAX_PATH] = {};
	GetModuleFileNameA(nullptr, buf, MAX_PATH);
	return buf;
}

std::string GetDir(const std::string& path)
{
	size_t pos = path.find_last_of("\\/");
	if (pos == std::string::npos) {
		return ".";
	}
	return path.substr(0, pos);
}

std::string Join(const std::string& a, const std::string& b)
{
	if (a.empty()) {
		return b;
	}
	if (a.back() == '\\' || a.back() == '/') {
		return a + b;
	}
	return a + "\\" + b;
}

void Log(const std::string& line)
{
	std::string msg = line + "\r\n";
	if (g_logFile != INVALID_HANDLE_VALUE) {
		DWORD written = 0;
		WriteFile(g_logFile, msg.data(), (DWORD)msg.size(), &written, nullptr);
		FlushFileBuffers(g_logFile);
	}
}

std::string FindIniPath()
{
	fs::path dir = GetDir(GetExePath());
	for (int i = 0; i < 6; ++i) {
		fs::path candidate = dir / "inject-service.ini";
		if (fs::exists(candidate)) {
			return candidate.string();
		}
		if (!dir.has_parent_path() || dir.parent_path() == dir) {
			break;
		}
		dir = dir.parent_path();
	}
	return Join(GetDir(GetExePath()), "inject-service.ini");
}

std::string IniString(const std::string& ini, const char* section, const char* key, const char* def = "")
{
	char buf[MAX_PATH * 2] = {};
	GetPrivateProfileStringA(section, key, def, buf, sizeof(buf), ini.c_str());
	return buf;
}

int IniInt(const std::string& ini, const char* section, const char* key, int def)
{
	return (int)GetPrivateProfileIntA(section, key, def, ini.c_str());
}

bool IniBool(const std::string& ini, const char* section, const char* key, bool def)
{
	std::string v = IniString(ini, section, key, def ? "1" : "0");
	return v == "1" || _stricmp(v.c_str(), "true") == 0 || _stricmp(v.c_str(), "yes") == 0 || _stricmp(v.c_str(), "on") == 0;
}

std::string ActiveConfigPath()
{
	if (!g_configPath.empty() && fs::exists(g_configPath)) {
		return g_configPath;
	}
	return FindIniPath();
}

std::string FileName(const std::string& path)
{
	size_t pos = path.find_last_of("\\/");
	if (pos == std::string::npos) {
		return path;
	}
	return path.substr(pos + 1);
}

std::string ResolvePayloadDll(const std::string& payloadDir, const std::string& rel)
{
	if (rel.empty()) {
		return {};
	}

	fs::path direct = fs::path(payloadDir) / rel;
	std::error_code ec;
	if (fs::exists(direct, ec)) {
		return fs::absolute(direct, ec).string();
	}

	std::string wanted = FileName(rel);
	if (!fs::exists(payloadDir, ec)) {
		return {};
	}

	for (fs::recursive_directory_iterator it(payloadDir, ec), end; it != end && !ec; it.increment(ec)) {
		if (it->is_regular_file(ec) && _stricmp(it->path().filename().string().c_str(), wanted.c_str()) == 0) {
			return fs::absolute(it->path(), ec).string();
		}
	}
	return {};
}

void ReportStatus(DWORD state, DWORD exitCode = NO_ERROR, DWORD waitHint = 0)
{
	static DWORD checkpoint = 1;
	g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	g_status.dwCurrentState = state;
	g_status.dwWin32ExitCode = exitCode;
	g_status.dwWaitHint = waitHint;
	g_status.dwControlsAccepted = (state == SERVICE_START_PENDING) ? 0 : (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN);

	if (state == SERVICE_RUNNING || state == SERVICE_STOPPED) {
		g_status.dwCheckPoint = 0;
	}
	else {
		g_status.dwCheckPoint = checkpoint++;
	}

	if (g_statusHandle) {
		SetServiceStatus(g_statusHandle, &g_status);
	}
}

void StopChildren()
{
	for (HANDLE h : g_children) {
		if (h && h != INVALID_HANDLE_VALUE) {
			TerminateProcess(h, 0);
			WaitForSingleObject(h, 3000);
			CloseHandle(h);
		}
	}
	g_children.clear();
}

bool StartWorkers()
{
	std::string ini = ActiveConfigPath();
	if (!fs::exists(ini)) {
		Log("Missing config ini: " + ini);
		return false;
	}
	Log("Using config: " + ini);

	std::string iniDir = GetDir(ini);
	std::string payloadDir = IniString(ini, "General", "PayloadDir");
	if (payloadDir.empty()) {
		payloadDir = Join(iniDir, "Payload");
	}

	int defaultDelay = IniInt(ini, "General", "InjectDelay", 10);
	int maxTargets = IniInt(ini, "General", "MaxTargets", 8);
	if (maxTargets < 1) {
		maxTargets = 1;
	}
	if (maxTargets > 32) {
		maxTargets = 32;
	}
	std::string exe = GetExePath();

	for (int i = 1; i <= maxTargets; ++i) {
		std::string section = "Target" + std::to_string(i);
		if (!IniBool(ini, section.c_str(), "Enabled", true)) {
			continue;
		}
		std::string process = IniString(ini, section.c_str(), "Process");
		std::string dllRel = IniString(ini, section.c_str(), "Dll");
		std::string mapping = IniString(ini, section.c_str(), "Mapping", "STANDART");
		if (process.empty() || dllRel.empty()) {
			continue;
		}

		std::string dll = ResolvePayloadDll(payloadDir, dllRel);
		if (dll.empty()) {
			Log("Payload not found for " + section + ": " + dllRel + " (PayloadDir=" + payloadDir + ")");
			continue;
		}

		int delay = IniInt(ini, section.c_str(), "InjectDelay", defaultDelay);
		std::string cmd = "\"" + exe + "\" -m " + mapping +
			" --auto_inject_process \"" + process + "\"" +
			" --inject_delay " + std::to_string(delay);
		if (IniBool(ini, section.c_str(), "DeleteHeaders", false)) {
			cmd += " -x";
		}
		if (IniBool(ini, section.c_str(), "ManualImports", false)) {
			cmd += " -i";
		}
		if (IniBool(ini, section.c_str(), "NoThreads", false)) {
			cmd += " -t";
		}
		cmd += " \"" + dll + "\"";

		STARTUPINFOA si{};
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
		si.wShowWindow = SW_HIDE;
		si.hStdInput = INVALID_HANDLE_VALUE;
		si.hStdOutput = g_logFile;
		si.hStdError = g_logFile;

		PROCESS_INFORMATION pi{};
		std::vector<char> cmdBuf(cmd.begin(), cmd.end());
		cmdBuf.push_back('\0');

		if (!CreateProcessA(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE,
			CREATE_NO_WINDOW, nullptr, iniDir.c_str(), &si, &pi)) {
			Log("Failed to start worker for " + process + " error=" + std::to_string(GetLastError()));
			continue;
		}

		CloseHandle(pi.hThread);
		g_children.push_back(pi.hProcess);
		Log("Started worker for " + process + " -> " + dll + " pid=" + std::to_string(pi.dwProcessId));
	}

	return !g_children.empty();
}

VOID WINAPI ServiceCtrlHandler(DWORD ctrl)
{
	if (ctrl == SERVICE_CONTROL_STOP || ctrl == SERVICE_CONTROL_SHUTDOWN) {
		ReportStatus(SERVICE_STOP_PENDING, NO_ERROR, 3000);
		if (g_stopEvent) {
			SetEvent(g_stopEvent);
		}
	}
}

VOID WINAPI ServiceMain(DWORD, LPSTR*)
{
	g_statusHandle = RegisterServiceCtrlHandlerA(g_serviceName.c_str(), ServiceCtrlHandler);
	if (!g_statusHandle) {
		return;
	}

	ReportStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

	std::string ini = ActiveConfigPath();
	std::string logPath = IniString(ini, "General", "LogFile");
	if (logPath.empty()) {
		logPath = Join(GetDir(ini), "inject-service.log");
	}
	SECURITY_ATTRIBUTES sa{};
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;
	g_logFile = CreateFileA(logPath.c_str(), FILE_APPEND_DATA,
		FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	SetFilePointer(g_logFile, 0, nullptr, FILE_END);
	Log("Service starting");

	g_stopEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
	if (!StartWorkers()) {
		Log("No workers started");
		ReportStatus(SERVICE_STOPPED, ERROR_SERVICE_SPECIFIC_ERROR);
		if (g_logFile != INVALID_HANDLE_VALUE) {
			CloseHandle(g_logFile);
			g_logFile = INVALID_HANDLE_VALUE;
		}
		return;
	}

	ReportStatus(SERVICE_RUNNING);
	Log("Service running");
	WaitForSingleObject(g_stopEvent, INFINITE);

	Log("Service stopping");
	StopChildren();
	if (g_stopEvent) {
		CloseHandle(g_stopEvent);
		g_stopEvent = nullptr;
	}
	if (g_logFile != INVALID_HANDLE_VALUE) {
		CloseHandle(g_logFile);
		g_logFile = INVALID_HANDLE_VALUE;
	}
	ReportStatus(SERVICE_STOPPED);
}

int InstallService()
{
	std::string ini = ActiveConfigPath();
	if (!fs::exists(ini)) {
		printf("Config not found: %s\n", ini.c_str());
		printf("Create inject-service.ini or pass --config <path>\n");
		return 1;
	}

	std::string serviceName = IniString(ini, "General", "ServiceName", kServiceName);
	std::string displayName = IniString(ini, "General", "ServiceDisplay", kServiceDisplay);
	if (serviceName.empty()) {
		serviceName = kServiceName;
	}
	if (displayName.empty()) {
		displayName = kServiceDisplay;
	}

	std::string startType = IniString(ini, "General", "StartType", "delayed");
	DWORD startMode = SERVICE_AUTO_START;
	bool delayedStart = true;
	if (_stricmp(startType.c_str(), "demand") == 0 || _stricmp(startType.c_str(), "manual") == 0) {
		startMode = SERVICE_DEMAND_START;
		delayedStart = false;
	}
	else if (_stricmp(startType.c_str(), "auto") == 0) {
		delayedStart = false;
	}

	SC_HANDLE scm = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
	if (!scm) {
		printf("OpenSCManager failed (%lu). Run as Administrator.\n", GetLastError());
		return 1;
	}

	std::string exe = GetExePath();
	std::string binPath = "\"" + exe + "\" --service --config \"" + ini + "\"";

	SC_HANDLE svc = OpenServiceA(scm, serviceName.c_str(), SERVICE_ALL_ACCESS);
	if (svc) {
		if (!ChangeServiceConfigA(svc, SERVICE_WIN32_OWN_PROCESS, startMode,
			SERVICE_ERROR_NORMAL, binPath.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr, displayName.c_str())) {
			printf("ChangeServiceConfig failed (%lu)\n", GetLastError());
			CloseServiceHandle(svc);
			CloseServiceHandle(scm);
			return 1;
		}
	}
	else {
		svc = CreateServiceA(
			scm,
			serviceName.c_str(),
			displayName.c_str(),
			SERVICE_ALL_ACCESS,
			SERVICE_WIN32_OWN_PROCESS,
			startMode,
			SERVICE_ERROR_NORMAL,
			binPath.c_str(),
			nullptr, nullptr, nullptr, nullptr, nullptr);
		if (!svc) {
			printf("CreateService failed (%lu)\n", GetLastError());
			CloseServiceHandle(scm);
			return 1;
		}
	}

	SERVICE_DESCRIPTIONA desc{};
	desc.lpDescription = (LPSTR)kServiceDesc;
	ChangeServiceConfig2A(svc, SERVICE_CONFIG_DESCRIPTION, &desc);

	SERVICE_DELAYED_AUTO_START_INFO delayed{};
	delayed.fDelayedAutostart = delayedStart ? TRUE : FALSE;
	ChangeServiceConfig2A(svc, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, &delayed);

	if (!StartServiceA(svc, 0, nullptr)) {
		DWORD err = GetLastError();
		if (err != ERROR_SERVICE_ALREADY_RUNNING) {
			printf("StartService failed (%lu)\n", err);
			CloseServiceHandle(svc);
			CloseServiceHandle(scm);
			return 1;
		}
	}

	printf("Service '%s' installed and started.\n", serviceName.c_str());
	printf("Binary: %s\n", exe.c_str());
	printf("Config: %s\n", ini.c_str());
	printf("Edit that ini to change processes, DLLs, mapping, and delay.\n");
	CloseServiceHandle(svc);
	CloseServiceHandle(scm);
	return 0;
}

int UninstallService()
{
	SC_HANDLE scm = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT);
	if (!scm) {
		printf("OpenSCManager failed (%lu). Run as Administrator.\n", GetLastError());
		return 1;
	}

	std::string serviceName = IniString(ActiveConfigPath(), "General", "ServiceName", kServiceName);
	if (serviceName.empty()) {
		serviceName = kServiceName;
	}

	SC_HANDLE svc = OpenServiceA(scm, serviceName.c_str(), SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS);
	if (!svc) {
		printf("Service '%s' is not installed.\n", serviceName.c_str());
		CloseServiceHandle(scm);
		return 1;
	}

	SERVICE_STATUS status{};
	ControlService(svc, SERVICE_CONTROL_STOP, &status);
	for (int i = 0; i < 20; ++i) {
		if (!QueryServiceStatus(svc, &status) || status.dwCurrentState == SERVICE_STOPPED) {
			break;
		}
		Sleep(250);
	}

	if (!DeleteService(svc)) {
		printf("DeleteService failed (%lu)\n", GetLastError());
		CloseServiceHandle(svc);
		CloseServiceHandle(scm);
		return 1;
	}

	printf("Service '%s' removed.\n", serviceName.c_str());
	CloseServiceHandle(svc);
	CloseServiceHandle(scm);
	return 0;
}

int RunService()
{
	g_serviceName = IniString(ActiveConfigPath(), "General", "ServiceName", kServiceName);
	if (g_serviceName.empty()) {
		g_serviceName = kServiceName;
	}

	SERVICE_TABLE_ENTRYA table[] = {
		{ g_serviceName.empty() ? (LPSTR)kServiceName : g_serviceName.data(), ServiceMain },
		{ nullptr, nullptr }
	};
	if (!StartServiceCtrlDispatcherA(table)) {
		printf("StartServiceCtrlDispatcher failed (%lu).\n", GetLastError());
		printf("Install with: InjectorCLI.exe --service install\n");
		return 1;
	}
	return 0;
}

} // namespace

bool HandleServiceCommand(int argc, char* argv[])
{
	if (argc < 2 || _stricmp(argv[1], "--service") != 0) {
		return false;
	}

	std::string action;
	for (int i = 2; i < argc; ++i) {
		if (_stricmp(argv[i], "--config") == 0 && i + 1 < argc) {
			g_configPath = argv[++i];
		}
		else if (_stricmp(argv[i], "install") == 0 || _stricmp(argv[i], "uninstall") == 0) {
			action = argv[i];
		}
	}
	if (g_configPath.empty()) {
		g_configPath = FindIniPath();
	}

	if (_stricmp(action.c_str(), "install") == 0) {
		exit(InstallService());
	}
	if (_stricmp(action.c_str(), "uninstall") == 0) {
		exit(UninstallService());
	}

	exit(RunService());
}
