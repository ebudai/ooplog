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
		auto contents = detail::get_process_file_contents<char_t>();
		std::unordered_set<std::basic_string<char_t>> strings{ detail::reserve_size };

		auto&& string = contents.data();
		auto&& end = std::next(string, contents.size());

		while (string != end)
		{	
			auto length = std::char_traits<char_t>::length(string);
			if (length > small_string_literal<char_t>::max_size)
			{
				if (detail::is_printable(string, length)) strings.insert(string);
			}
			string = std::min(std::next(string, length + 1), end);
		}

		return strings;
	}

	namespace detail
	{
		static constexpr size_t reserve_size = 256;

		template <typename char_t> auto get_process_file_contents()
		{
			std::string filename;
			filename.reserve(reserve_size);
			auto success = GetModuleFileNameA(nullptr, filename.data(), reserve_size);
			if (!success) throw std::exception("GetModuleFileName", GetLastError());

			size_t filesize = std::experimental::filesystem::file_size(filename);
			auto character_count = static_cast<size_t>(std::ceil(filesize / static_cast<double>(sizeof(char_t))));
			std::vector<char_t> contents;
			contents.resize(character_count, 0);

			std::ifstream file{ filename, std::ios::binary };
			file.read(reinterpret_cast<char*>(&contents[0]), filesize);

			return contents;
		}

		template <typename char_t> bool is_printable(const char_t* string, size_t length)
		{
			static std::locale locale;
			for (auto i = 0ull; i < length; i++)
			{
				if (!std::isprint(string[i], locale)) return false;
			}
			return true;
		}
	}
}
