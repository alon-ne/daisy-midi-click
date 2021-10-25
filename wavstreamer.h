//
// Created by Alon Neubach on 25/10/2021.
//

#ifndef MIDICLICK_WAVSTREAMER_H
#define MIDICLICK_WAVSTREAMER_H

#include "daisy_core.h"

class WavStreamer {
public:
	WavStreamer() {}
	~WavStreamer() {}

	void Init(const float *samples, size_t size) {
		samples_ = samples;
		size_ = size;
		offset_ = 0;
		playing_ = false;
	}

	float Stream() {
		if (!playing_) {
			return 0;
		}

		float sample = samples_[offset_++];
		if (offset_ >= size_) {
			offset_ = 0;
			playing_ = false;
		}
		return sample;
	}

	void Stop() { playing_ = false; }

	void Play() {
		offset_ = 0;
		playing_ = true;
	}

	bool IsPlaying() const { return playing_; }

	size_t Size() const { return size_; }

private:
	bool playing_;
	const float *samples_;
	size_t size_;
	size_t offset_;
};


#endif //MIDICLICK_WAVSTREAMER_H
