//copyright Eric Budai 2017
#define NOMINMAX

#include <windows.h>
#include <algorithm>
#include "ooplog.h"

#include <iostream>

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

	using namespace std;
	using namespace std::chrono;

	spry::log log;
	nanoseconds max_latency = 0ns;
	nanoseconds avg_latency = 0ns;
	log.level = spry::log::level::info;
	//remove init penalty
	log.info("", 0);
	auto iterations = 1000000;
	for (int i = 0; i < iterations; i++)
	{
		auto start = high_resolution_clock::now();
		log.info("test", 7);
		auto end = high_resolution_clock::now();
		auto latency = end - start;
		avg_latency += latency;
		max_latency = std::max(max_latency, latency);
	}

	cout << avg_latency.count() / (double)iterations << '\n';

	cout << max_latency.count() << '\n';

	abort();

	return 0;
}



