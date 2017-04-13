//copyright Eric Budai 2017
#pragma once

//#include "interprocess.h"
#include <variant>
#include <string>
#include <chrono>
#include <Windows.h>

namespace spry
{
	struct page
	{
		enum { pagesize = 1 << 16 };

		page(const char* filename, HANDLE file_handle) 
			: base(nullptr)
			, mapping_object(INVALID_HANDLE_VALUE)
		{
			mapping_object = CreateFileMappingA(file_handle, nullptr, PAGE_READWRITE, 0, pagesize, nullptr);
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

		uint64_t free_space() const { return pagesize - offset; }

		uint8_t* write(const uint8_t* buffer, size_t buffer_size_in_bytes)
		{
			auto start = base + offset;
			std::memcpy(start, buffer, buffer_size_in_bytes);
			offset += buffer_size_in_bytes;
			return start;
		}

	private:

		void change_page(const char* filename)
		{
			static uint64_t page_id = 0;

			auto wide_file_offset = page_id * pagesize;
			auto file_offset_high = static_cast<uint32_t>(wide_file_offset >> 32);
			auto file_offset_low = static_cast<uint32_t>(wide_file_offset);

			auto mapped_memory = MapViewOfFile(mapping_object, FILE_MAP_ALL_ACCESS, file_offset_high, file_offset_low, pagesize);
			base = reinterpret_cast<decltype(base)>(mapped_memory);
		}

		uint8_t* base;
		uint64_t offset;
		HANDLE mapping_object;		
	};

	struct log
	{
		using clock = std::chrono::high_resolution_clock;

		enum class level { none, fatal, info, warn, debug, trace };

		log(const char* filename = "spry.binlog")
			: filename(filename)
			, file(INVALID_HANDLE_VALUE) 
		{
			file = CreateFileA(filename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file == INVALID_HANDLE_VALUE) throw std::exception("CreateFile", GetLastError());
		}

		~log() { CloseHandle(file); }

		template <typename... Args> __forceinline void fatal(Args&&... args) { write_fatal_message({ clock::now(), std::forward<Args>(args)..., newline{} }); }
		template <typename... Args> __forceinline void info(Args&&... args) { write_info_message({ clock::now(), std::forward<Args>(args)..., newline{} }); }
		template <typename... Args> __forceinline void warn(Args&&... args) { write_warn_message({ clock::now(), std::forward<Args>(args)..., newline{} }); }
		template <typename... Args> __forceinline void debug(Args&&... args) { write_debug_message({ clock::now(), std::forward<Args>(args)..., newline{} }); }
		template <typename... Args> __forceinline void trace(Args&&... args) { write_trace_message({ clock::now(), std::forward<Args>(args)..., newline{} }); }

		static void set_level(level level)
		{
			//fallthrough is intentional
			switch (level)
			{
				case level::trace:	enable(level::trace);
				case level::debug:	enable(level::debug);
				case level::warn:	enable(level::warn);
				case level::info:	enable(level::info);
				case level::fatal:	enable(level::fatal);
				default:			break;
			}

			switch (level)
			{
				case level::none:	disable(level::fatal);
				case level::fatal:	disable(level::info);
				case level::info:	disable(level::warn);
				case level::warn:	disable(level::debug);
				case level::debug:	disable(level::trace);
				default:			break;
			}
		}

	private:

		class newline { };

		using arg = std::variant<newline, std::chrono::steady_clock::time_point, const char*, uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t, uint64_t, int64_t, float, double>;
		using arglist = std::initializer_list<arg>;

		__declspec(noinline) void write_fatal_message(arglist&& args) { write(std::move(args)); }
		__declspec(noinline) void write_info_message(arglist&& args) { write(std::move(args)); }
		__declspec(noinline) void write_warn_message(arglist&& args) { write(std::move(args)); }
		__declspec(noinline) void write_debug_message(arglist&& args) { write(std::move(args)); }
		__declspec(noinline) void write_trace_message(arglist&& args) { write(std::move(args)); }

		__forceinline void write(arglist&& args)
		{
			static thread_local page messages{ filename.data(), file };
			write_strings(args);
			auto size = args.size() * sizeof(arg);
			if (messages.free_space() < size) messages.flip_to_next_page(filename.data());
			messages.write(reinterpret_cast<const uint8_t*>(args.begin()), size);
		}

		__forceinline void write_strings(arglist& args)
		{
			const auto stringcopy = [this](const auto& string) { write_to_string_page(string); };
			for (auto& arg : args) std::visit(stringcopy, arg);
		}

		template <typename T> __forceinline void write_to_string_page(const T& value) { }
		template <> __forceinline void write_to_string_page<const char*&>(const char*& string)
		{
			static thread_local page strings{ filename.data(), file };
			auto length = std::strlen(string);
			if (strings.free_space() < length) strings.flip_to_next_page(filename.data());
			auto pointer = strings.write(reinterpret_cast<const uint8_t*>(string), std::strlen(string));
			string = reinterpret_cast<const char*>(pointer);
		}

		static void enable(level level)
		{
			auto get_first_byte = [](void(log::*function)(arglist&&))
			{
				auto address = reinterpret_cast<uint8_t*&>(function);
				return address[0];
			};

			auto restore_first_byte = [](void(log::*function)(arglist&&), uint8_t byte)
			{
				auto address = reinterpret_cast<uint8_t*&>(function);
				
				DWORD protection = 0;
				auto success = VirtualProtect(address, 1, PAGE_EXECUTE_READWRITE, &protection);
				if (!success) throw std::exception("VirtualProtect (gain write access)", GetLastError());

				address[0] = byte;

				success = VirtualProtect(address, 1, protection, &protection);
				if (!success) throw std::exception("VirtualProtect (lose write access)", GetLastError());
			};

			static auto fatal_code = get_first_byte(&write_fatal_message);
			static auto info_code = get_first_byte(&write_info_message);
			static auto warn_code = get_first_byte(&write_warn_message);
			static auto debug_code = get_first_byte(&write_debug_message);
			static auto trace_code = get_first_byte(&write_trace_message);

			switch (level)
			{
				case level::fatal:	restore_first_byte(&write_fatal_message, fatal_code);	break;
				case level::info:	restore_first_byte(&write_info_message, info_code);		break;
				case level::warn:	restore_first_byte(&write_warn_message, warn_code);		break;
				case level::debug:	restore_first_byte(&write_debug_message, debug_code);	break;
				case level::trace:	restore_first_byte(&write_trace_message, trace_code);	break;
				default:																	break;
			}
		}

		static void disable(level level)
		{
			auto clear_first_byte = [](void(log::*function)(arglist&&))
			{
				auto address = reinterpret_cast<uint8_t*&>(function);

				DWORD protection = 0;
				auto success = VirtualProtect(address, 1, PAGE_EXECUTE_READWRITE, &protection);
				if (!success) throw std::exception("VirtualProtect (gain write access)", GetLastError());

				address[0] = 0xC3; //change first instruction of function to RET

				success = VirtualProtect(address, 1, protection, &protection);
				if (!success) throw std::exception("VirtualProtect (lose write access)", GetLastError());
			};

			switch (level)
			{
				case level::fatal:	clear_first_byte(&write_fatal_message);	break;
				case level::info:	clear_first_byte(&write_info_message);	break;
				case level::warn:	clear_first_byte(&write_warn_message);	break;
				case level::debug:	clear_first_byte(&write_debug_message);	break;
				case level::trace:	clear_first_byte(&write_trace_message);	break;
			}
		}

		std::string filename;
		HANDLE file;
	};
}