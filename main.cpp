#include "Presenter.h"
#include "Recorder.h"

#include <cmath>
#include <iomanip>
#include <iostream>

// idk how to perf on Windows
namespace timer {
	std::chrono::time_point<std::chrono::steady_clock> start_time;

	void start() {
		start_time = std::chrono::steady_clock::now();
	}

	void stop(char const *message) {
		auto stop_time = std::chrono::steady_clock::now();
		auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(stop_time - start_time).count();
		std::cout << std::right << std::setw(15) << static_cast<long double>(nanos) / 1e9 << " s - " << message << std::endl;
	}

	std::chrono::time_point<std::chrono::steady_clock> prev_frame_time;
	uint64_t frame_count = 0;
	uint64_t prev_frame_count;

	void frame() {
		if (!frame_count) {
			prev_frame_time = std::chrono::steady_clock::now();
		}
		++frame_count;
		auto now = std::chrono::steady_clock::now();
		auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now - prev_frame_time).count();
		if (nanos > 1000000000ll) { // one second
			prev_frame_time = now;
			std::cout << frame_count - prev_frame_count << ' ';
			prev_frame_count = frame_count;
		}
	}
}

// todo name the variables better
// todo this main.cpp is super scuffed
// todo cache friendliness
// todo simd

// use long double for all precomputations
// switch to float for the main part


// "normalized device coordinates" https://stackoverflow.com/q/48036410
// frequency at y=0
constexpr long double center_frequency = 440.0l;
// multiplicative difference between adjacent frequencies
long double const frequency_factor = powl(2.0l, 1.0l / 24.0l); // one thing per quarter-tone
// visual space between adjacent frequencies
constexpr long double viewport_y_interval = 0.01l;

std::vector<long double> freqs;
std::vector<long double> y_vals;
size_t num_freqs;

void generate_freqs_and_y_vals() {
	std::vector<long double> lo, hi;
	long double y_dist = viewport_y_interval;
	long double lo_freq = center_frequency / frequency_factor;
	long double hi_freq = center_frequency * frequency_factor;
	while (y_dist < 1.0l) {
		lo.push_back(lo_freq);
		hi.push_back(hi_freq);
		lo_freq /= frequency_factor;
		hi_freq *= frequency_factor;
		y_dist += viewport_y_interval;
	}
	freqs = std::vector<long double>(lo.rbegin(), lo.rend());
	freqs.push_back(center_frequency);
	freqs.insert(freqs.end(), hi.begin(), hi.end());
	assert(lo.size() == hi.size());
	int n = lo.size();
	y_vals.clear();
	for (int i = n; i >= -n; --i)
		y_vals.push_back(viewport_y_interval * i);
	num_freqs = freqs.size();
	assert(freqs.size() == y_vals.size());
}

// https://tauday.com/tau-manifesto
constexpr long double tau = std::numbers::pi_v<long double> * 2.0l;

// goertzel algorithm -- see figure 4 here https://asp-eurasipjournals.springeropen.com/articles/10.1186/1687-6180-2012-56
// i also used euler's formula to avoid complex arithmetic -- exp(j x) = cos(x) + j sin(x)
// this might also work https://www.embedded.com/the-goertzel-algorithm/
// edit: after doing some math, i found out that both algorithms compute the same magnitude pretty much, whatever
/* the algorithm i will be using (mainly similar to embedded.com's):
 * N = number of samples
 * k = N * frequency / sample_rate
 * g = 2 cos(tau k / N)
 *   = 2 cos(tau frequency / sample_rate)
 * s0 = s1 = s2 = 0
 * for i in [0, N-2]
 *   s0 = g * s1 - s2 + x[i]        # the i-th sample, zero-indexed
 *   s2 = s1
 *   s1 = s0
 * s0 = g * s1 - s2 + x[N-1]
 * squared magnitude = s0**2 + s1**2 - 2*s0*s1*cos(tau k / N)
 *                   = s0**2 + s1**2 - s0*s1*g
 * i hope i did the math correctly ............. todo unit testing??
 */
std::vector<float> goertzel_coeffs;

void generate_goertzel_constants(long double sample_rate) {
	// remember to initialize Recorder to get a correct sample rate
	assert(sample_rate > 1000);
	goertzel_coeffs.clear();
	for (long double f : freqs)
		goertzel_coeffs.push_back(2 * cosl(tau * f / sample_rate));
}

