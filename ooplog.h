//copyright Eric Budai 2017
#pragma once

#define NOMINMAX

#include <variant>
#include <shared_mutex>
#include <string>
#include <chrono>
#include <functional>
#include <atomic>
#include <Windows.h>

namespace spry
{
	class newline { int ensure_zero = 0; };

	using arg = std::variant
	<
		newline, 
		std::chrono::steady_clock::time_point, 
		const char*, 
		uint8_t, int8_t, 
		uint16_t, int16_t, 
		uint32_t, int32_t, 
		uint64_t, int64_t, 
		float, double
	>;

	struct page
	{
		friend struct memory_mapped_file;

		enum { page_granularity = 1 << 16 };
		
		page()
			: base(nullptr)
			, offset(0)
			, mapping_object(INVALID_HANDLE_VALUE)
		{ }

		~page() { clear(); }

		page& operator=(page&& other)
		{
			std::swap(base, other.base);
			std::swap(offset, other.offset);
			std::swap(mapping_object, other.mapping_object);
			return *this;
		}

		void set(HANDLE file_handle, uint64_t page_number)
		{
			constexpr auto protection = PAGE_READWRITE;
			constexpr auto access = FILE_MAP_ALL_ACCESS;

			auto wide_file_size = (page_number + 1) * page_granularity;
			auto file_size_high = static_cast<uint32_t>(wide_file_size >> 32);
			auto file_size_low = static_cast<uint32_t>(wide_file_size);

			//todo enable huge pages
			mapping_object = CreateFileMappingA(file_handle, nullptr, protection, file_size_high, file_size_low, nullptr);
			if (mapping_object == INVALID_HANDLE_VALUE) throw std::exception("CreateFileMapping", GetLastError());
			
			auto wide_file_offset = page_number * page_granularity;
			auto file_offset_high = static_cast<uint32_t>(wide_file_offset >> 32);
			auto file_offset_low = static_cast<uint32_t>(wide_file_offset);
			
			auto mapped_memory = MapViewOfFile(mapping_object, access, file_offset_high, file_offset_low, page_granularity);
			if (!mapped_memory) throw std::exception("MapViewOfFile", GetLastError());
			
			base = reinterpret_cast<decltype(base)>(mapped_memory);
			offset = 0;			
		}

		void clear()
		{
			UnmapViewOfFile(base);
			CloseHandle(mapping_object);
		}
		
		int64_t free_space() const { return page_granularity - offset; }

		inline uint8_t* write(const uint8_t* buffer, size_t buffer_size_in_bytes)
		{
			auto start = base + offset;
			offset += buffer_size_in_bytes;
			std::copy_n(buffer, buffer_size_in_bytes, start);
			return start;
		}		

	private:

		uint8_t* base;
		HANDLE mapping_object;
		int64_t offset;
	};
	
	struct memory_mapped_file
	{
		memory_mapped_file(const char* filename) : page_number(2)
		{
			constexpr auto access = GENERIC_READ | GENERIC_WRITE;
			constexpr auto sharemode = FILE_SHARE_READ | FILE_SHARE_WRITE;
			constexpr auto creation = CREATE_ALWAYS;
			constexpr auto attributes = FILE_ATTRIBUTE_NORMAL;

			file_handle = CreateFileA(filename, access, sharemode, nullptr, creation, attributes, nullptr);
			if (file_handle == INVALID_HANDLE_VALUE) throw std::exception("CreateFile", GetLastError());

			current_page.set(file_handle, 0);
			next_page.set(file_handle, 1);
		}

		~memory_mapped_file() 
		{ 
			CloseHandle(file_handle); 
		}

		inline void write(std::initializer_list<arg>&& args)
		{
			const auto size = args.size() * sizeof(arg);
			const auto start = reinterpret_cast<const uint8_t*>(args.begin());
			if (current_page.free_space() < size) flip_to_next_page();
			current_page.write(start, size);
		}

		const uint8_t* write(const char*& string)
		{
			const auto size = std::strlen(string);
			if (current_page.free_space() < size) flip_to_next_page();
			return current_page.write(reinterpret_cast<const uint8_t*>(string), size);
		}

	private:

		void flip_to_next_page()
		{
			while (!finished_next_page_creation) std::this_thread::sleep_for(std::chrono::nanoseconds{ 50 });
			finished_next_page_creation = false;
			current_page = std::move(next_page);
			std::thread([this]()
			{
				next_page.clear();
				next_page.set(file_handle, page_number++);
				this->finished_next_page_creation = true;
			}).detach();
		}

		page current_page;
		page next_page;
		HANDLE file_handle;
		uint64_t page_number;
		std::atomic_bool finished_next_page_creation{ true };
	};

	struct log
	{
		friend struct page;

		using clock = std::chrono::high_resolution_clock;

		enum class level : int { none = 0, fatal, info, warn, debug, trace };

		log(std::string filename = "spry.binlog") : filename(filename) { }

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

		std::atomic<level> level;

	private:

		inline void write(std::initializer_list<arg>&& args)
		{
			static thread_local memory_mapped_file file{ filename.data() };
			file.write(std::move(args));
		}

		template <typename T> inline T&& write_strings(T&& arg)
		{
			return std::forward<T>(arg);
		}
		
		inline const char* write_strings(const char* string)
		{
			static thread_local memory_mapped_file file{ (filename + "strings").data() };
			return reinterpret_cast<const char*>(file.write(string));
		}
		
		std::string filename;
	};
}