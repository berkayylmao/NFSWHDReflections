/*
// clang-format off
//
//    NFS World HD Reflections (NFSWHDReflections)
//    Copyright (C) 2021 Berkay Yigit <berkaytgy@gmail.com>
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU Affero General Public License as published
//    by the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
//    GNU Affero General Public License for more details.
//
//    You should have received a copy of the GNU Affero General Public License
//    along with this program. If not, see <https://www.gnu.org/licenses/>.
//
// clang-format on
*/

#pragma warning(push, 0)

// Win32 targeting
#include <winsdkver.h>
#define _WIN32_WINNT 0x0501  // _WIN32_WINNT_WINXP
#include <sdkddkver.h>

// Win32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

// Commonly used headers
#include <algorithm>  // clamp
#include <cstddef>    // size_t
#include <cstdint>    // integer types
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

// rapidJSON
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>

#pragma warning(pop)  // re-enable warnings

inline DWORD makeabs(DWORD rva) {
  static const auto _base = reinterpret_cast<DWORD>(::GetModuleHandle(NULL));
  return _base + rva;
}

struct AllAccess {
  LPVOID      mAddr{nullptr};
  std::size_t mSize{0};
  DWORD       mOldProtect{0};

  explicit AllAccess(DWORD rva, std::size_t size, bool makeAbs) {
    if (makeAbs) rva = makeabs(rva);
    mAddr = reinterpret_cast<LPVOID>(rva);
    mSize = size;

    ::VirtualProtect(mAddr, mSize, PAGE_EXECUTE_READWRITE, &mOldProtect);
  }
  explicit AllAccess(DWORD rva, std::size_t size) : AllAccess(rva, size, true) {}

  ~AllAccess() {
    DWORD _dummy;
    ::VirtualProtect(mAddr, mSize, mOldProtect, &_dummy);
  }
};

static void Main(HMODULE hModule) {
  thread_local rapidjson::Document _config;
  static constexpr char            _configFileName[] = "NFSWHDReflections.json";
  static constexpr char            _configDefFile[] =
      R"({"ReflectionResolution":1024,"BetterReflectionLODs":true,"BetterReflectionDrawDistance":true,"BetterChrome":{"Enabled":true,"Saturation":0.075,"ReflectionIntensity":6.75}})";

  // Config
  {
    std::filesystem::path _asiDir, _configFilePath;

    // Get current .asi directory
    {
      std::wstring _strPath(2048, 0);
      ::GetModuleFileNameW(hModule, &_strPath[0], _strPath.capacity());

      _asiDir.assign(_strPath);
      _asiDir = _asiDir.parent_path();
    }

    // Get config file path
    { _configFilePath = _asiDir / _configFileName; }

    if (std::filesystem::exists(_configFilePath)) {  // Load config
      std::ifstream             _ifs(_configFilePath, std::ios_base::binary);
      rapidjson::IStreamWrapper _isw(_ifs);
      _config.ParseStream(_isw);
    } else {  // Create config
      _config.Parse(_configDefFile);

      std::ofstream                                      _ofs(_configFilePath, std::ios_base::binary);
      rapidjson::OStreamWrapper                          _osw(_ofs);
      rapidjson::PrettyWriter<rapidjson::OStreamWrapper> _writer(_osw);
      _writer.SetIndent(' ', 3);
      _config.Accept(_writer);
    }
  }

  // Reflection resolution
  {
    AllAccess _a1(0x22CA04, sizeof(std::uint16_t));
    AllAccess _a2(0x22CA07, sizeof(std::uint32_t));

    *reinterpret_cast<std::uint16_t*>(_a1.mAddr) = 0x9090;
    *reinterpret_cast<std::uint32_t*>(_a2.mAddr) = _config["ReflectionResolution"].GetUint();
  }

  // Better reflection LODs
  if (_config["BetterReflectionLODs"].GetBool()) {
    AllAccess _a1(0x89E48C, sizeof(bool));
    AllAccess _a2(0x33D8BF, sizeof(unsigned char));
    AllAccess _a3(0x33DEBD, sizeof(std::uint32_t));

    *reinterpret_cast<bool*>(_a1.mAddr)          = false;
    *reinterpret_cast<unsigned char*>(_a2.mAddr) = 0x10;
    *reinterpret_cast<std::uint32_t*>(_a3.mAddr) = 0x8002;
  }

  // Better reflection draw distance
  if (_config["BetterReflectionDrawDistance"].GetBool()) {
    AllAccess _a1(0x882D44, sizeof(float));

    *reinterpret_cast<float*>(_a1.mAddr) = 16000.0f;
  }

  // Better chrome
  {
    const auto& _chrome = _config["BetterChrome"];

    if (_chrome["Enabled"].GetBool()) {
      AllAccess _pBase(0x8B4208, sizeof(DWORD));
      auto      _base = *reinterpret_cast<DWORD*>(_pBase.mAddr);

      while (!_base || strcmp(reinterpret_cast<const char*>(_base - 0xE24), "CHROME") != 0) {
        _base = *reinterpret_cast<DWORD*>(_pBase.mAddr);
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }

      AllAccess _sat1(_base - 0xDC0, sizeof(float), false);
      AllAccess _sat2(_base - 0xDD0, sizeof(float), false);
      AllAccess _eff1(_base - 0xD60, sizeof(float), false);
      AllAccess _eff2(_base - 0xD70, sizeof(float), false);

      const auto _satV = static_cast<float>(std::clamp(_chrome["Saturation"].GetDouble(), -1.0, 1.0));
      const auto _effV = static_cast<float>(std::clamp(_chrome["ReflectionIntensity"].GetDouble(), -10.0, 10.0));

      *reinterpret_cast<float*>(_sat1.mAddr) = _satV;
      *reinterpret_cast<float*>(_sat2.mAddr) = _satV;
      *reinterpret_cast<float*>(_eff1.mAddr) = _effV;
      *reinterpret_cast<float*>(_eff2.mAddr) = _effV;
    }
  }
}

// win32 entry
BOOL WINAPI DllMain(const HMODULE hModule, const DWORD ul_reason_for_call, LPCVOID) {
  if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
    ::DisableThreadLibraryCalls(hModule);
    std::thread(Main, hModule).detach();
  }
  return TRUE;
}
