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

	const char x[5] = "test";
	const auto y = std::is_array_v<decltype(x)>;
	spry::log log;
	nanoseconds max_latency = 0ns;
	nanoseconds avg_latency = 0ns;
	log.level = spry::log::level::info;
	//remove init penalty
	log.info("");
	auto iterations = 100000000;
	for (int i = 0; i < iterations; i++)
	{
		//auto start = high_resolution_clock::now();
		log.info("test");
		/*auto end = high_resolution_clock::now();
		auto latency = end - start;
		avg_latency += latency;
		max_latency = std::max(max_latency, latency);*/
	}

	/*cout << avg_latency.count() / (double)iterations << '\n';

	cout << max_latency.count() << '\n';*/

	//abort();

	return 0;
}



