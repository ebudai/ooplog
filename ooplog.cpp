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

	auto strings = spry::extract_embedded_strings_from_current_module();

	spry::log log;
	nanoseconds max_latency = 0ns;
	nanoseconds avg_latency = 0ns;
	log.set_to_info();
	//remove init penalty
	std::string thismightwork = "yay";
	auto data = thismightwork.c_str();
	log.info(" ", 0);
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