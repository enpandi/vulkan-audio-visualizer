#ifndef AUDIO_VISUALIZER_RECORDER_H
#define AUDIO_VISUALIZER_RECORDER_H

#include <miniaudio/miniaudio.h>

namespace av {
	class Recorder {
	public:
		float *sample_history_begin, *sample_history_end;

		explicit Recorder(size_t sample_history_length);

		~Recorder();

		static void print_recording_devices();

		void start();

		void stop();

		[[nodiscard]] float get_sample_rate() const;

	private:
		float *sample_history, *sample_history_ptr;
		ma_device device;
		ma_uint32 const &frames_per_period = device.capture.internalPeriodSizeInFrames;

		// when there's new audio, this function will get called
		static void data_callback(ma_device *pDevice, void *pOutput, void const *pInput, ma_uint32 frameCount);
	};
}

#endif //AUDIO_VISUALIZER_RECORDER_H
