# Vulkan audio visualizer

A windowed desktop application which listens to audio data from the system's default microphone. Uses the Goertzel algorithm to compute spectrum intensities. Outputs pretty shapes and colors via Vulkan.

TODO clean up code, document better, check if it builds (if the latest commit doesn't build, one of the older commits should build?), try [FFTW](https://www.fftw.org/).

(system default microphone must be selected before program startup)
