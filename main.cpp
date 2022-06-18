#include "constants.hpp"
#include "Renderer.hpp"
#include "SoundRecorder.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <iostream>
#include <numeric>
#include <ranges>
#include <system_error>
#include <vector>

constexpr long double lo_frequency = 55.0l;
constexpr long double hi_frequency = 4186.009044809578l;
constexpr unsigned int num_goertzel_samples = 480 * 16;
constexpr unsigned int num_history_samples = 480000;
constexpr unsigned int freqs_per_octave = 12 * 2;
constexpr float dampening_factor = 0.95f; // higher values mean the max volume (for normalization) will decrease slower
constexpr unsigned int target_fps = 90;


// perf doesn't work
namespace timer {
	std::chrono::time_point<std::chrono::steady_clock> start_time;

	// start stopwatch
	void start() {
		start_time = std::chrono::steady_clock::now();
	}

	// stop stopwatch and print message
	void stop() {
		auto stop_time = std::chrono::steady_clock::now();
		auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(stop_time - start_time).count();
		std::cout << 1000000000 / nanos << ' ';
	}
}

// todo a lot of the constants are calculated in a bad way and a lot of the code style is questionable
// todo but it's the main file so i don't care as much -- users are invited to rewrite

// todo cache locality
// todo simd

// use long double for all precomputations
// switch to float for the main part

std::vector<long double> generate_frequencies(
	long double lo_frequency,
	long double hi_frequency,
	unsigned int frequencies_per_octave,
	long double round_to_frequency = 440.0l
) {
	if (std::isnan(lo_frequency) || lo_frequency <= 0.0l)
		throw std::invalid_argument("lo_frequency must be positive");
	if (std::isnan(hi_frequency) || hi_frequency <= 0.0l)
		throw std::invalid_argument("hi_frequency must be positive");
	if (std::isnan(round_to_frequency) || round_to_frequency <= 0.0l)
		throw std::invalid_argument("round_to_frequency must be positive");
	if (!frequencies_per_octave)
		throw std::invalid_argument("frequencies_per_octave must be positive");
	if (lo_frequency > hi_frequency)
		throw std::invalid_argument("lo_frequency must be less than or equal to hi_frequency");
	// note_index == 0 corresponds to round_to_frequency
	// note_index == frequencies_per_octave corresponds to round_to_frequency*2.0l
	long long lo_note_index = std::llround(std::log2(lo_frequency / round_to_frequency) * frequencies_per_octave);
	long long hi_note_index = std::llround(std::log2(hi_frequency / round_to_frequency) * frequencies_per_octave);
	std::vector<long double> frequencies;
	frequencies.reserve(hi_note_index - lo_note_index + 1);
	for (long long note_index = lo_note_index; note_index <= hi_note_index; ++note_index)
		frequencies.emplace_back(
			std::exp2(static_cast<long double>(note_index) / frequencies_per_octave) * round_to_frequency);
	return frequencies; // std::ranges?
}

