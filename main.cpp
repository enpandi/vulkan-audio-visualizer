#include "Presenter.h"

#include <iomanip>
#include <iostream>

// idk how to perf on windows
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
	std::vector<uint64_t> frame_counts;

	void frame() {
		if (!frame_count) {
			prev_frame_time = std::chrono::steady_clock::now();
			frame_counts.emplace_back(0);
		}
		++frame_count;
		auto now = std::chrono::steady_clock::now();
		auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now - prev_frame_time).count();
		if (nanos > 1000000000ll) { // one second
			prev_frame_time = now;
			frame_counts.emplace_back(frame_count);
		}
	}

	void dump_fps() {
		for (size_t i = 1; i < frame_counts.size(); ++i)
			std::cout << frame_counts[i] - frame_counts[i - 1] << ' ';
	}
}

int main() {
	try {
		using Vertex = av::Presenter::Vertex;
		std::array vertices = {
			Vertex{{-0.5f, -0.5f},
			       {1.0f,  0.0f, 0.0f}},
			Vertex{{+0.5f, -0.5f},
			       {0.0f,  1.0f, 0.0f}},
			Vertex{{+0.5f, +0.5f},
			       {0.0f,  0.0f, 1.0f}},
			Vertex{{-0.5f, +0.5f},
			       {0.5f,  0.5f, 0.5f}},
		};
		timer::start();
		av::Presenter presenter(vertices.size());
		timer::stop("initialization");

		presenter.set_vertices(vertices.data());
		while (presenter.is_running()) {
			presenter.draw_frame();
			timer::frame();
		}
		timer::dump_fps();

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
