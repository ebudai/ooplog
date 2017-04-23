#pragma once

#include <type_traits>

namespace spry
{
	template <typename T> static constexpr bool is_character_type_v =
		std::is_same_v<T, char>
		|| std::is_same_v<T, unsigned char>
		|| std::is_same_v<T, wchar_t>
		|| std::is_same_v<T, char16_t>
		|| std::is_same_v<T, char32_t>;

	template <typename T> static constexpr bool is_char_type_pointer_v =
		std::is_same_v<std::remove_reference_t<T>, char*>
		|| std::is_same_v<std::remove_reference_t<T>, unsigned char*>
		|| std::is_same_v<std::remove_reference_t<T>, wchar_t*>
		|| std::is_same_v<std::remove_reference_t<T>, char16_t*>
		|| std::is_same_v<std::remove_reference_t<T>, char32_t*>
		|| std::is_same_v<std::remove_reference_t<T>, const char*>
		|| std::is_same_v<std::remove_reference_t<T>, const unsigned char*>
		|| std::is_same_v<std::remove_reference_t<T>, const wchar_t*>
		|| std::is_same_v<std::remove_reference_t<T>, const char16_t*>
		|| std::is_same_v<std::remove_reference_t<T>, const char32_t*>;

	template <typename T, size_t N> static constexpr bool is_small_string_literal_v =
		!std::is_pointer_v<T&&>
		&& is_character_type_v<std::decay_t<T>>
		&& N <= small_string_literal<std::decay_t<T>>::max_size;

	template <typename T, size_t N> static constexpr bool is_large_string_literal_v =
		!std::is_pointer_v<T&&>
		&& is_character_type_v<std::decay_t<T>>
		&& N > small_string_literal<std::decay_t<T>>::max_size;
}
