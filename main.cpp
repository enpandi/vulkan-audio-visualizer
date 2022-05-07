#include "Presenter.h"
#include "Recorder.h"

#include <cmath>
#include <iomanip>
#include <iostream>

// perf doesn't work
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

	// for counting fps
	std::chrono::time_point<std::chrono::steady_clock> prev_frame_time;
	uint64_t frame_count = 0;
	uint64_t prev_frame_count;

	void fps() {
		if (!frame_count)
			prev_frame_time = std::chrono::steady_clock::now();
		++frame_count;
		auto now = std::chrono::steady_clock::now();
		auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now - prev_frame_time).count();
		if (nanos > 1000000000) { // one second
			prev_frame_time = now;
			std::cout << frame_count - prev_frame_count << ' ';
			prev_frame_count = frame_count;
		}
	}
}

// todo cache locality
// todo simd

// use long double for all precomputations
// switch to float for the main part

// "normalized device coordinates" https://stackoverflow.com/q/48036410

// frequency at the coordinate y=0
constexpr long double center_frequency = 440.0l;

// multiplicative difference between adjacent frequencies
long double const frequency_factor = powl(2.0l, 1.0l / 24.0l);
// frequencies are spaced a quarter-tone apart

// visual space between adjacent frequencies, based on normalized device coordinates
constexpr long double viewport_y_interval = 0.01l;

std::vector<long double> frequencies;
std::vector<long double> y_values;
size_t num_freqs; // frequencies.size()

void generate_frequencies_and_y_values() {
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
	int n = static_cast<int>(lo.size());
	frequencies.clear();
	frequencies.reserve(n * 2 + 1);
	frequencies.insert(frequencies.end(), lo.rbegin(), lo.rend());
	frequencies.push_back(center_frequency);
	frequencies.insert(frequencies.end(), hi.begin(), hi.end());
	y_values.clear();
	for (int i = n; i >= -n; --i)
		y_values.push_back(viewport_y_interval * i);
	num_freqs = frequencies.size();
}

// https://tauday.com/tau-manifesto
constexpr long double tau = std::numbers::pi_v<long double> * 2.0l;

// goertzel algorithm -- see figure 4 here https://asp-eurasipjournals.springeropen.com/articles/10.1186/1687-6180-2012-56
// i also used euler's formula to avoid complex arithmetic -- exp(j x) = cos(x) + j sin(x)
// which ended up giving the algorithm described here https://www.embedded.com/the-goertzel-algorithm/
/* the algorithm i will be using:
 * x = the signal (zero-indexed)
 * N = number of samples in the signal
 * g = 2 cos(tau frequency / sample_rate)
 * s0 = s1 = s2 = 0
 * for i in [0, N-2]
 *   s0 = g * s1 - s2 + x[i]
 *   s2 = s1
 *   s1 = s0
 * s0 = g * s1 - s2 + x[N-1]
 * squared magnitude = s0**2 + s1**2 - s0*s1*g
 * todo how to test this
 */
std::vector<float> goertzel_coeffs;

void generate_goertzel_constants(long double sample_rate) {
	goertzel_coeffs.clear();
	for (long double f : frequencies)
		goertzel_coeffs.emplace_back(2.0l * cosl(tau * f / sample_rate));
}

std::vector<float> mag_sq; // squared magnitudes (the outputs of the goertzel algorithm)
void compute_goertzel() {
	// todo SSE/AVX ? also check if the compiler does it automatically
	for (size_t i = 0; i < num_freqs; ++i) {
		float const &g = goertzel_coeffs[i];
		float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f;
		for (float *p = av::Recorder::recorded_data_begin; p != av::Recorder::recorded_data_end; ++p) {
			s0 = g * s1 - s2 + *p;
			s2 = s1;
			s1 = s0;
		} // it's fine if the signal isn't in order
		mag_sq[i] = s1 * s1 + s2 * s2 - s1 * s2 * g;
	}
}

std::vector<av::Presenter::Vertex::Color> make_rainbow(size_t n) {
	std::vector<av::Presenter::Vertex::Color> rainbow(n);
	for (size_t i = 0; i < n; ++i) {
		float h = i * 6.0f / n;
		switch ((size_t) h) {
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
	return rainbow;
}

constexpr float dampening_factor = 0.95f; // higher values mean the max volume (for normalization) will decrease slower
constexpr unsigned int target_fps = 1000;
//constexpr unsigned int target_nanoseconds_per_frame = 1000000000/target_fps;
constexpr unsigned int target_nanoseconds_per_frame = 0;
static_assert(0.0f < dampening_factor && dampening_factor < 1.0f);

int main() {
	try {
		av::Recorder::init();
		generate_frequencies_and_y_values();
		generate_goertzel_constants(av::Recorder::get_sample_rate());
		mag_sq.resize(num_freqs);

		using Vertex = av::Presenter::Vertex;
		std::vector<Vertex> vertices;
		vertices.reserve(num_freqs);
		for (size_t i = 0; i < num_freqs; ++i) {
			vertices.emplace_back(
				Vertex{
					{(i & 1) * 2.0f - 1.0f, static_cast<float>(y_values[i])},
					{},
				}
			);
		}

		// a lot of this work can probably be put in the shader todo

		std::vector rainbow = make_rainbow(num_freqs * 3);

		timer::start();
		av::Presenter presenter(vertices.size(), true, "av");
		timer::stop("initialization");

		// relevant to normalization https://en.wikipedia.org/wiki/Parseval%27s_theorem
		av::Recorder::start();
		// limit the fps
		std::chrono::steady_clock::time_point frame_start = std::chrono::steady_clock::now();
		float max_mag = 0.0f;
		size_t rainbow_offset = 0;
		while (presenter.is_running()) {
			compute_goertzel();
			max_mag = std::max(max_mag * dampening_factor, *std::max_element(mag_sq.begin(), mag_sq.end()));
			for (size_t i = 0; i < num_freqs; ++i) {
				Vertex::Color const &color = rainbow[(i + rainbow_offset) % rainbow.size()];
				vertices[i].color.r = color.r * mag_sq[i] / max_mag;
				vertices[i].color.g = color.g * mag_sq[i] / max_mag;
				vertices[i].color.b = color.b * mag_sq[i] / max_mag;
			}
			++rainbow_offset;

			presenter.set_vertices(vertices.data());
			presenter.draw_frame();

			timer::fps();
			std::chrono::steady_clock::time_point frame_end;
			while (
				std::chrono::duration_cast<std::chrono::nanoseconds>(
					(frame_end = std::chrono::steady_clock::now()) - frame_start
				).count() < target_nanoseconds_per_frame);
			frame_start = frame_end;
		}

		av::Recorder::stop();
		av::Recorder::uninit(); // todo RAII

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
