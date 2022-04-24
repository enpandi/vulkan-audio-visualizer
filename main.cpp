#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <iostream>
#include <iomanip>


void enumerate_devices() {
	ma_context context;
	if (ma_context_init(nullptr, 0, nullptr, &context) != MA_SUCCESS)
		exit(EXIT_FAILURE);

	ma_device_info *pPlaybackInfos;
	ma_uint32 playbackCount;
	ma_device_info *pCaptureInfos;
	ma_uint32 captureCount;
	if (ma_context_get_devices(&context, &pPlaybackInfos, &playbackCount, &pCaptureInfos, &captureCount) != MA_SUCCESS)
		exit(EXIT_FAILURE);


	for (ma_uint32 i = 0; i < playbackCount; i += 1) {
		ma_device_info device = pPlaybackInfos[i];
		std::cout << "ind=" << i << ", name=" << device.name << ", isDefault=" << device.isDefault
		<< ", nativeDataFormatCount=" << device.nativeDataFormatCount << std::endl;
		for (ma_uint32 j = 0; j < device.nativeDataFormatCount; ++j) {
			std::cout << '\t' << "ind=" << j << ", format=" << device.nativeDataFormats[j].format << ", channels="
			<< device.nativeDataFormats[j].channels << ", sampleRate=" << device.nativeDataFormats[j].sampleRate
			<< ", flags" << device.nativeDataFormats[j].flags << std::endl;
		}
	}

	for (ma_uint32 i = 0; i < captureCount; i += 1) {
		ma_device_info device = pCaptureInfos[i];
		std::cout << "ind=" << i << ", name=" << device.name << ", isDefault=" << device.isDefault
		          << ", nativeDataFormatCount=" << device.nativeDataFormatCount << std::endl;
		for (ma_uint32 j = 0; j < device.nativeDataFormatCount; ++j) {
			std::cout << '\t' << "ind=" << j << ", format=" << device.nativeDataFormats[j].format << ", channels="
			          << device.nativeDataFormats[j].channels << ", sampleRate=" << device.nativeDataFormats[j].sampleRate
			          << ", flags" << device.nativeDataFormats[j].flags << std::endl;
		}
	}

}

void data_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount) {
	auto *inp = (float*) pInput;
	float sum = 0.0f;
	for (ma_uint32 iFrame = 0; iFrame < frameCount; ++iFrame)
		sum += std::abs(inp[iFrame]);
	unsigned int loudness = sum;
	for (unsigned int i=0; i<loudness; ++i) std::cout << '-';
	std::cout << std::endl;
}

int main() {


	std::ios::sync_with_stdio(false);
	enumerate_devices();



	ma_device_config config = ma_device_config_init(ma_device_type_capture);
	config.capture.format = ma_format_f32;
	config.capture.channels = 0;
	config.sampleRate = 0;
	config.dataCallback = data_callback;
//	config.pUserData = pMyCustomData;

	ma_device device;
	if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS)
		return EXIT_FAILURE;

	std::cout << std::setprecision(20);

	std::cin.ignore();

	ma_device_start(&device);

	std::cin.ignore();

	ma_device_uninit(&device);
	return EXIT_SUCCESS;
}
