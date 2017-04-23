//copyright Eric Budai 2017
#define NOMINMAX

#include <windows.h>
#include <algorithm>
#include "sprylog.h"

#include <iostream>

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(hInstance);
	UNREFERENCED_PARAMETER(nCmdShow);

	using namespace std;
	using namespace std::chrono;

	//void* base = GetModuleHandle(nullptr);

	auto strings = spry::extract_strings_from_process<char>();

	spry::log log;
	nanoseconds max_latency = 0ns;
	nanoseconds avg_latency = 0ns;
	log.set_to_info();
	//remove init penalty
	std::string thismightwork = "yay";
	const std::string thismightwork2 = "yay";
	auto data = thismightwork.c_str();
	auto data2 = thismightwork.data();
	log.info("small", "long literal", data, thismightwork, thismightwork2, data2, 7);
	log.info("asdfasdfasdf", 0);
	log.info(data);
	log.info(thismightwork);

	int iterations = 50000000;
	for (int i = 0; i < iterations; i++)
	{
		auto start = high_resolution_clock::now();
		log.info("test", 7);
		log.info("test", 7);
		log.info("test", 7);
		auto end = high_resolution_clock::now();
		auto latency = end - start;
		avg_latency += latency;
		max_latency = std::max(max_latency, latency);
	}

	avg_latency /= iterations;

	cout << avg_latency.count() << '\n';

	cout << max_latency.count() << '\n';

	//abort();

	return 0;
}