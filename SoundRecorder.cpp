#include "SoundRecorder.hpp"

#include <iostream>

// useful: https://miniaudio.docsforge.com/master/api/ma_device/

av::SoundRecorder::SoundRecorder(size_t min_history_samples) {
	ma_device_config config = ma_device_config_init(ma_device_type_capture);
	config.sampleRate = 0;
	config.dataCallback = data_callback;
	config.pUserData = this;
	config.capture.format = ma_format_f32;
	config.capture.channels = 1; // todo multiple channels
	if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS)
		throw std::runtime_error("miniaudio failed to initialize device");
	// round the history length up to the nearest period_size_in_frames
	size_t sample_history_length =
		(min_history_samples + frames_per_period - 1) / frames_per_period * frames_per_period;
	sample_history = new float[sample_history_length](); // zero-initialized
	sample_history_ptr = sample_history_begin = sample_history;
	sample_history_end = sample_history_begin + sample_history_length;
}

av::SoundRecorder::~SoundRecorder() {
	delete[] sample_history;
	ma_device_uninit(&device);
}

void av::SoundRecorder::print_recording_devices() {
	ma_context context;
	if (ma_context_init(nullptr, 0, nullptr, &context) != MA_SUCCESS)
		throw std::runtime_error("miniaudio failed to initialize context");

	ma_device_info *pPlaybackDeviceInfos, *pCaptureDeviceInfos;
	ma_uint32 playbackDeviceCount, captureDeviceCount;
	if (ma_context_get_devices(
		&context, &pPlaybackDeviceInfos, &playbackDeviceCount, &pCaptureDeviceInfos, &captureDeviceCount
	) != MA_SUCCESS)
		throw std::runtime_error("miniaudio failed to get audio devices");

	std::cout << "capture devices (* denotes default):" << std::endl;
	for (ma_uint32 i = 0; i < captureDeviceCount; ++i) {
		std::cout << '\t';
		if (pCaptureDeviceInfos[i].isDefault)
			std::cout << "* ";
		std::cout << pCaptureDeviceInfos[i].name << std::endl;
	}
}

void av::SoundRecorder::start() {
	if (ma_device_start(&device) != MA_SUCCESS)
		throw std::runtime_error("miniaudio failed to start device");
}

void av::SoundRecorder::stop() {
	if (ma_device_stop(&device) != MA_SUCCESS)
		throw std::runtime_error("miniaudio failed to stop device");
}

float av::SoundRecorder::get_sample_rate() const { return static_cast<float>(device.sampleRate); }

void av::SoundRecorder::data_callback(
	ma_device *pDevice,
	void *const pOutput,
	void const *const pInput,
	ma_uint32 frameCount
) {
//	todo maybe implement downmixing manually: https://dsp.stackexchange.com/q/3581
	auto *rec = static_cast<SoundRecorder *>(pDevice->pUserData);
	float *sample_history_ptr_new = rec->sample_history_ptr + frameCount;
	if (sample_history_ptr_new >= rec->sample_history_end) {
		rec->sample_history_ptr = rec->sample_history_begin;
		sample_history_ptr_new = rec->sample_history_ptr + frameCount;
	}
	auto const input = static_cast<float const *>(pInput);
	std::copy(input, input + frameCount, rec->sample_history_ptr);
	rec->sample_history_ptr = sample_history_ptr_new;
}
