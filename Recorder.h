#ifndef AUDIO_VISUALIZER_RECORDER_H
#define AUDIO_VISUALIZER_RECORDER_H

#include <miniaudio/miniaudio.h>

#include <iostream>
#include <stdexcept>


// useful: https://miniaudio.docsforge.com/master/api/ma_device/

// todo handle device change

namespace av {
	// in the interest of performance, i've decided to make everything global.
	// (the callback function cannot capture variables so it would have to access ma_device.)
	// i don't know whether this actually helps performance, todo see if this helps performance
	namespace Recorder {

//		static, extern, inline (C++17):
//		https://stackoverflow.com/a/2328715
//		https://stackoverflow.com/q/14349877
//		https://stackoverflow.com/q/30208685
//		https://stackoverflow.com/q/10422034
//		https://stackoverflow.com/q/38043442
//		don't know if all of the keywords i used here are necessary

// todo make this a parameter or something?
		constexpr inline size_t MAX_SAMPLES_RECORDED = 480 * 16;
		inline float recorded_data[MAX_SAMPLES_RECORDED];

// circular queue, only needs one pointer
// (assume the queue is initialized to the full size; this pointer represents both the beginning and the end)
		inline float *recorded_data_ptr;
// todo is there any reason for this to be mutable?
		constexpr inline float *recorded_data_begin = recorded_data;
// this could be different from (uintptr_t(recorded_data)+sizeof(recorded_data)) because i'm doing alignment
		inline float *recorded_data_end;

// todo these don't behave well if the device changes (probably)

//		extern inline ma_uint32 &sample_rate;


		// when there's new audio, this function will get called
		static void data_callback(ma_device *pDevice, void *const pOutput, void const *const pInput, ma_uint32 frameCount);

		void print_devices();

		void init();

		void uninit();

		void start();

		void stop();

		float get_sample_rate();
	};
}

#endif //AUDIO_VISUALIZER_RECORDER_H
