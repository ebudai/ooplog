#pragma once

#define NOMINMAX

#include <cstdint>
#include <algorithm>
#include <chrono>
#include <variant>
#include <array>
#include <Windows.h>

namespace spry
{
	struct newline { };
	struct string_hash { uint64_t value; };
	using time_point = std::chrono::steady_clock::time_point;

	template <typename char_t> struct small_string_literal
	{
		small_string_literal(const char_t* string)
			: storage(reinterpret_cast<uint64_t>(string))
		{ }

		static constexpr auto max_size = sizeof(uint64_t) / sizeof(char_t);

	private:

		uint64_t storage;
	};

	using arg = std::variant<
		newline, time_point,
		string_hash,
		const char*, const wchar_t*, const char16_t*, const char32_t*,
		small_string_literal<char>,
		small_string_literal<wchar_t>,
		small_string_literal<char16_t>,
		small_string_literal<char32_t>,
		uint8_t, int8_t,
		uint16_t, int16_t,
		uint32_t, int32_t,
		uint64_t, int64_t,
		float, double>;

	template <typename T> static constexpr bool is_character_type_v =
		std::is_same_v<T, char>
			|| std::is_same_v<T, unsigned char>
			|| std::is_same_v<T, wchar_t>
			|| std::is_same_v<T, char16_t>
			|| std::is_same_v<T, char32_t>;

	template <typename T, size_t N> static constexpr bool is_small_string_literal_v =
		!std::is_pointer_v<T&&>
			&& is_character_type_v<std::decay_t<T>>
			&& N <= small_string_literal<std::decay_t<T>>::max_size;

	template <typename T, size_t N> static constexpr bool is_large_string_literal_v =
		!std::is_pointer_v<T&&>
			&& is_character_type_v<std::decay_t<T>>
			&& N > small_string_literal<std::decay_t<T>>::max_size;

	struct memory_mapped_file
	{

		memory_mapped_file(const char* filename, uint64_t page)
			: base(nullptr)
			, offset(0)
			, file_handle(INVALID_HANDLE_VALUE)
			, mapping_object(INVALID_HANDLE_VALUE)
		{		
			static constexpr auto access = GENERIC_READ | GENERIC_WRITE;
			static constexpr auto share = FILE_SHARE_READ | FILE_SHARE_WRITE;
			static constexpr auto create = CREATE_ALWAYS;
			static constexpr auto attributes = FILE_ATTRIBUTE_NORMAL;

			file_handle = CreateFileA(filename, access, share, nullptr, create, attributes, nullptr);
			if (file_handle == INVALID_HANDLE_VALUE) throw std::exception("CreateFile", GetLastError());

			open_mapping();

			flip_to_page(page);
		}

		~memory_mapped_file()
		{
			UnmapViewOfFile(base);
			CloseHandle(mapping_object);
			CloseHandle(file_handle);
		}

		void flip_to_page(uint64_t page)
		{
			static constexpr auto access = FILE_MAP_ALL_ACCESS;

			UnmapViewOfFile(base);
			auto wide_file_offset = page * page_granularity;
			auto file_offset_high = static_cast<uint32_t>(wide_file_offset >> 32);
			auto file_offset_low = static_cast<uint32_t>(wide_file_offset);

			auto mapped_memory = MapViewOfFile(mapping_object, access, file_offset_high, file_offset_low, page_granularity);
			if (!mapped_memory)
			{
				CloseHandle(mapping_object);
				file_size += file_granularity;
				open_mapping();
				mapped_memory = MapViewOfFile(mapping_object, access, file_offset_high, file_offset_low, page_granularity);
				if (!mapped_memory) throw std::exception("MapViewOfFile", GetLastError());
			}
			base = static_cast<decltype(base)>(mapped_memory);
			offset = 0;
		}

		uint64_t free_space() const { return page_granularity - offset; }

		uint8_t* write(const uint8_t* buffer, size_t buffer_size_in_bytes)
		{
			auto start = base + offset;
			offset += buffer_size_in_bytes;
			std::memcpy(start, buffer, buffer_size_in_bytes);
			return start;
		}

		void write(std::initializer_list<arg> args)
		{
			auto start = reinterpret_cast<const uint8_t*>(args.begin());
			write(start, args.size() * sizeof(arg));
		}

	private:

		void open_mapping()
		{
			static constexpr auto protect = PAGE_READWRITE;

			mapping_object = CreateFileMappingA(file_handle, nullptr, protect, 0, file_size, nullptr);
			if (mapping_object == nullptr) throw std::exception("CreateFileMapping", GetLastError());
		}

		static constexpr auto page_granularity = 1 << 16;
		static constexpr auto file_granularity = std::numeric_limits<DWORD>::max();

		uint8_t* base;
		uint64_t offset;
		HANDLE mapping_object;
		HANDLE file_handle;
		uint64_t file_size = file_granularity;
	};
}