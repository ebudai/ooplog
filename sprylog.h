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
			filename += /*std::to_string(GetCurrentProcessId()) +*/ ".binlog";
		}

		template <typename... Args> constexpr __forceinline void fatal(Args&&... args)
		{
			if (level == level::none) return;
			write({ clock::now(), convert_arg(std::forward<Args>(args))..., new_line{} });
		}
		template <typename... Args> constexpr __forceinline void info(Args&&... args)
		{
			if (level < level::info) return;
			write({ clock::now(), convert_arg(std::forward<Args>(args))..., new_line{} });
		}
		template <typename... Args> constexpr __forceinline void warn(Args&&... args)
		{
			if (level < level::warn) return;
			write({ clock::now(), convert_arg(std::forward<Args>(args))..., new_line{} });
		}
		template <typename... Args> constexpr __forceinline void debug(Args&&... args)
		{
			if (level < level::debug) return;
			write({ clock::now(), convert_arg(std::forward<Args>(args))..., new_line{} });
		}
		template <typename... Args> constexpr __forceinline void trace(Args&&... args)
		{
			if (level < level::trace) return;
			write({ clock::now(), convert_arg(std::forward<Args>(args))..., new_line{} });
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

		__forceinline uint8_t* write(const uint8_t* buffer, size_t buffer_size)
		{
			static std::atomic<uint64_t> page_counter{ 0 };
			static thread_local memory_mapped_file strings{ ("strings_for_" + filename).data(), page_counter++ };

			if (strings.free_space() < buffer_size) strings.flip_to_page(page_counter++);
			return strings.write(buffer, buffer_size);
		}

		template <typename T> 
		__forceinline std::enable_if_t<!is_char_type_pointer_v<T>, T&&> convert_arg(T&& arg)
		{
			return std::forward<T>(arg);
		}

		__forceinline const char* convert_arg(std::string& string)
		{
			auto data = string.c_str();
			return convert_arg(data);
		}
		
		__forceinline const char* convert_arg(const std::string& string)
		{
			auto data = string.c_str();
			return convert_arg(data);
		}

		template <typename char_t, size_t N>
		constexpr __forceinline
		std::enable_if_t<is_small_string_literal_v<char_t&&, N>, small_string_literal<char_t>> 
		convert_arg(const char_t(&string)[N])
		{
			return { string };
		}

		template <typename char_t, size_t N>
		constexpr __forceinline 
		std::enable_if_t<is_large_string_literal_v<char_t&&, N>, string_hash> convert_arg(char_t(&string)[N])
		{
			return { std::hash<const char_t*>{}(string) };
		}

		template <typename char_t>
		__forceinline std::enable_if_t<is_char_type_pointer_v<char_t>, const char_t> convert_arg(char_t& string)
		{
			const auto length = std::strlen(string);
			const auto pointer = write(reinterpret_cast<const uint8_t*>(string), length);
			return reinterpret_cast<const char_t>(pointer);
		}

		std::string filename;
		std::atomic<level> level;
	};
}