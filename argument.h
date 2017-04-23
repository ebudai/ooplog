#pragma once

#include <cstdint>
#include <chrono>
#include <variant>

namespace spry
{
	struct new_line { };
	struct string_hash { uint64_t value; };
	using time_point = std::chrono::steady_clock::time_point;

	template <typename char_t> struct small_string_literal
	{
		template <size_t N, typename = std::enable_if_t<N <= max_size>>
		small_string_literal(const char_t(&string)[N])
			: storage(*reinterpret_cast<const uint64_t*>(&string[0]))
		{ }

		operator const char_t*() { return new (&storage) const char_t[max_size]; }

		static constexpr auto max_size = sizeof(uint64_t) / sizeof(char_t);

	private:

		uint64_t storage;
	};

	using error_code = unsigned long;

	using arg = std::variant<
		new_line, time_point, error_code,
		const char*, const wchar_t*, const char16_t*, const char32_t*,
		string_hash,
		small_string_literal<char>,
		small_string_literal<wchar_t>,
		small_string_literal<char16_t>,
		small_string_literal<char32_t>,
		uint8_t, int8_t,
		uint16_t, int16_t,
		uint32_t, int32_t,
		uint64_t, int64_t,
		float, double>;
}