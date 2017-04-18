#pragma once

#define NOMINMAX

#include <cstdint>
#include <algorithm>
#include <chrono>
#include <variant>
#include <Windows.h>

namespace spry
{
	class newline { };

	struct ct_string
	{
		template <size_t N> constexpr ct_string(const char(&string)[N]) : hash(fnv1(string)) {	}

		uint64_t hash;

	private:

		constexpr uint64_t fnv1(uint64_t h, const char* s)
		{
			return (*s == 0) ? h :
				fnv1((h * 1099511628211ull) ^ static_cast<uint64_t>(*s), s + 1);
		}

		constexpr uint64_t fnv1(const char* s)
		{
			return true ?
				fnv1(14695981039346656037ull, s) :
				throw std::exception{};
		}
	};

	using time_point = std::chrono::steady_clock::time_point;
	using arg = std::variant<newline, time_point, ct_string, const char*, uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t, uint64_t, int64_t, float, double>;

	struct page
	{
		friend struct log;

		static constexpr auto page_granularity = 1 << 16;
		static constexpr auto file_granularity = std::numeric_limits<DWORD>::max();

		page(const char* filename, uint64_t page)
			: base(nullptr)
			, offset(0)
			, file_handle(INVALID_HANDLE_VALUE)
			, mapping_object(INVALID_HANDLE_VALUE)
		{
			constexpr auto access = GENERIC_READ | GENERIC_WRITE;
			constexpr auto share = FILE_SHARE_READ | FILE_SHARE_WRITE;
			constexpr auto create = CREATE_ALWAYS;
			constexpr auto attributes = FILE_ATTRIBUTE_NORMAL;

			file_handle = CreateFileA(filename, access, share, nullptr, create, attributes, nullptr);
			if (file_handle == INVALID_HANDLE_VALUE) throw std::exception("CreateFile", GetLastError());

			constexpr auto protect = PAGE_READWRITE;

			mapping_object = CreateFileMappingA(file_handle, nullptr, protect, 0, file_granularity, nullptr);
			if (mapping_object == INVALID_HANDLE_VALUE) throw std::exception("CreateFileMapping", GetLastError());

			flip_to_page(page);
		}

		~page()
		{
			UnmapViewOfFile(base);
			CloseHandle(mapping_object);
			CloseHandle(file_handle);
		}

		void flip_to_page(uint64_t page)
		{
			UnmapViewOfFile(base);
			auto wide_file_offset = page * page_granularity;
			auto file_offset_high = static_cast<uint32_t>(wide_file_offset >> 32);
			auto file_offset_low = static_cast<uint32_t>(wide_file_offset);

			auto mapped_memory = MapViewOfFile(mapping_object, FILE_MAP_ALL_ACCESS, file_offset_high, file_offset_low, page_granularity);
			if (!mapped_memory) throw std::exception("MapViewOfFile", GetLastError());
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

		uint8_t* base;
		uint64_t offset;
		HANDLE mapping_object;
		HANDLE file_handle;
	};
}
