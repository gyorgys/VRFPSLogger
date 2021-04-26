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

const int MAX_FPS = 160;
const int MAX_GET_FRAME_TIMINGS = 128;
const int MEASUREMENTS_PER_SEC = 2;
const int HISTORY_SIZE = MAX_FPS + MAX_FPS / MEASUREMENTS_PER_SEC;

vr::IVRSystem* _pHMD = NULL;
vr::IVRCompositor* _pCompositor = NULL;
vr::Compositor_FrameTiming _timings_history[HISTORY_SIZE];
vr::Compositor_FrameTiming _get_timings_buff[MAX_GET_FRAME_TIMINGS];
bool _fStop = false;

double frametime(vr::Compositor_FrameTiming& timing) {
	return timing.m_flSystemTimeInSeconds;
}

void log_header(std::ostream* pofile) {
	if (pofile == NULL) {
		return;
	}
	*pofile << "FPS";
	*pofile << ", FrameIndex, NumFramePresents, NumMisPresented, NumDroppedFrames";
	*pofile << ", ReprojectionMotion, PredictedFrames, ThrottledFrames";
	*pofile << ", SystemTimeInMs, PreSubmitGpuMs, PostSubmitGpuMs, TotalRenderGpuMs";
	*pofile << ", CompositorRenderGpuMs, CompositorRenderCpuMs, CompositorIdleCpuMs";
	*pofile << ", ClientFrameIntervalMs";
	*pofile << ", WaitGetPosesCalledMs, NewPosesReadyMs, NewFrameReadyMs";
	*pofile << ", CompositorUpdateStartMs, CompositorUpdateEndMs, CompositorRenderStartMs";
	*pofile << "\n";
	pofile->setf(std::ios::fixed, std::ios::floatfield); // set fixed floating format
	pofile->precision(2); // for fixed format, two decimal places
}

void log_row(std::ostream* pofile, vr::Compositor_FrameTiming& t, int fps) {
	*pofile << fps;
	*pofile << ", " << t.m_nFrameIndex << ", " << t.m_nNumFramePresents << ", " << t.m_nNumMisPresented << ", " << t.m_nNumDroppedFrames;
	*pofile << ", " << ((t.m_nReprojectionFlags & vr::VRCompositor_ReprojectionMotion) ? 1 : 0);
	*pofile << ", " << VR_COMPOSITOR_ADDITIONAL_PREDICTED_FRAMES(t);
	*pofile << ", " << VR_COMPOSITOR_NUMBER_OF_THROTTLED_FRAMES(t);
	*pofile << ", " << t.m_flSystemTimeInSeconds * 1000.0;
	*pofile << ", " << t.m_flPreSubmitGpuMs << ", " << t.m_flPostSubmitGpuMs << ", " << t.m_flTotalRenderGpuMs;
	*pofile << ", " << t.m_flCompositorRenderGpuMs << ", " << t.m_flCompositorRenderCpuMs << ", " << t.m_flCompositorIdleCpuMs;
	*pofile << ", " << t.m_flClientFrameIntervalMs;
	*pofile << ", " << t.m_flWaitGetPosesCalledMs << ", " << t.m_flNewPosesReadyMs << ", " << t.m_flNewFrameReadyMs;
	*pofile << ", " << t.m_flCompositorUpdateStartMs << ", " << t.m_flCompositorUpdateEndMs << ", " << t.m_flCompositorRenderStartMs;
	*pofile << "\n";
}
void refresh_buffer() {
	_get_timings_buff[0].m_nSize = sizeof(vr::Compositor_FrameTiming);

	int result = _pCompositor->GetFrameTimings(_get_timings_buff, MAX_GET_FRAME_TIMINGS);
	if (result < MAX_GET_FRAME_TIMINGS) {
		return;
	}

	// Determine how many new frames are in _get_timings_buff
	int lastHistory = _timings_history[HISTORY_SIZE - 1].m_nFrameIndex;
	int i = MAX_GET_FRAME_TIMINGS;
	while (i > 0 && _get_timings_buff[i-1].m_nFrameIndex > lastHistory) {
		i--;
	}
	int cNewFrames = MAX_GET_FRAME_TIMINGS - i;
	if (cNewFrames == 0) {
		return;
	}
	// move back _timings_history entries to make room for cNewFrames frames
	int j = 0;
	for (; j < HISTORY_SIZE - cNewFrames; j++) {
		_timings_history[j] = _timings_history[j + cNewFrames];
	}
	// append new frames
	for (; j < HISTORY_SIZE; j++) {
		_timings_history[j] = _get_timings_buff[i++];
	}
}

void log_fps(std::ostream* pofile) {
	using namespace std::chrono_literals;
	for (int i = 0; i < HISTORY_SIZE; i++) {
		_timings_history[i].m_nFrameIndex = 0;
		_timings_history[1].m_flSystemTimeInSeconds = 0.0;
	}
	int lastLoggedIndex = 0;
	std::cout << "Avg FPS, Min FPS, Max FPS\n";
	while (!_fStop) {
		refresh_buffer();
		int max = 0;
		int min = 1000;
		double avg = 0;
		int count = 0;
		int currIndex = 0;
		while (_timings_history[currIndex].m_nFrameIndex <= lastLoggedIndex && currIndex < HISTORY_SIZE) {
			currIndex++;
		}
		int oneSecIndex = 0;
		while (currIndex < HISTORY_SIZE) {
			// move oneSecIndex forward
			while (
				frametime(_timings_history[currIndex]) - frametime(_timings_history[oneSecIndex]) >= 1.0) {
				oneSecIndex++;
			}
			int fps = currIndex - oneSecIndex + 1;
			log_row(pofile, _timings_history[currIndex], fps);
			lastLoggedIndex = _timings_history[currIndex].m_nFrameIndex;
			max = max < fps ? fps : max;
			min = min > fps ? fps : min;
			avg += fps;
			count++;
			currIndex++;
		}
		
		std::cout << avg / count << ", " << min << ", " << max << "\n";

		std::this_thread::sleep_for(1000ms / MEASUREMENTS_PER_SEC);
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
		pFile = new std::ofstream(argv[1]);
	}

	_pCompositor = vr::VRCompositor();
	if (!_pCompositor)
	{
		printf("Compositor initialization failed\n");
		vr::VR_Shutdown();
		return false;
	}
	log_header(pFile);
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
