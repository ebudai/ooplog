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
		make_name(3);
		component += make_name(std::forward<Components>(components)...);
		return component.data();
	}
	static const char* make_name() { return ""; }

	template <typename T>
	struct shared
	{
		shared() = default;

		explicit shared(const char* const name) : pointer(nullptr)
		{
			constexpr SECURITY_ATTRIBUTES* security_attributes = nullptr;
			constexpr auto protection = PAGE_READWRITE;
			constexpr DWORD size_high = 0;
			auto size_low = static_cast<DWORD>(sizeof(T));

			object = CreateFileMappingA(INVALID_HANDLE_VALUE, security_attributes, protection, size_high, size_low, name);
			if (!object) throw std::exception("CreateFileMapping", GetLastError());

			connect();
		}

		shared(const shared& other)
		{
			UnmapViewOfFile(pointer);

			auto success = DuplicateHandle(GetCurrentProcess(), other.object, GetCurrentProcess(), &object, 0, FALSE, DUPLICATE_SAME_ACCESS);
			if (!success) throw std::exception("DuplicateHandle", GetLastError());

			connect();
		}

		shared(shared&& other)
		{
			auto handle = InterlockedExchangePointer(&object, other.object);
			InterlockedExchangePointer(&other.object, handle);
		}

		~shared()
		{
			UnmapViewOfFile(pointer);
			CloseHandle(object);
		}

		shared& operator=(const shared& other)
		{
			UnmapViewOfFile(pointer);

			auto success = DuplicateHandle(GetCurrentProcess(), other.object, GetCurrentProcess(), &object, 0, FALSE, DUPLICATE_SAME_ACCESS);
			if (!success) throw std::exception("DuplicateHandle", GetLastError());

			connect();
		};

		shared& operator=(shared&& other)
		{
			auto handle = InterlockedExchangePointer(&object, other.object);
			InterlockedExchangePointer(&other.object, handle);
		}

		T& operator*() { return *pointer; }
		const T& operator*() const { return *pointer; }

		T* operator->() { return pointer; }
		const T* operator->() const { return pointer; }

		T* get() { return pointer; }
		const T* get() const { return pointer; }

	private:

		void connect()
		{
			auto void_pointer = reinterpret_cast<void*>(pointer);
			auto new_pointer = MapViewOfFile(object, FILE_MAP_ALL_ACCESS, 0, 0, 0);
			InterlockedExchangePointer(&void_pointer, new_pointer);
		}

		T* pointer;
		HANDLE object;
	};

	struct page
	{
		enum { pagesize = 1 << 23 };

		page(uint64_t id, const char* filename, HANDLE file)
			: page_id(id)
			, mapping(INVALID_HANDLE_VALUE)
			, reference_count(make_name("refcount", id, filename))
			, offset(make_name("offset", id, filename))
		{
			InterlockedAdd(reference_count.get(), 1);
			change_page(filename, file);
		}

		~page()
		{
			UnmapViewOfFile(base);
			CloseHandle(mapping);
			InterlockedAdd(reference_count.get(), -1);
		}

		void flip_to_next_page(const char* filename, HANDLE file)
		{
			//todo not threadsafe
			//todo refcount fucked
			UnmapViewOfFile(base);
			page_id++;
			change_page(filename, file);
		}

		uint64_t free_space() const { return pagesize - *offset; }

		uint8_t* write(const uint8_t* buffer, size_t size)
		{
			auto start = InterlockedExchangeAdd64(offset.get(), size);
			std::memcpy(base + start, buffer, size);
			return base + start;
		}

	private:

		void change_page(const char* filename, HANDLE file)
		{
			CloseHandle(mapping);

			mapping = CreateFileMapping(file, nullptr, PAGE_READWRITE, 0, pagesize, nullptr);
			if (!mapping || mapping == INVALID_HANDLE_VALUE) throw std::exception("CreateFileMapping", GetLastError());
			
			uint64_t wide_offset = page_id * pagesize;
			auto offset_high = static_cast<uint32_t>(wide_offset >> 32);
			auto offset_low = static_cast<uint32_t>(wide_offset);
			void* base_pointer = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, offset_high, offset_low, pagesize);
			base = reinterpret_cast<decltype(base)>(base_pointer);
			
			offset = shared<ptrdiff_t>{ make_name("offset", page_id, filename) };
		}

	private:

		std::atomic<uint64_t> page_id;
		uint8_t* base;
		shared<ptrdiff_t> offset;
		shared<long> reference_count;
		HANDLE mapping;
	};
}