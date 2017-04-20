#pragma once

#define NOMINMAX

#include <cstdint>
#include <algorithm>
#include <chrono>
#include <variant>
#include <Windows.h>

#define NOT_SET INVALID_HANDLE_VALUE

namespace spry
{
	struct newline { };
	struct string_hash { uint64_t value; };

	using time_point = std::chrono::steady_clock::time_point;
	using arg = std::variant<
		newline, time_point, string_hash, const char*, 
		uint8_t, int8_t, 
		uint16_t, int16_t, 
		uint32_t, int32_t, 
		uint64_t, int64_t, 
		float, double>;

	struct memory_mapped_file
	{

		memory_mapped_file(const char* filename, uint64_t page)
			: base(nullptr)
			, offset(0)
			, file_handle(NOT_SET)
			, mapping_object(NOT_SET)
		{		
			static constexpr auto access = GENERIC_READ | GENERIC_WRITE;
			static constexpr auto share = FILE_SHARE_READ | FILE_SHARE_WRITE;
			static constexpr auto create = CREATE_ALWAYS;
			static constexpr auto attributes = FILE_ATTRIBUTE_NORMAL;
			static constexpr auto protect = PAGE_READWRITE;

			file_handle = CreateFileA(filename, access, share, nullptr, create, attributes, nullptr);
			if (file_handle == NOT_SET) throw std::exception("CreateFile", GetLastError());			

			mapping_object = CreateFileMappingA(file_handle, nullptr, protect, 0, file_granularity, nullptr);
			if (mapping_object == NOT_SET) throw std::exception("CreateFileMapping", GetLastError());

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

		static constexpr auto page_granularity = 1 << 16;
		static constexpr auto file_granularity = std::numeric_limits<DWORD>::max();

		uint8_t* base;
		uint64_t offset;
		HANDLE mapping_object;
		HANDLE file_handle;
	};
}

#undef unset