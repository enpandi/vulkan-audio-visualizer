#include "Recorder.h"

#include <cstring>

static bool is_initialized = false;
static bool is_running = false;

// circular queue, only needs one pointer
// (assume the queue is initialized to the full size; this pointer represents both the beginning and the end)
float *recorded_data_ptr;

// this could be different from (uintptr_t(recorded_data)+sizeof(recorded_data)) because i'm aligning to the period size
float *recorded_data_end;

static ma_device device;

constexpr ma_uint32 &period_size_in_frames = device.capture.internalPeriodSizeInFrames;

void av::Recorder::data_callback(ma_device *pDevice, void *const pOutput, void const *const pInput, ma_uint32 frameCount) {
//	todo maybe implement downmixing manually https://dsp.stackexchange.com/q/3581
	float *recorded_data_new_ptr = recorded_data_ptr + period_size_in_frames;
	if (recorded_data_new_ptr == recorded_data_end) {
		recorded_data_ptr = recorded_data_begin;
		recorded_data_new_ptr = recorded_data_ptr + period_size_in_frames;
	}
	memcpy(recorded_data_ptr, pInput, period_size_in_frames * sizeof(float));
	recorded_data_ptr = recorded_data_new_ptr;
}

void av::Recorder::print_recording_devices() {
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

void av::Recorder::init() {
	if (is_initialized)
		throw std::runtime_error("Recorder cannot be initialized because it is already initialized");
	is_initialized = true;
	ma_device_config config = ma_device_config_init(ma_device_type_capture);
	config.capture.format = ma_format_f32;
	config.capture.channels = 1; // todo support multiple channels
	config.sampleRate = 0;
	config.dataCallback = data_callback;
	if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS)
		throw std::runtime_error("miniaudio failed to initialize device");

	memset(recorded_data, 0, sizeof(recorded_data));
	recorded_data_ptr = recorded_data_begin;
	// round the size of the queue down to the nearest period_size_in_frames
	recorded_data_end = recorded_data_begin + MAX_SAMPLES_RECORDED / period_size_in_frames * period_size_in_frames;
}

void av::Recorder::uninit() {
	if (!is_initialized)
		throw std::runtime_error("Recorder cannot be uninitialized because it is already uninitialized");
	is_initialized = false;
	ma_device_uninit(&device);
}

void av::Recorder::start() {
	if (is_running)
		throw std::runtime_error("Recorder cannot be started because it is already running");
	is_running = true;
	ma_device_start(&device);
}

void av::Recorder::stop() {
	if (!is_running)
		throw std::runtime_error("Recorder cannot be stopped because it is already stopped");
	is_running = false;
	ma_device_stop(&device);
}

float av::Recorder::get_sample_rate() { return static_cast<float>(device.sampleRate); }
