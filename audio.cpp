#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio/miniaudio.h>

#include "audio.h"

#include <iostream>


static void data_callback(ma_device *pDevice, void *const pOutput, void const *const pInput, ma_uint32 frameCount) {
	std::cout << frameCount << ' ';
}

ma_device device;

void audio_init() {
	ma_device_config config = ma_device_config_init(ma_device_type_capture);
	config.capture.format = ma_format_f32;
	config.capture.channels = 0;
	config.sampleRate = 0;
	config.dataCallback = data_callback;
	if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS) {
		std::cerr << "miniaudio failed to initialize device" << std::endl;
		std::exit(EXIT_FAILURE);
	}
}

void audio_uninit() { ma_device_uninit(&device); }

void audio_start() { ma_device_start(&device); }

void audio_stop() { ma_device_stop(&device); }

void audio_print_devices() {
	ma_context context;
	if (ma_context_init(nullptr, 0, nullptr, &context) != MA_SUCCESS) {
		std::cerr << "miniaudio failed to initialize context" << std::endl;
		std::exit(EXIT_FAILURE);
	}

	ma_device_info *pPlaybackDeviceInfos, *pCaptureDeviceInfos;
	ma_uint32 playbackDeviceCount, captureDeviceCount;
	if (ma_context_get_devices(
		&context, &pPlaybackDeviceInfos, &playbackDeviceCount, &pCaptureDeviceInfos, &captureDeviceCount
	) != MA_SUCCESS) {
		std::cerr << "miniaudio failed to get audio devices" << std::endl;
		std::exit(EXIT_FAILURE);
	}

	std::cout << "playback devices (* denotes default):" << std::endl;
	for (ma_uint32 i = 0; i < playbackDeviceCount; ++i) {
		std::cout << '\t';
		if (pPlaybackDeviceInfos[i].isDefault)
			std::cout << "* ";
		std::cout << pPlaybackDeviceInfos[i].name << std::endl;
	}
	std::cout << "capture devices (* denotes default):" << std::endl;
	for (ma_uint32 i = 0; i < captureDeviceCount; ++i) {
		std::cout << '\t';
		if (pCaptureDeviceInfos[i].isDefault)
			std::cout << "* ";
		std::cout << pCaptureDeviceInfos[i].name << std::endl;
	}
}
