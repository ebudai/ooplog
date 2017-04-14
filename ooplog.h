//copyright Eric Budai 2017
#pragma once

#define NOMINMAX

#include <variant>
#include <shared_mutex>
#include <string>
#include <chrono>
#include <functional>
#include <new>
#include <Windows.h>

namespace spry
{
	class newline { };

	using arg = std::variant<newline, std::chrono::steady_clock::time_point, const char*, uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t, uint64_t, int64_t, float, double>;

	struct page
	{
		friend struct log;

		enum { page_granularity = 1 << 16 };
		
		page(const char* filename)
			: base(nullptr)
			, file_handle(INVALID_HANDLE_VALUE)
			, mapping_object(INVALID_HANDLE_VALUE)
		{
			file_handle = CreateFileA(filename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file_handle == INVALID_HANDLE_VALUE) throw std::exception("CreateFile", GetLastError());

			mapping_object = CreateFileMappingA(file_handle, nullptr, PAGE_READWRITE, 0, std::numeric_limits<int32_t>::max(), nullptr);
			if (mapping_object == INVALID_HANDLE_VALUE) throw std::exception("CreateFileMapping", GetLastError());
			
			flip_to_next_page();			
		}

		~page()
		{
			UnmapViewOfFile(base);
			CloseHandle(mapping_object);
			CloseHandle(file_handle);
		}

		void flip_to_next_page()
		{
			UnmapViewOfFile(base);
			change_page();
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

		void change_page()
		{
			static uint64_t page_id = 0;

			auto wide_file_offset = page_id++ * page_granularity;
			auto file_offset_high = static_cast<uint32_t>(wide_file_offset >> 32);
			auto file_offset_low = static_cast<uint32_t>(wide_file_offset);

			auto mapped_memory = MapViewOfFile(mapping_object, FILE_MAP_ALL_ACCESS, file_offset_high, file_offset_low, page_granularity);
			if (!mapped_memory) throw std::exception("MapViewOfFile", GetLastError());
			base = reinterpret_cast<decltype(base)>(mapped_memory);
			offset = 0;
		}

		uint8_t* base;
		uint64_t offset;
		HANDLE mapping_object;
		HANDLE file_handle;
	};

	struct log
	{
		friend struct page;

		using clock = std::chrono::high_resolution_clock;

		enum class level : int { none = 0, fatal, info, warn, debug, trace };

		log(const char* filename = "spry.binlog") : filename(filename) { }
		~log() = default;

		template <typename... Args> inline void fatal(Args&&... args)
		{
			if (level == level::none) return; 
			write({ clock::now(), write_strings(std::forward<Args>(args))..., newline{} }); 
		}
		template <typename... Args> inline void info(Args&&... args)
		{
			if (level < level::info) return; 
			write({ clock::now(), write_strings(std::forward<Args>(args))..., newline{} });
		}
		template <typename... Args> inline void warn(Args&&... args)
		{ 
			if (level < level::warn) return; 
			write({ clock::now(), write_strings(std::forward<Args>(args))..., newline{} });
		}
		template <typename... Args> inline void debug(Args&&... args)
		{ 
			if (level < level::debug) return; 
			write({ clock::now(), write_strings(std::forward<Args>(args))..., newline{} });
		}
		template <typename... Args> inline void trace(Args&&... args)
		{
			if (level < level::trace) return; 
			write({ clock::now(), write_strings(std::forward<Args>(args))..., newline{} });
		}

		level level;

	private:

		inline void write(std::initializer_list<arg>&& args)
		{
			static thread_local page messages{ filename.data() };
			
			auto length = args.size() * sizeof(arg);
			if (messages.free_space() < length) {  messages.flip_to_next_page(); }
			messages.write(std::move(args));
		}

		template <typename T> inline T&& write_strings(T&& arg)
		{
			return std::forward<T>(arg);
		}
		
		inline const char* write_strings(const char* string)
		{
			static thread_local page strings{ (filename + "strings").data() };
			
			auto length = std::strlen(string);
			if (strings.free_space() < length) strings.flip_to_next_page();
			auto pointer = strings.write(reinterpret_cast<const uint8_t*>(string), length);
			return reinterpret_cast<const char*>(pointer);
		}
		
		std::string filename;
	};
}