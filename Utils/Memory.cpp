#include <tlhelp32.h>
#include <psapi.h>
#include "ProcessHandle.h"
#include "Memory.h"

std::unordered_map<string, Memory::ModuleInfo> Memory::loaded_modules;

#define InRange(x, a, b) (x >= a && x <= b)
#define getBit(x) (InRange((x & (~0x20)), 'A', 'F') ? ((x & (~0x20)) - 'A' + 0xA): (InRange(x, '0', '9') ? x - '0': 0))
#define getByte(x) (getBit(x[0]) << 4 | getBit(x[1]))

optional<uintptr_t> Memory::pattern_scan(const string target_module, const string target_pattern) {
    auto* buffer = new unsigned char[Memory::loaded_modules[target_module].region_size];
    SIZE_T bytesRead;

    if (!ReadProcessMemory(ProcessHandle::get_handle(), reinterpret_cast<LPCVOID>(Memory::loaded_modules[target_module].start_address), buffer, Memory::loaded_modules[target_module].region_size, &bytesRead)) {
        LOG::ERR("(PatternScan) ReadProcessMemory failed: 0x%d", GetLastError());
        delete[] buffer;
        return nullopt;
    }

    const char* pattern = target_pattern.c_str();
    uintptr_t first_match = 0;

    for (uintptr_t i = 0; i < bytesRead; ++i) {
        if (!*pattern) {
            delete[] buffer;
            return Memory::loaded_modules[target_module].start_address + first_match;
        }

// Retarder visual studio
#pragma warning(push)
#pragma warning(disable : 6385)
        const unsigned char pattern_current = *reinterpret_cast<const unsigned char*>(pattern);
        const unsigned char memory_current = buffer[i];
#pragma warning(pop)

        if (pattern_current == '\?' || memory_current == getByte(pattern)) {
            if (!first_match) {
                first_match = i;
            }

            if (!pattern[2]) {
                delete[] buffer;
                return Memory::loaded_modules[target_module].start_address + first_match;
            }

            pattern += pattern_current != '\?' ? 3 : 2;
        }
        else {
            pattern = target_pattern.c_str();
            first_match = 0;
        }
    }

    delete[] buffer;
    return nullopt;
}

bool Memory::load_modules(DWORD process_ID) {
    const char* Modules[] {
        "client.dll",
        "engine2.dll",
        "schemasystem.dll",
        "particles.dll",
    };

    std::unordered_map<string, ModuleInfo> found_modules;
    std::vector<string> needed_modules(std::begin(Modules), std::end(Modules));

    while (true) {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, process_ID);
        if (hSnapshot == INVALID_HANDLE_VALUE)
            return false;

        MODULEENTRY32W me32{};
        me32.dwSize = sizeof(MODULEENTRY32W);

        if (Module32FirstW(hSnapshot, &me32)) {
            do {
                string module_name = Utils::wchar_to_string(me32.szModule).value();
                auto it = std::find(needed_modules.begin(), needed_modules.end(), module_name);
                if (it != needed_modules.end()) {
                    ModuleInfo modInfo = {
                        reinterpret_cast<uintptr_t>(me32.modBaseAddr),
                        reinterpret_cast<uintptr_t>(me32.modBaseAddr) + me32.modBaseSize,
                        static_cast<size_t>(me32.modBaseSize),
                        me32.hModule
                    };

                    found_modules[module_name] = modInfo;
                    needed_modules.erase(it);
                }
            }
            while (Module32NextW(hSnapshot, &me32) && !needed_modules.empty());
        }

        CloseHandle(hSnapshot);

        if (needed_modules.empty()) {
            loaded_modules = std::move(found_modules);
            return true;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return false;
}

bool Memory::patch(const uintptr_t patch_addr, const Patches::JumpType jump_type) {
    std::vector<BYTE> patchData;
    patchData.push_back((BYTE)jump_type);

    DWORD oldProtect;
    if (!VirtualProtectEx(ProcessHandle::get_handle(), reinterpret_cast<LPVOID>(patch_addr), patchData.size(), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        LOG::ERR("(Patch) Failed to change memory protection: 0x%d", GetLastError());
        return false;
    }

    SIZE_T bytesWritten;
    if (!WriteProcessMemory(ProcessHandle::get_handle(), reinterpret_cast<LPVOID>(patch_addr), patchData.data(), patchData.size(), &bytesWritten)) {
        LOG::ERR("(Patch) Failed to write to process memory: 0x%d", GetLastError());
        VirtualProtectEx(ProcessHandle::get_handle(), reinterpret_cast<LPVOID>(patch_addr), patchData.size(), oldProtect, &oldProtect);
        return false;
    }

    if (!VirtualProtectEx(ProcessHandle::get_handle(), reinterpret_cast<LPVOID>(patch_addr), patchData.size(), oldProtect, &oldProtect)) {
        LOG::ERR("(Patch) Failed to restore memory protection: 0x%d", GetLastError());
        return false;
    }

    return true;
}
