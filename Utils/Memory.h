#pragma once
#include <unordered_map>
#include "..\Dota2Patcher.h"
#include "ProcessHandle.h"

class Memory {
public:
	struct ModuleInfo {
		uintptr_t start_address;
		uintptr_t end_address;
		size_t region_size;
		HMODULE hmodule;
	};

	static bool load_modules(const DWORD process_ID);
	static optional<uintptr_t> pattern_scan(const string& target_module, const string& target_pattern);
	static bool patch(const uintptr_t& patch_addr, const PATCH_TYPE patch_type, const optional<string>& replace_str = nullopt);

	template<typename T, typename N>
	static optional<T> absolute_address(const N& instruction_ptr, const ASM_TYPE instr_type = ASM_TYPE::LEA) {
		uintptr_t address = 0;

		if constexpr (std::is_pointer_v<N>)
			address = reinterpret_cast<uintptr_t>(instruction_ptr);
		else if constexpr (std::is_integral_v<N>)
			address = static_cast<uintptr_t>(instruction_ptr);

		ptrdiff_t offset = 0;
		uint32_t size = 0;
		switch (instr_type) {
		case ASM_TYPE::LEA:
			offset = 3;
			size = 7;
			break;
		case ASM_TYPE::CALL:
			offset = 2;
			size = 6;
			break;
		default:
			LOG::ERR("(absolute_address) Unsupported instruction type");
			return nullopt;
		}

		int32_t relative_offset = 0;
		if (!ReadProcessMemory(ProcessHandle::get_handle(), reinterpret_cast<LPCVOID>(address + offset), &relative_offset, sizeof(relative_offset), nullptr)) {
			LOG::ERR("(absolute_address) ReadProcessMemory failed: {}", GetLastError());
			return nullopt;
		}

		uintptr_t absolute_address = address + relative_offset + size;

		return absolute_address;
	}

	template<typename T, typename N>
	static optional<T> virtual_function(const N& vmt, const int function_index) {
		uintptr_t address = 0;

		if constexpr (std::is_pointer_v<N>)
			address = reinterpret_cast<uintptr_t>(vmt);
		else if constexpr (std::is_integral_v<N>)
			address = static_cast<uintptr_t>(vmt);

		const auto base_ptr = Memory::read_memory<uintptr_t>(address);
		if (!base_ptr)
			return nullopt;

		int actual_index = (function_index - 1) * 8;

		const auto instruction_ptr = Memory::read_memory<uintptr_t>(base_ptr.value() + actual_index);
		if (!instruction_ptr)
			return nullopt;

		return instruction_ptr;
	}

	template<typename T, typename N>
	static optional<T> read_memory(const N& address) {
		T value{};
		SIZE_T bytesRead = 0;

		if (!is_valid_ptr(address))
			return nullopt;

		if (!ReadProcessMemory(ProcessHandle::get_handle(), reinterpret_cast<LPCVOID>(address), &value, sizeof(T), &bytesRead)) {
			LOG::ERR("(read_memory) Failed to read memory: {}", GetLastError());
			return nullopt;
		}

		if (bytesRead != sizeof(T)) {
			LOG::ERR("(read_memory) Partial read at {}", (void*)address);
			return nullopt;
		}

		return value;
	}

	template<typename T, typename N>
	static bool write_memory(const N& address, const T& value) {
		SIZE_T bytesWritten;
		if (!WriteProcessMemory(ProcessHandle::get_handle(), reinterpret_cast<LPVOID>(address), &value, sizeof(T), &bytesWritten)) {
			LOG::ERR("(write_memory) Failed to write memory at {}: {}", (void*)address, GetLastError());
			return false;
		}

		if (bytesWritten != sizeof(T)) {
			LOG::ERR("(write_memory) Partial write at {}", (void*)address);
			return false;
		}

		return true;
	}

	template<typename T>
	static void write_string(const T& address, const string& text) {
		size_t text_size = strlen(text.c_str()) + 1;
		HANDLE hproc = ProcessHandle::get_handle();

		VirtualAllocEx(hproc, nullptr, text_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		WriteProcessMemory(hproc, reinterpret_cast<LPVOID>(address), text.c_str(), text_size, nullptr);
		VirtualFreeEx(hproc, reinterpret_cast<LPVOID>(address), 0, MEM_RELEASE);
	}

	template<typename T>
	static optional <string> read_string(const T& address, const size_t max_length = 64) {
		std::vector<char> buffer(max_length, 0);
		SIZE_T bytesRead;

		if (!ReadProcessMemory(ProcessHandle::get_handle(), reinterpret_cast<LPCVOID>(address), buffer.data(), buffer.size(), &bytesRead))
			return nullopt;

		return string(buffer.data());
	}

	template<typename T>
	static bool is_valid_ptr(const T& ptr) {
		MEMORY_BASIC_INFORMATION mbi{};

		if (!VirtualQueryEx(ProcessHandle::get_handle(), reinterpret_cast<LPCVOID>(ptr), &mbi, sizeof(mbi)))
			return false;

		uintptr_t test = 0;
		if (!ReadProcessMemory(ProcessHandle::get_handle(), reinterpret_cast<LPCVOID>(ptr), &test, sizeof(test), nullptr))
			return false;

		return true;
	}

	template<typename T>
	static int count_vms(const T& vmt) {
		int count = 1;

		while (true) {
			const auto vfunc = Memory::virtual_function<uintptr_t>(vmt, count);
			if (!vfunc || vfunc.value_or(0) == 0)
				break;

			if (!Memory::is_valid_ptr(vfunc.value()))
				return count;

			count++;
		}

		return count;
	}

	static std::unordered_map<string, ModuleInfo> loaded_modules;
};
