#pragma once

#include "interprocess.h"
#include <variant>
#include <atomic>
#include <string>
#include <filesystem>
#include <Windows.h>

namespace oop
{
	struct log
	{
		enum class loglevel { none, fatal, info, warn, debug, trace };

		log(const char* filename = make_name("ooplog", GetCurrentProcessId(), ".binlog"))
			: filename(filename)
			, file(CreateFileA(filename, GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr))
			, messages(0, filename, file)
			, strings(0, filename, file)
		{ }

		~log() { CloseHandle(file); }

		//todo timing as first parameter
		template <typename... Args> __forceinline void fatal(Args&&... args) const { if (level >= loglevel::fatal) write({ std::forward<Args>(args)..., newline{} }); }
		template <typename... Args> __forceinline void info(Args&&... args) const { if (level >= loglevel::info) write({ std::forward<Args>(args)..., newline{} }); }
		template <typename... Args> __forceinline void warn(Args&&... args) const { if (level >= loglevel::warn) write({ std::forward<Args>(args)..., newline{} }); }
		template <typename... Args> __forceinline void debug(Args&&... args) const { if (level >= loglevel::debug) write({ std::forward<Args>(args)..., newline{} }); }
		template <typename... Args> __forceinline void trace(Args&&... args) const { if (level >= loglevel::trace) write({ std::forward<Args>(args)..., newline{} }); }

		mutable std::atomic<loglevel> level;

	private:

		class newline { };

		using arg = std::variant<uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t, uint64_t, int64_t, float, double, const char*, newline>;
		using arglist = std::initializer_list<arg>;

		__forceinline void write(arglist&& args) const
		{
			write_strings(args);
			auto size = args.size() * sizeof(arg);
			if (messages.free_space() < size) messages.next_page(filename.data(), file);
			messages.write(reinterpret_cast<const uint8_t*>(args.begin()), size);
		}

		__forceinline void write_strings(arglist& args) const
		{
			const auto stringcopy = [this](auto& string) { copy_if_string(string); };

			for (auto& arg : args) std::visit(stringcopy, arg);
		}

		template <typename T> __forceinline void copy_if_string(T& value) const { }
		template <> __forceinline void copy_if_string<const char*>(const char*& string) const
		{
			auto length = std::strlen(string);
			if (strings.free_space() < length) strings.next_page(filename.data(), file);
			auto pointer = strings.write(reinterpret_cast<const uint8_t*>(string), std::strlen(string));
			string = reinterpret_cast<const char*>(pointer);
		}

		std::string filename;

		page messages;
		page strings;

		HANDLE file;
	};
}