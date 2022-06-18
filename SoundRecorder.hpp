#ifndef AUDIO_VISUALIZER_SOUNDRECORDER_HPP
#define AUDIO_VISUALIZER_SOUNDRECORDER_HPP

#include <miniaudio/miniaudio.h>

namespace av {
	class SoundRecorder {
	public:
		// todo const
		float *sample_history_begin, *sample_history_end, *sample_history_ptr;

		explicit SoundRecorder(size_t min_history_samples);

		~SoundRecorder();

		static void print_recording_devices();

		void start();

		void stop();

		[[nodiscard]] float get_sample_rate() const;

	private:
		ma_device device;
		ma_uint32 const &frames_per_period = device.capture.internalPeriodSizeInFrames;
		float *sample_history;

		// when there's new audio, this function will get called
		static void data_callback(ma_device *pDevice, void *pOutput, void const *pInput, ma_uint32 frameCount);
	};
} // av

#endif //AUDIO_VISUALIZER_SOUNDRECORDER_HPP
