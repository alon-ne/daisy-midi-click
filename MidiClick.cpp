#include "daisy_pod.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

enum State {
	STOPPED,
	STARTING,
	PLAYING,
	STOPPING
};

enum {
	SONG_POSITION_TO_MIDI_CLOCK_RATIO = 6,
	AUDIO_BLOCK_SIZE = 48,

	BEAT1_WAV_INDEX = 1,
	BEATN_WAV_INDEX = 0,

	MIDI_CLOCKS_PER_QUARTER_NOTE = 24,
	QUARTERS_PER_MEASURE = 4,
	LEDS_OFF_MIDI_CLOCK = MIDI_CLOCKS_PER_QUARTER_NOTE / 4,
};


DaisyPod hw;
SdmmcHandler sdcard;
WavPlayer sampler;

int midiClock = 0;
int continueClock = 0;

State state = STOPPED;
int beat = 0;
int continueBeat = 0;

Color beat1Color;
Color allBeatsColor;
Color offColor;

void HandleTimingClock();

void HandleSystemCommon(MidiEvent &m);

void HandleSystemRealTime(MidiEvent &m);

void HandleControlChange(MidiEvent &m);

void handleBeat();

void AudioCallback(AudioHandle::InterleavingInputBuffer in,
				   AudioHandle::InterleavingOutputBuffer out,
				   size_t size) {
	for (size_t i = 0; i < size; i += 2) {
		out[i] = out[i + 1] = s162f(sampler.Stream());
	}
}

void HandleMidiMessage(MidiEvent m) {
	switch (m.type) {
		case SystemRealTime:
			HandleSystemRealTime(m);
			break;

		case SystemCommon:
			HandleSystemCommon(m);
			break;

		case ControlChange:
			HandleControlChange(m);
			break;

		default:
			break;
	}
}

void HandleSystemRealTime(MidiEvent &m) {
	switch (m.srt_type) {
		case Start:
			continueClock = 0;
			continueBeat = 0;
			state = STARTING;
			break;

		case Continue:
			state = STARTING;
			break;

		case Stop:
			state = STOPPING;
			break;

		case TimingClock:
			HandleTimingClock();
			break;
		default:
			break;
	}
}

void HandleSystemCommon(MidiEvent &m) {
	switch (m.sc_type) {
		case SongPositionPointer:
			continueClock = SONG_POSITION_TO_MIDI_CLOCK_RATIO * m.AsSongPositionPointer().position;
			break;

		default:
			break;
	}
}

void HandleControlChange(MidiEvent &m) {
	ControlChangeEvent cc = m.AsControlChange();
	switch (cc.control_number) {
		case 1:
			break;
		case 2:
			break;
		default:
			break;
	}
}

void HandleTimingClock() {
	//http://midi.teragonaudio.com/tech/midispec/seq.htm
	switch (state) {
		case STARTING:
			midiClock = continueClock;
			beat = continueBeat;
			state = PLAYING;

		case PLAYING: {
			if (midiClock == 0) {
				handleBeat();
			} else if (midiClock > LEDS_OFF_MIDI_CLOCK) {
				hw.led1.SetColor(offColor);
				hw.led2.SetColor(offColor);
			}
			hw.UpdateLeds();
			break;

		}
		case STOPPING:
			continueClock = midiClock;
			continueBeat = beat;
			state = STOPPED;
			hw.seed.SetLed(false);
			break;

		case STOPPED:
			break;
	}

	midiClock++;
	if (midiClock >= MIDI_CLOCKS_PER_QUARTER_NOTE) {
		midiClock = 0;
		beat = (beat + 1) % QUARTERS_PER_MEASURE;
	}
}

void handleBeat() {
	// if (beat == 0) {
	// 	sampler.Open(BEAT1_WAV_INDEX);
	// 	hw.led1.SetColor(beat1Color);
	// } else {
	// 	sampler.Open(BEATN_WAV_INDEX);
	// }
	// sampler.Open(BEATN_WAV_INDEX);

	if (beat == 0) {
		hw.led1.SetColor(beat1Color);
	}

	sampler.Restart();
	hw.led2.SetColor(allBeatsColor);
}


int initSampler() {
	SdmmcHandler::Config sd_cfg;
	sd_cfg.Defaults();
	sdcard.Init(sd_cfg);

	sampler.Init();

	if (sampler.GetNumberFiles() < 2) {
		hw.led1.SetRed(1.0);
		hw.led2.SetRed(1.0);
		hw.UpdateLeds();
		return -1;
	}

	return 0;
}

void initPod() {
	hw.Init();
	hw.seed.usb_handle.Init(UsbHandle::FS_INTERNAL);

	hw.SetAudioBlockSize(AUDIO_BLOCK_SIZE);
	hw.StartAudio(AudioCallback);
	hw.midi.StartReceive();
}

int main(void) {
	initPod();

	int rc = initSampler();
	if (rc != 0) {
		return rc;
	}

	beat1Color.Init(Color::RED);
	allBeatsColor.Init(Color::GREEN);
	offColor.Init(Color::OFF);

	for (;;) {
		sampler.Prepare();
		hw.midi.Listen();
		while (hw.midi.HasEvents()) {
			HandleMidiMessage(hw.midi.PopEvent());
		}
	}
}