std::vector<float> generate_y_values(unsigned int n) {
	std::vector<float> y_values;
	y_values.reserve(n);
	for (unsigned int i = n; i >= 1; --i)
		y_values.push_back(i * 2.0l / (n + 1) - 1.0l);
	return y_values;
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
std::vector<float> generate_goertzel_constants(std::vector<long double> const &frequencies, long double sample_rate) {
	std::vector<float> goertzel_constants;
	goertzel_constants.reserve(frequencies.size());
	for (long double f : frequencies)
		goertzel_constants.emplace_back(2.0l * cos(f * tau / sample_rate));
	return goertzel_constants;
}

std::vector<float> s0, s1, s2;
std::vector<float> mag; // squared magnitudes (the outputs of the goertzel algorithm)
void compute_goertzel(av::SoundRecorder &rec, std::vector<float> const &goertzel_constants) {
	// todo SSE/AVX ? also check if the compiler does it automatically
/*
	for (size_t i = 0; i < num_freqs; ++i) {
		float const &g = goertzel_constants[i];
		float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f;
		for (float *p = rec.sample_history_begin; p != rec.sample_history_end; ++p) {
			s0 = g * s1 - s2 + *p;
			s2 = s1;
			s1 = s0;
		} // it's fine if the signal isn't in order
		mag[i] = s1 * s1 + s2 * s2 - s1 * s2 * g;
	}
*/
	std::ranges::fill(s1, 0.0f);
	std::ranges::fill(s2, 0.0f);
	float *ptr = rec.sample_history_ptr; // prevent race conditions
	if (ptr - num_goertzel_samples < rec.sample_history_begin) {
		for (float *p = rec.sample_history_end - (ptr - rec.sample_history_begin); p != rec.sample_history_end; ++p) {
			float sample = *p;
			for (size_t i = 0; i < goertzel_constants.size(); ++i)
				s0[i] = goertzel_constants[i] * s1[i] - s2[i] + sample;
			s2 = s1;
			s1 = s0;
			for (size_t i = 0; i < goertzel_constants.size(); ++i)
				mag[i] = s1[i] * s1[i] + s2[i] * s2[i] - s1[i] * s2[i] * goertzel_constants[i];
		}
		for (float *p = rec.sample_history_begin; p != ptr; ++p) {
			float sample = *p;
			for (size_t i = 0; i < goertzel_constants.size(); ++i)
				s0[i] = goertzel_constants[i] * s1[i] - s2[i] + sample;
			s2 = s1;
			s1 = s0;
			for (size_t i = 0; i < goertzel_constants.size(); ++i)
				mag[i] = s1[i] * s1[i] + s2[i] * s2[i] - s1[i] * s2[i] * goertzel_constants[i];
		}
	} else {
		for (float *p = ptr - num_goertzel_samples; p != ptr; ++p) {
			float sample = *p;
			for (size_t i = 0; i < goertzel_constants.size(); ++i)
				s0[i] = goertzel_constants[i] * s1[i] - s2[i] + sample;
			s2 = s1;
			s1 = s0;
			for (size_t i = 0; i < goertzel_constants.size(); ++i)
				mag[i] = s1[i] * s1[i] + s2[i] * s2[i] - s1[i] * s2[i] * goertzel_constants[i];
		}
	}
}

std::vector<av::Vertex::Color> make_rainbow(size_t n) {
	std::vector<av::Vertex::Color> rainbow(n);
	for (size_t i = 0; i < n; ++i) {
		float h = i * 6.0l / n;
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

constexpr unsigned long long target_nanoseconds_per_rainbow_cycle = 8e9;
constexpr unsigned int target_nanoseconds_per_frame = 1000000000 / target_fps;
static_assert(0.0f < dampening_factor && dampening_factor < 1.0f);

int main() {
	try {
		std::cout << "HIIII" << std::endl;
		// todo adjust these
		std::vector<long double> frequencies = generate_frequencies(lo_frequency, hi_frequency, freqs_per_octave);
//		std::ranges::reverse(frequencies);
		size_t num_freqs = frequencies.size();
		std::vector<float> y_values = generate_y_values(num_freqs);
		av::SoundRecorder rec(num_history_samples);
		std::vector<float> goertzel_constants = generate_goertzel_constants(frequencies, rec.get_sample_rate());
		{
			std::cout << "frequencies:";
			for (long double frequency : frequencies)
				std::cout << ' ' << frequency;
			std::cout << std::endl << std::endl;
			std::cout << "y values:";
			for (float y_value : y_values)
				std::cout << ' ' << y_value;
			std::cout << std::endl << std::endl;
			std::cout << "goertzel constants:";
			for (float goertzel_constant : goertzel_constants)
				std::cout << ' ' << goertzel_constant;
			std::cout << std::endl << std::endl;
		}
		s0.resize(num_freqs);
		s1.resize(num_freqs);
		s2.resize(num_freqs);
		mag.resize(num_freqs);

		using Vertex = av::Vertex;

		std::vector<Vertex> vertex_vector;
		std::vector<av::constants::index_t> index_vector;

#define CIRCLE
#ifdef CIRCLE
		std::vector<Vertex::Color> rainbow = make_rainbow(num_freqs);
		vertex_vector.reserve(num_freqs);
		for (size_t i = 1; i <= num_freqs; ++i) {
			long double scalar = std::sqrt(static_cast<long double>(i) / num_freqs);
			vertex_vector.emplace_back(
				Vertex{
					{scalar * std::cos(tau * i / freqs_per_octave), scalar * std::sin(tau * i / freqs_per_octave)},
//					rainbow[i % freqs_per_octave],
//					{1.0f, 1.0f, 1.0f,},
					rainbow[i - 1],
					0.0f,
				}
			);
		}
		for (size_t i = 0; i + freqs_per_octave < num_freqs; ++i) {
			index_vector.emplace_back(i);
			index_vector.emplace_back(i + freqs_per_octave);
		}
#else
		std::vector<Vertex::Color> rainbow = make_rainbow(freqs_per_octave);
		vertex_vector.reserve(num_freqs * 2 + 4);
		vertex_vector.emplace_back(Vertex{{-1.0f, +1.0f},
		                                  {},
		                                  0.0f,});
		vertex_vector.emplace_back(Vertex{{+1.0f, +1.0f},
		                                  {},
		                                  0.0f,});
		for (size_t i = 0; i < num_freqs; ++i) {
			vertex_vector.emplace_back(
				Vertex{
					{-1.0f, y_values[i]},
					rainbow[i % freqs_per_octave],
					0.0f,
				}
			);
			vertex_vector.emplace_back(
				Vertex{
					{+1.0f, y_values[i]},
					rainbow[i % freqs_per_octave],
					0.0f,
				}
			);
		}
		vertex_vector.emplace_back(Vertex{{-1.0f, -1.0f},
		                                  {},
		                                  0.0f,});
		vertex_vector.emplace_back(Vertex{{+1.0f, -1.0f},
		                                  {},
		                                  0.0f,});
		index_vector.resize(vertex_vector.size());
		std::iota(index_vector.begin(), index_vector.end(), 0);
#endif

		for (auto &&v : vertex_vector) {
			std::cout << v.position.x << ',' << v.position.y << '\n';
		}

		// a lot of this work can probably be put in the shader todo

//		timer::start();
		av::Renderer renderer{vertex_vector.size(), index_vector.size()};
		std::span<Vertex> const &vertex_data = renderer.vertex_data;
		std::span<av::constants::index_t> const &index_data = renderer.index_data;
		std::ranges::copy(vertex_vector, vertex_data.begin());
		std::ranges::copy(index_vector, index_data.begin());
//		timer::stop();

		rec.start();
		// relevant to normalization https://en.wikipedia.org/wiki/Parseval%27s_theorem
		// use steady_clock to limit the fps
		std::chrono::steady_clock::time_point frame_start = std::chrono::steady_clock::now();
		std::chrono::steady_clock::time_point rainbow_stage_start = frame_start;
		float max_mag = 0.0f;
//		size_t rainbow_offset = 0; // cycle through the colors
		while (renderer.is_running()) {

			timer::start();

			compute_goertzel(rec, goertzel_constants);
			for (size_t i = 0; i < num_freqs; ++i)
				mag[i] *= frequencies[i];
//				mag[i] = 1.0f;
			max_mag = std::max(max_mag * dampening_factor, *std::max_element(mag.begin(), mag.end()));
			for (size_t i = 0; i < num_freqs; ++i) {
//				Vertex::Color const &color = rainbow[(i + rainbow_offset) % rainbow.size()];
//				Vertex::Color const &color = rainbow[i % rainbow.size()];
				bool yes = (i == 0 && mag[0] > mag[1])
				           || (i == num_freqs - 1 && mag[num_freqs - 1] > mag[num_freqs - 2])
				           || (mag[i] > mag[i - 1] && mag[i] > mag[i + 1]);
				if (yes || true) {
#ifdef CIRCLE
					vertex_data[i].color_multiplier = mag[i] / max_mag;
#else
					vertex_data[i * 2 + 2].color_multiplier = vertex_data[i * 2 + 3].color_multiplier =
						mag[i] / max_mag;
#endif
				} else
#ifdef CIRCLE
					vertex_data[i].color_multiplier = 0.0f;
#else
					vertex_data[i * 2 + 2].color_multiplier = vertex_data[i * 2 + 3].color_multiplier = 0.0f;
#endif
			}

//			renderer.set_vertices(vertex_vector);
			renderer.draw_frame();

			timer::stop();

//			timer::fps();
			std::chrono::steady_clock::time_point frame_end;
			while (
				std::chrono::duration_cast<std::chrono::nanoseconds>(
					(frame_end = std::chrono::steady_clock::now()) - frame_start
				).count() < target_nanoseconds_per_frame);
			frame_start = frame_end;

			if (static_cast<unsigned long long>(std::chrono::duration_cast<std::chrono::nanoseconds>(
				(frame_end = std::chrono::steady_clock::now()) - rainbow_stage_start
			).count()) > target_nanoseconds_per_rainbow_cycle / rainbow.size()) {
				rainbow_stage_start = frame_end;
//				++rainbow_offset;
			}

		}
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
