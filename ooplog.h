//copyright Eric Budai 2017
#pragma once

#define NOMINMAX

#include "memory_mapped_file.h"
#include "exe_strings.h"
#include <atomic>
#include <mutex>
#include <Windows.h>



namespace spry
{
	//template <typename name = void> //uncomment to allow multiple files per process
	struct log
	{
		using clock = std::chrono::high_resolution_clock;

		log() : filename("spry")
		{
			filename += std::to_string(GetCurrentProcessId()) + ".binlog";
		}

		template <typename... Args> constexpr __forceinline void fatal(Args&&... args)
		{
			if (level == level::none) return;
			write({ clock::now(), write_strings(std::forward<Args>(args))..., newline{} });
		}
		template <typename... Args> constexpr __forceinline void info(Args&&... args)
		{
			if (level < level::info) return;
			write({ clock::now(), write_strings(std::forward<Args>(args))..., newline{} });
		}
		template <typename... Args> constexpr __forceinline void warn(Args&&... args)
		{
			if (level < level::warn) return;
			write({ clock::now(), write_strings(std::forward<Args>(args))..., newline{} });
		}
		template <typename... Args> constexpr __forceinline void debug(Args&&... args)
		{
			if (level < level::debug) return;
			write({ clock::now(), write_strings(std::forward<Args>(args))..., newline{} });
		}
		template <typename... Args> constexpr __forceinline void trace(Args&&... args)
		{
			if (level < level::trace) return;
			write({ clock::now(), write_strings(std::forward<Args>(args))..., newline{} });
		}

		void set_to_none() { set_level(level::none); }
		void set_to_fatal() { set_level(level::fatal); }
		void set_to_info() { set_level(level::info); }
		void set_to_warn() { set_level(level::warn); }
		void set_to_debug() { set_level(level::debug); }
		void set_to_trace() { set_level(level::trace); }

	private:

		enum class level : int { none = 0, fatal, info, warn, debug, trace };

		inline void set_level(level new_level) { level = new_level; }

		__forceinline void write(std::initializer_list<arg>&& args)
		{
			static std::atomic<uint64_t> page_counter{ 0 };
			static thread_local memory_mapped_file messages{ filename.data(), page_counter++ };

			const auto length = args.size() * sizeof(arg);
			if (messages.free_space() < length) { messages.flip_to_page(page_counter++); }
			messages.write(std::move(args));
		}

		template <typename T> 
		__forceinline std::enable_if_t<!std::is_pointer_v<T>, T&&> write_strings(T&& arg)
		{
			return std::forward<T>(arg);
		}

		__forceinline const char* write_strings(std::string& string)
		{
			auto data = string.c_str();
			return write_strings(data);
		}
		
		__forceinline const char* write_strings(const std::string& string)
		{
			auto data = string.c_str();
			return write_strings(data);
		}

		template <typename T, size_t N> 
		constexpr __forceinline std::enable_if_t<!std::is_pointer_v<T&&>, string_hash> write_strings(T(&string)[N])
		{
			return { std::hash<const char*>{}(string) };
		}

		template <typename T, size_t N>
		constexpr __forceinline std::enable_if_t<!std::is_pointer_v<T&&>, string_hash> write_strings(const T(&string)[N])
		{
			return { std::hash<const char*>{}(string) };
		}

		template <typename T>
		__forceinline std::enable_if_t<std::is_pointer_v<T>, const char*> write_strings(T& string)
		{
			static std::atomic<uint64_t> page_counter{ 0 };
			static thread_local memory_mapped_file strings{ (filename + "strings").data(), page_counter++ };

			const auto length = std::strlen(string);
			if (strings.free_space() < length) strings.flip_to_page(page_counter++);
			const auto pointer = strings.write(reinterpret_cast<const uint8_t*>(string), length);
			return reinterpret_cast<const char*>(pointer);
		}

		std::string filename;
		std::atomic<level> level;
	};
}