std::vector<float> mag; // squared magnitudes (the outputs of the goertzel algorithm)
void compute_goertzel() {
	// todo SSE/AVX ??
	using av::Recorder::recorded_data_begin, av::Recorder::recorded_data_ptr, av::Recorder::recorded_data_end;

	// try this maybe? https://en.wikipedia.org/wiki/Parseval%27s_theorem
	// nvm goertzel samples are incompatible with fourier samples
	for (size_t i = 0; i < num_freqs; ++i) {
		float const &g = goertzel_coeffs[i];
		float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f;
		// recorded_data_ptr is allowed to change at the same time as this, so it should be stored
		// but i decided to out-of-order because the output should be close enough & it's simpler
		// todo what's a proper way to handle this...
		for (float *p = recorded_data_begin; p != recorded_data_end; ++p) {
			s0 = g * s1 - s2 + *p;
			s2 = s1;
			s1 = s0;
		}
		mag[i] = s1 * s1 + s2 * s2 - s1 * s2 * g;
	}
}

int main() {
	try {
		av::Recorder::init();
		generate_freqs_and_y_vals();
		generate_goertzel_constants(av::Recorder::get_sample_rate());
		mag.resize(num_freqs);

		using Vertex = av::Presenter::Vertex;
		std::vector<Vertex> vertices;
		vertices.reserve(num_freqs);
		for (size_t i = 0; i < num_freqs; ++i) {
			vertices.emplace_back(
				Vertex{
					{(i & 1) * 2.0f - 1.0f, static_cast<float>(y_vals[i])},
					{},
				}
			);
		}
		// a lot of this work can probably be put in the shader todo
		// scuffff
		std::vector<Vertex::Color> rainbow(num_freqs * 3);
		for (size_t i = 0; i < rainbow.size(); ++i) {
			float h = i * 6.0f / rainbow.size();
			switch ((int) h) {
				case 0:
					rainbow[i].r = 1.0f;
					rainbow[i].g = h;
					rainbow[i].b = 0.0f;
					break;
				case 1:
					rainbow[i].r = 2.0f - h;
					rainbow[i].g = 1.0f;
					rainbow[i].b = 0.0f;
					break;
				case 2:
					rainbow[i].r = 0.0f;
					rainbow[i].g = 1.0f;
					rainbow[i].b = h - 2.0f;
					break;
				case 3:
					rainbow[i].r = 0.0f;
					rainbow[i].g = 4.0f - h;
					rainbow[i].b = 1.0f;
					break;
				case 4:
					rainbow[i].r = h - 4.0f;
					rainbow[i].g = 0.0f;
					rainbow[i].b = 1.0f;
					break;
				case 5:
					rainbow[i].r = 1.0f;
					rainbow[i].g = 0.0f;
					rainbow[i].b = 6.0f - h;
					break;
			}
		}

		timer::start();
		av::Presenter presenter(vertices.size(), true);
		timer::stop("initialization");

//		presenter.set_vertices(vertices.data());

		av::Recorder::start();
		std::chrono::steady_clock::time_point frame_start = std::chrono::steady_clock::now();
		float delay_max = 0.0f;
		int color_offset = 0;
		while (presenter.is_running()) {
			compute_goertzel();
			float max_mag = 0.0f;
			for (float const &m : mag)
				max_mag = std::max(m, max_mag);
			max_mag = std::max(max_mag, delay_max * 0.95f); // magic number
			delay_max = max_mag;
			for (size_t i = 0; i < num_freqs; ++i) {
				Vertex::Color const &color = rainbow[(i + color_offset) % rainbow.size()];
//				vertices[i].color.r = i == 0 || i == num_freqs - 1 || (mag[i] > mag[i - 1] && mag[i] > mag[i + 1]) ? mag[i] / max_mag : 0.0f;
				vertices[i].color.r = color.r * mag[i] / max_mag;
				vertices[i].color.g = color.g * mag[i] / max_mag;
				vertices[i].color.b = color.b * mag[i] / max_mag;
			}
			++color_offset;
//			std::cout << max_mag << ' ';

			presenter.set_vertices(vertices.data());

			presenter.draw_frame();
			timer::frame();
			std::chrono::steady_clock::time_point frame_end;
			while (std::chrono::duration_cast<std::chrono::nanoseconds>((frame_end = std::chrono::steady_clock::now()) - frame_start).count() < 20000000);
			frame_start = frame_end;
		}
		av::Recorder::stop();
		av::Recorder::uninit();

	} catch (std::system_error &err) {
		std::cerr << "std::system_error: code " << err.code() << ": " << err.what() << std::endl;
		std::exit(EXIT_FAILURE);
	} catch (std::exception &err) {
		std::cerr << "std::exception: " << err.what() << std::endl;
		std::exit(EXIT_FAILURE);
	} catch (...) {
		std::cerr << "unknown error" << std::endl;
		std::exit(EXIT_FAILURE);
	}
}
