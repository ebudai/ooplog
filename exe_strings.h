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
		static constexpr size_t reserve_size = 256;

		std::string filename;
		filename.reserve(reserve_size);
		auto success = GetModuleFileNameA(nullptr, filename.data(), reserve_size);
		if (!success) throw std::exception("GetModuleFileName", GetLastError());
		
		size_t filesize = std::experimental::filesystem::file_size(filename);
		auto character_count = static_cast<size_t>(std::ceil(filesize / static_cast<double>(sizeof(char_t))));
		auto start = std::make_unique<char_t[]>(character_count);

		{
			std::ifstream file{ filename, std::ios::binary };
			file.read(reinterpret_cast<char*>(start.get()), filesize);
		}

		std::unordered_set<std::basic_string<char_t>> strings{ reserve_size };

		auto is_printable = [&](const char_t* string, size_t length)
		{
			std::locale locale;
			for (auto i = 0ull; i < length; i++)
			{
				if (!std::isprint(string[i], locale)) return false;
			}
			return true;
		};

		auto&& string = start.get();
		const auto end = std::next(start.get(), character_count);

		while (string != end)
		{	
			auto length = std::char_traits<char_t>::length(string);
			if (length > small_string_literal<char_t>::max_size)
			{
				if (is_printable(string, length)) strings.insert(string);
			}
			string = std::min(std::next(string, length + 1), end);
		}

		return strings;
	}
}
