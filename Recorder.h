#ifndef AUDIO_VISUALIZER_RECORDER_H
#define AUDIO_VISUALIZER_RECORDER_H

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio/miniaudio.h>

#include <iostream>
#include <stdexcept>

// i would put the function definitions in a Recorder.cpp but that causes multiple-definition errors of miniaudio stuff
// further reading:
// https://stackoverflow.com/q/29196460
// https://stackoverflow.com/q/14425262
// https://stackoverflow.com/q/212006

unsigned long long ct = 0;
namespace av {
	class Recorder {
	public:
		Recorder() {
			ma_device_config config = ma_device_config_init(ma_device_type_capture);
			config.capture.format = ma_format_f32;
			config.capture.channels = 0;
			config.sampleRate = 0;
			config.dataCallback = data_callback;
			if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS)
				throw std::runtime_error("miniaudio failed to initialize device");
		}

		~Recorder() { ma_device_uninit(&device); }

		void start() { ma_device_start(&device); } // start recording audio samples

		void stop() { ma_device_stop(&device); } // stop recording audio samples

		static void print_devices() {
			ma_context context;
			if (ma_context_init(nullptr, 0, nullptr, &context) != MA_SUCCESS)
				throw std::runtime_error("miniaudio failed to initialize context");

			ma_device_info *pPlaybackDeviceInfos, *pCaptureDeviceInfos;
			ma_uint32 playbackDeviceCount, captureDeviceCount;
			if (ma_context_get_devices(
				&context, &pPlaybackDeviceInfos, &playbackDeviceCount, &pCaptureDeviceInfos, &captureDeviceCount
			) != MA_SUCCESS)
				throw std::runtime_error("miniaudio failed to get audio devices");

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

	private:
		ma_device device;

		static void data_callback(ma_device *pDevice, void *const pOutput, void const *const pInput, ma_uint32 frameCount) {
			ct += frameCount;
		} // todo
	};
}

#endif //AUDIO_VISUALIZER_RECORDER_H
