#pragma once

#include <unordered_set>
#include <string>
#include <fstream>
#include <filesystem>
#include <locale>
#include <Psapi.h>

namespace spry
{
	template <typename char_t> static std::unordered_set<std::basic_string<char_t>> extract_strings_from_process()
	{
		std::locale locale;
		std::string filename;
		filename.reserve(256);
		auto success = GetModuleFileNameA(nullptr, filename.data(), 256);
		if (!success) throw std::exception("GetModuleFileName", GetLastError());
		
		size_t filesize = std::experimental::filesystem::file_size(filename);
		char_t* start = new char_t[filesize / sizeof(char_t)];
		const char_t* end = start + (filesize / sizeof(char_t));

		{
			std::ifstream file{ filename, std::ios::binary };
			file.read(reinterpret_cast<char*>(start), filesize);
		}

		std::unordered_set<std::basic_string<char_t>> strings{ 64 };

		auto contains_control_characters = [&](const char_t* string)
		{
			char_t character = *string;
			while (character)
			{
				if (!std::isprint(character, locale)) return true;
				character = *++string;
			}
			return false;
		};

		const char_t* string = start;

		while (string != end)
		{	
			auto length = std::char_traits<char_t>::length(string);
			if (length > small_string_literal<char_t>::max_size)
			{
				if (!contains_control_characters(string)) strings.insert(string);
			}

			string = std::min(string + std::max(1ull, length), end);
		}

		return strings;
	}
}
