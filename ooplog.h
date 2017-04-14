//copyright Eric Budai 2017
#pragma once

//#include "interprocess.h"
#include <variant>
#include <shared_mutex>
#include <string>
#include <chrono>
#include <functional>
#include <any>
#include <Windows.h>

namespace spry
{
	struct page
	{
		enum { page_granularity = 1 << 16 };

		page(const char* filename, HANDLE file_handle) 
			: base(nullptr)
			, mapping_object(INVALID_HANDLE_VALUE)
		{
			mapping_object = CreateFileMappingA(file_handle, nullptr, PAGE_READWRITE, 0, page_granularity, nullptr);
			if (mapping_object == INVALID_HANDLE_VALUE) throw std::exception("CreateFileMapping", GetLastError());
			flip_to_next_page(filename);
		}

		~page()
		{
			UnmapViewOfFile(base);
			CloseHandle(mapping_object);
		}

		void flip_to_next_page(const char* filename)
		{
			UnmapViewOfFile(base);
			change_page(filename);
		}

		uint64_t free_space() const { return page_granularity - offset; }

		uint8_t* write(const uint8_t* buffer, size_t buffer_size_in_bytes)
		{
			auto start = base + offset;
			std::copy(buffer, buffer + buffer_size_in_bytes, start);
			offset += buffer_size_in_bytes;
			return start;
		}

		template <typename... Args> void write(Args&&... args)
		{
			auto start = base + offset;
			new (start) log::arg[sizeof...(args)]{ std::forward<Args>(args)... };
			offset += sizeof...(args) * sizeof(log::arg);
		}

	private:

		void change_page(const char* filename)
		{
			static uint64_t page_id = 0;

			auto wide_file_offset = page_id * page_granularity;
			auto file_offset_high = static_cast<uint32_t>(wide_file_offset >> 32);
			auto file_offset_low = static_cast<uint32_t>(wide_file_offset);

			auto mapped_memory = MapViewOfFile(mapping_object, FILE_MAP_ALL_ACCESS, file_offset_high, file_offset_low, page_granularity);
			base = reinterpret_cast<decltype(base)>(mapped_memory);
		}

		uint8_t* base;
		uint64_t offset;
		HANDLE mapping_object;		
	};

	struct log
	{
		friend struct page;

		using clock = std::chrono::high_resolution_clock;

		enum class level : int { none = 0, fatal, info, warn, debug, trace };

		log(const char* filename = "spry.binlog")
			: filename(filename)
			, file(INVALID_HANDLE_VALUE) 
		{
			file = CreateFileA(filename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file == INVALID_HANDLE_VALUE) throw std::exception("CreateFile", GetLastError());
		}

		~log() { CloseHandle(file); }

		template <typename... Args> __forceinline void fatal(Args&&... args) 
		{
			if (level == level::none) return;
			write(clock::now(), std::forward<Args>(args)..., newline{}); 
		}
		template <typename... Args> __forceinline void info(Args&&... args) 
		{
			if (level < level::info) return;
			write(clock::now(), std::forward<Args>(args)..., newline{});
		}
		template <typename... Args> __forceinline void warn(Args&&... args) 
		{
			if (level < level::warn) return;
			write({ clock::now(), std::forward<Args>(args)..., newline{} }); 
		}
		template <typename... Args> __forceinline void debug(Args&&... args) 
		{
			if (level < level::debug) return;
			write({ clock::now(), std::forward<Args>(args)..., newline{} }); 
		}
		template <typename... Args> __forceinline void trace(Args&&... args) 
		{
			if (level < level::trace) return;
			write_trace_message({ clock::now(), std::forward<Args>(args)..., newline{} }); 
		}

		level level;

	private:

		class newline { };

		using arg = std::variant<newline, std::chrono::steady_clock::time_point, const char*, uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t, uint64_t, int64_t, float, double>;

		template <typename... Args> __forceinline void write(Args&&... args)
		{
			static thread_local page messages{ filename.data(), file };
			
			write_strings(std::forward<Args>(args)...);
			
			messages.write(std::forward<Args>(args)...);
		}

		template <typename T, typename... Args> __forceinline void write_strings(T&&, Args&&...) { }
		template <typename... Args> __forceinline void write_strings(const char*& string, Args&&... args)
		{
			static thread_local page strings{ (filename + "strings").data(), file };
			static int base_pointer_saver = [&strings]()
			{
				strings.write(&strings.base, sizeof(strings.base)); 
				return 0; 
			}();

			auto length = std::strlen(string);
			if (strings.free_space() < length) strings.flip_to_next_page(filename.data());
			auto pointer = strings.write(reinterpret_cast<const uint8_t*>(string), std::strlen(string));
			string = reinterpret_cast<const char*>(pointer);

			write_strings(std::forward<Args>(args)...);
		}
		
		std::string filename;
		HANDLE file;
	};
}