// VRFPSLogger.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <openvr.h>
#include <chrono>
#include <thread>
#include <fstream>
#include <ctime>
#include <sstream>
#include <iomanip>

const int MAX_FRAMES = 100;
const int MEASUREMENTS_PER_MIN = 10;

vr::IVRSystem* _pHMD = NULL;
vr::IVRCompositor* _pCompositor = NULL;
vr::Compositor_FrameTiming _timings[MAX_FRAMES];
bool _fStop = false;

int get_fps() {
	
	_timings[0].m_nSize = sizeof(vr::Compositor_FrameTiming);

	int result = _pCompositor->GetFrameTimings(_timings, MAX_FRAMES);
	if (result < MAX_FRAMES) {
		return 0;
	}
	int curIndex = MAX_FRAMES - 1;
	int countFrames = 0;
	double lastFrameTimeSec = _timings[curIndex].m_flSystemTimeInSeconds;
	double curFrameTimeSec = lastFrameTimeSec;

	while (curIndex > 0 && lastFrameTimeSec - curFrameTimeSec < 1.0) {
		curIndex--;
		curFrameTimeSec = _timings[curIndex].m_flSystemTimeInSeconds;
		countFrames++;
	}
	
	return countFrames;
}

void log_fps(std::ostream* pofile) {
	using namespace std::chrono_literals;
	int fpss[MEASUREMENTS_PER_MIN];
	int i = 0;
	while (!_fStop) {
		fpss[i++] = get_fps();
		if (i >= MEASUREMENTS_PER_MIN) {
			i = 0;
			int min = 1000;
			int max = 0;
			double avg = 0.0;
			for (int j = 0; j < MEASUREMENTS_PER_MIN; j++) {
				min = fpss[j] < min ? fpss[j] : min;
				max = fpss[j] > max ? fpss[j] : max;
				avg += fpss[j];
			}
			avg /= MEASUREMENTS_PER_MIN;
			
			std::time_t t = std::time(nullptr);
			double fps = get_fps();
			std::stringstream strLine;
			strLine << t << ", " << std::put_time(std::localtime(&t), "%F %T");
			strLine << ", " << min << ", " << max << ", " << avg << "\n";
			std::cout << strLine.str();
			if (pofile) {
				*pofile << strLine.str();
			}
		}
		std::this_thread::sleep_for(100ms);
	}
}

int main(int argc, char* argv[])
{
	std::cout << "OpenVR FPS Logger\n";
	// try to connect with openvr as an background application
	vr::EVRInitError eError = vr::VRInitError_None;
	_pHMD = vr::VR_Init(&eError, vr::VRApplication_Background);
	char buf[1024];

	if (eError != vr::VRInitError_None)
	{
		_pHMD = NULL;
		sprintf_s(buf, sizeof(buf), "Unable to init VR runtime: %s", vr::VR_GetVRInitErrorAsEnglishDescription(eError));
		printf_s(buf);
		exit(EXIT_FAILURE);
	}
	std::cout << "VR Runtime initialized\n";

	// Open output file
	std::ofstream* pFile = NULL;
	if (argc > 1) {
		std::cout << "Logging to " << argv[1] << "\n";
		pFile = new std::ofstream(argv[1], std::ofstream::app);
	}

	_pCompositor = vr::VRCompositor();
	if (!_pCompositor)
	{
		printf("Compositor initialization failed\n");
		vr::VR_Shutdown();
		return false;
	}
	std::thread loggerThread(log_fps, pFile);
	std::cout << "Press any key to stop\n";
	std::cin.get();
	std::cout << "Exiting...\n";
	_fStop = true;
	loggerThread.join();
	if (pFile) {
		pFile->close();
	}
	vr::VR_Shutdown();
	std::cout << "Bye\n";
}
