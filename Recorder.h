//
// Created by panda on 22/05/03.
//

#ifndef AUDIO_VISUALIZER_RECORDER_H
#define AUDIO_VISUALIZER_RECORDER_H

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio/miniaudio.h>

namespace av {
	class Recorder {
	public:
		Recorder();
		~Recorder();
		void start(); // start recording audio samples
		void stop(); // stop recording audio samples
		static void print_devices();
	private:
		ma_device device;
		static void data_callback(ma_device *pDevice, void *const pOutput, void const *const pInput, ma_uint32 frameCount);
	};
}

#endif //AUDIO_VISUALIZER_RECORDER_H
