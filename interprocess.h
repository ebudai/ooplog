#pragma once

#include <Windows.h>
#include <stdint.h>
#include <mutex>

namespace oop
{
	template <typename T, typename... Components> const char* make_name(T&& component, Components&&... components)
	{
		std::string name = std::to_string(component) + make_name(std::forward<Components>(components)...);
		return name.data();
	}
	template <typename... Components> const char* make_name(const char* component, Components&&... components)
	{
		std::string name = component;
		name += make_name(std::forward<Components>(components)...);
		return name.data();
	}
	template <typename... Components> const char* make_name(std::string component, Components&&... components)
	{
		component += make_name(std::forward<Components>(components)...);
		return component.data();
	}
	const char* make_name() { return ""; }

	struct mapping
	{
		mapping() = delete;

		explicit mapping(const char* name, size_t size)
		{
			constexpr SECURITY_ATTRIBUTES* security_attributes = nullptr;
			constexpr auto protection = PAGE_READWRITE;
			constexpr DWORD size_high = 0;
			auto size_low = static_cast<DWORD>(size);
			
			object = CreateFileMappingA(INVALID_HANDLE_VALUE, security_attributes, protection, size_high, size_low, name);
			if (!object) throw std::exception("CreateFileMapping", GetLastError());
		}

		mapping(const mapping& other)
		{
			auto success = DuplicateHandle(GetCurrentProcess(), other.object, GetCurrentProcess(), &object, 0, FALSE, DUPLICATE_SAME_ACCESS);
			if (!success) throw std::exception("DuplicateHandle", GetLastError());
		}

		mapping(mapping&& other)
		{
			std::swap(object, other.object); 
		}

		~mapping()
		{
			CloseHandle(object);
		}

		mapping& operator=(const mapping& other)
		{
			auto success = DuplicateHandle(GetCurrentProcess(), other.object, GetCurrentProcess(), &object, 0, FALSE, DUPLICATE_SAME_ACCESS);
			if (!success) throw std::exception("DuplicateHandle", GetLastError());
		}

		mapping& operator=(mapping&& other)
		{
			std::swap(object, other.object);
		}

		operator HANDLE() const { return object; }

	private:

		HANDLE object;
	};

	template <typename T>
	struct shared
	{
		shared() = default;
		
		explicit shared(const char* name) : file_mapping(name, sizeof(T)), pointer(nullptr)
		{
			connect();
		}

		shared(const shared&) = default;
		shared(shared&&) = default;

		~shared()
		{
			UnmapViewOfFile(pointer); 
		}

		shared& operator=(const shared& other)
		{
			UnmapViewOfFile(pointer);
			file_mapping = other.file_mapping;
			connect();
		};

		shared& operator=(shared&&) = default;

		T*& operator->()
		{
			return pointer; 
		}
		
		operator T*() 
		{
			return pointer; 
		}
		
		operator const T* const() const 
		{
			return pointer; 
		}

	private:

		void connect()
		{
			pointer = reinterpret_cast<T*>(MapViewOfFile(file_mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0));
		}

		T* pointer;
		mapping file_mapping;
	};

	struct page
	{
		enum { size = 1 << 23 };

		page(uint64_t id, const char* filename, HANDLE file)
			: id(id)
			, mapping(INVALID_HANDLE_VALUE)
			, reference_count(make_name("refcount", id, filename))
			, offset(make_name("offset", id, filename))
		{
			change_page(id, filename, file);
		}

		~page()
		{
			clean();			
		}

		void next_page(const char* filename, HANDLE file) const
		{
			static std::mutex mutex;
			std::lock_guard<std::mutex> lock(mutex);

			clean();
			change_page(++id, filename, file);
		}

		uint64_t free_space() const { return size - *offset; }

		void* write(const uint8_t* buffer, size_t size) const
		{
			auto start = InterlockedExchangeAdd64(offset, size);
			std::memcpy(base + start, buffer, size);
			return base + start;
		}

	private:

		void clean() const
		{
			UnmapViewOfFile(base);
			CloseHandle(mapping);
			InterlockedAdd(reference_count, -1);
		}

		void change_page(uint64_t id, const char* filename, HANDLE file) const
		{
			InterlockedAdd(reference_count, 1);
			
			{
				mapping = CreateFileMapping(file, nullptr, PAGE_READWRITE, 0, size, nullptr);
				if (!mapping || mapping == INVALID_HANDLE_VALUE) throw std::exception("CreateFileMapping", GetLastError());
			}
			
			{
				uint64_t wide_offset = id * size;
				auto offset_high = static_cast<uint32_t>(wide_offset >> 32);
				auto offset_low = static_cast<uint32_t>(wide_offset);
				void* base_pointer = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, offset_high, offset_low, size);
				base = reinterpret_cast<decltype(base)>(base_pointer);
			}
			
			offset = shared<ptrdiff_t>{ make_name("offset", id, filename) };
		}

	private:

		mutable uint64_t id;
		mutable uint8_t* base;
		mutable shared<ptrdiff_t> offset;
		mutable shared<long> reference_count;
		mutable HANDLE mapping;
	};
}