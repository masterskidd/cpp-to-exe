#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <intrin.h>

struct SysInfo {
  std::string cpu_name, cpu_arch;
  int cpu_cores = 0, cpu_logical = 0;
  MEMORYSTATUSEX mem = {sizeof(MEMORYSTATUSEX)};
  std::vector<std::string> drives;
  std::vector<ULONGLONG> drives_total, drives_free;
  std::string os_name, os_version, computer_name, user_name;
  ULARGE_INTEGER last_idle{}, last_kernel{}, last_user{};
};

inline std::string wstr(const std::wstring& ws) {
  if (ws.empty()) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
  std::string s(n, 0);
  WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), s.data(), n, nullptr, nullptr);
  return s;
}

inline SysInfo gather_sysinfo() {
  SysInfo info;

  SYSTEM_INFO si;
  GetNativeSystemInfo(&si);
  info.cpu_cores = 0; info.cpu_logical = si.dwNumberOfProcessors;

  std::string arch;
  switch (si.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_AMD64: arch = "x64"; break;
    case PROCESSOR_ARCHITECTURE_INTEL: arch = "x86"; break;
    case PROCESSOR_ARCHITECTURE_ARM64: arch = "ARM64"; break;
    default: arch = "Unknown";
  }
  info.cpu_arch = arch;

  HKEY hk;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hk) == ERROR_SUCCESS) {
    WCHAR buf[256]; DWORD sz = sizeof(buf);
    if (RegQueryValueExW(hk, L"ProcessorNameString", nullptr, nullptr, (BYTE*)buf, &sz) == ERROR_SUCCESS)
      info.cpu_name = wstr(buf);
    RegCloseKey(hk);
  }

  DWORD buf_size = 0;
  GetLogicalProcessorInformation(nullptr, &buf_size);
  if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buf(buf_size / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
    if (GetLogicalProcessorInformation(buf.data(), &buf_size)) {
      for (auto& lp : buf)
        if (lp.Relationship == RelationProcessorCore) info.cpu_cores++;
    }
  }

  GlobalMemoryStatusEx(&info.mem);

  DWORD drives_mask = GetLogicalDrives();
  for (int i = 0; i < 26; i++) {
    if (drives_mask & (1 << i)) {
      WCHAR root[] = { (WCHAR)(L'A' + i), L':', L'\\', 0 };
      ULARGE_INTEGER free_, total_;
      if (GetDiskFreeSpaceExW(root, &free_, &total_, nullptr)) {
        info.drives.push_back(wstr(root));
        info.drives_total.push_back(total_.QuadPart);
        info.drives_free.push_back(free_.QuadPart);
      }
    }
  }

  HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  if (ntdll) {
    auto RtlGetVersion = (LONG(WINAPI*)(PRTL_OSVERSIONINFOW))GetProcAddress(ntdll, "RtlGetVersion");
    if (RtlGetVersion) {
      RTL_OSVERSIONINFOW ovi = { sizeof(ovi) };
      if (RtlGetVersion(&ovi) == 0) {
        std::ostringstream os;
        os << "Windows " << ovi.dwMajorVersion << "." << ovi.dwMinorVersion << " (Build " << ovi.dwBuildNumber << ")";
        info.os_name = os.str();
        info.os_version = std::to_string(ovi.dwBuildNumber);
        if (ovi.szCSDVersion[0]) info.os_name += " " + wstr(ovi.szCSDVersion);
      }
    }
  }

  WCHAR buf2[MAX_COMPUTERNAME_LENGTH + 1]; DWORD sz2 = MAX_COMPUTERNAME_LENGTH + 1;
  if (GetComputerNameW(buf2, &sz2)) info.computer_name = wstr(buf2);
  DWORD sz3 = 256;
  if (GetUserNameW(buf2, &sz3)) info.user_name = wstr(buf2);

  return info;
}

inline double cpu_usage(SysInfo& info) {
  FILETIME idle, kernel, user;
  if (!GetSystemTimes(&idle, &kernel, &user)) return 0.0;
  ULARGE_INTEGER i{idle.dwLowDateTime, idle.dwHighDateTime};
  ULARGE_INTEGER k{kernel.dwLowDateTime, kernel.dwHighDateTime};
  ULARGE_INTEGER u{user.dwLowDateTime, user.dwHighDateTime};
  if (info.last_idle.QuadPart == 0) {
    info.last_idle = i; info.last_kernel = k; info.last_user = u;
    return 0.0;
  }
  ULONGLONG idle_diff = i.QuadPart - info.last_idle.QuadPart;
  ULONGLONG kernel_diff = k.QuadPart - info.last_kernel.QuadPart;
  ULONGLONG user_diff = u.QuadPart - info.last_user.QuadPart;
  info.last_idle = i; info.last_kernel = k; info.last_user = u;
  ULONGLONG total = kernel_diff + user_diff;
  return total ? (1.0 - (double)idle_diff / total) * 100.0 : 0.0;
}
