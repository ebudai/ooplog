#pragma once

#include <unordered_set>
#include <string>
#include <Psapi.h>

namespace spry
{
	static std::unordered_set<std::string> extract_embedded_strings_from_current_module()
	{
		MODULEINFO module_info{ 0 };
		auto success = GetModuleInformation(GetCurrentProcess(), GetModuleHandle(nullptr), &module_info, sizeof(module_info));
		if (!success) throw std::exception("extract_embedded_strings_from_current_module", GetLastError());

		const char* current_char = reinterpret_cast<decltype(current_char)>(module_info.lpBaseOfDll);
		const char* end = current_char + module_info.SizeOfImage;

		std::string buffer;
		std::unordered_set<std::string> strings;
		strings.reserve(64);

		while (++current_char != end)
		{
			if (*current_char == '\0')
			{
				if (!buffer.empty()) strings.insert(buffer);
				buffer.clear();
				current_char;
			}

			if (*current_char == '\r') buffer += *current_char;
			else if (*current_char == '\n') buffer += *current_char;
			else if (*current_char >= 32 && *current_char <= 165 && *current_char != 127) buffer += *current_char;
			else buffer.clear();
		}

		return strings;
	}	
}
