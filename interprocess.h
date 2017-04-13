//copyright Eric Budai 2017
#pragma once

#include <Windows.h>
#include <stdint.h>
#include <string>
#include <atomic>

namespace oop
{
	template <typename T, typename... Components> static const char* make_name(T&& component, Components&&... components)
	{
		std::string name = std::to_string(component) + make_name(std::forward<Components>(components)...);
		return name.data();
	}
	template <typename... Components> static const char* make_name(const char* component, Components&&... components)
	{
		std::string name = component;
		name += make_name(std::forward<Components>(components)...);
		return name.data();
	}
	template <typename... Components> static const char* make_name(std::string component, Components&&... components)
	{
		component += make_name(std::forward<Components>(components)...);
		return component.data();
	}
	static const char* make_name() { return ""; }

	template <typename T>
	struct shared
	{
		shared() : pointer(nullptr), mapping_object_handle(INVALID_HANDLE_VALUE), file_handle(INVALID_HANDLE_VALUE) { }

		explicit shared(const char* name) : shared()
		{
			reseat(name);
		}
		
		~shared()
		{
			UnmapViewOfFile(pointer);
			CloseHandle(object);
		}

		shared(const shared& other) : pointer(other.pointer) { copy(other); }
		shared(shared&& other) : pointer(other.pointer) { std::swap(mapping_object_handle, other.mapping_object_handle); }
		shared& operator=(const shared& other) { copy(other); }
		shared& operator=(shared&& other)
		{
			std::swap(pointer, other.pointer);
			std::swap(mapping_object_handle, other.mapping_object_handle);
		}

		T& operator*() { return *pointer; }
		const T& operator*() const { return *pointer; }

		T* operator->() { return pointer; }
		const T* operator->() const { return pointer; }

		T* get() { return pointer; }
		const T* get() const { return pointer; }

		void reseat(const char* name)
		{
			UnmapViewOfFile(pointer);
			CloseHandle(object);

			constexpr SECURITY_ATTRIBUTES* security_attributes = nullptr;
			constexpr auto protection = PAGE_READWRITE;
			constexpr DWORD size_high = sizeof(T) >> 32;
			DWORD size_low = sizeof(T);

			object = CreateFileMappingA(INVALID_HANDLE_VALUE, security_attributes, protection, size_high, size_low, name);
			if (!object) throw std::exception("CreateFileMapping", GetLastError());

			pointer = reinterpret_cast<T*>(MapViewOfFile(mapping_object_handle, FILE_MAP_ALL_ACCESS, 0, 0, 0));
		}

	private:

		inline void copy(const shared& other)
		{
			auto success = DuplicateHandle(GetCurrentProcess(), other.mapping_object_handle, GetCurrentProcess(), &mapping_object_handle, 0, FALSE, DUPLICATE_SAME_ACCESS);
			if (!success) throw std::exception("DuplicateHandle", GetLastError());
		}

		T* pointer;
		HANDLE mapping_object_handle;
	};

	struct page
	{
		enum { pagesize = 1 << 14 };

		page() : base(nullptr), mapping_object(INVALID_HANDLE_VALUE) { }

		void initialize(const char* filename, HANDLE file_handle)
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
		HANDLE mapping_object;
		uint32_t offset;
	};
}