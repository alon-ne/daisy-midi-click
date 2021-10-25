#include "daisy_pod.h"

#include "beat1.h"
#include "beatn.h"
#include "wavstreamer.h"

using namespace daisy;

enum State {
	STOPPED,
	STARTING,
	PLAYING,
	STOPPING
};

enum {
	SONG_POSITION_TO_MIDI_CLOCK_RATIO = 6,
	AUDIO_BLOCK_SIZE = 48,

	MIDI_CLOCKS_PER_QUARTER_NOTE = 24,
	QUARTERS_PER_MEASURE = 4,
	LEDS_OFF_MIDI_CLOCK = MIDI_CLOCKS_PER_QUARTER_NOTE / 4,
};


DaisyPod hw;
WavStreamer beat1Wav;
WavStreamer beatnWav;
WavStreamer *activeBeatWav;

int midiClock = 0;
int continueClock = 0;

State state = STOPPED;
int beat = 0;
int continueBeat = 0;

Color beat1Color;
Color allBeatsColor;

void HandleTimingClock();

void HandleSystemCommon(MidiEvent &m);

void HandleSystemRealTime(MidiEvent &m);

void HandleControlChange(MidiEvent &m);

void playBeat();

void AudioCallback(AudioHandle::InterleavingInputBuffer in,
				   AudioHandle::InterleavingOutputBuffer out,
				   size_t size) {
	for (size_t i = 0; i < size; i += 2) {
		out[i] = out[i + 1] = s162f(activeBeatWav->Stream());
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
				playBeat();
			} else if (midiClock > LEDS_OFF_MIDI_CLOCK) {
				hw.ClearLeds();
			}
			hw.UpdateLeds();
			break;

		}
		case STOPPING:
			continueClock = midiClock;
			continueBeat = beat;
			state = STOPPED;
			hw.ClearLeds();
			hw.UpdateLeds();
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

void playBeat() {
	if (beat == 0) {
		activeBeatWav = &beat1Wav;
		hw.led1.SetColor(beat1Color);
	} else {
		activeBeatWav = &beatnWav;
	}

	activeBeatWav->Restart();
	hw.led2.SetColor(allBeatsColor);
}


void initPod() {
	hw.Init();
	hw.seed.usb_handle.Init(UsbHandle::FS_INTERNAL);

	hw.SetAudioBlockSize(AUDIO_BLOCK_SIZE);
	hw.StartAudio(AudioCallback);
	hw.midi.StartReceive();
}

void initWaves() {
	beat1Wav.Init((int16_t*)beat1_data, sizeof(beat1_data) / sizeof(int16_t));
	beatnWav.Init((int16_t*)beatn_data, sizeof(beatn_data) / sizeof(int16_t));
	activeBeatWav = &beat1Wav;
}

int main(void) {
	initPod();
	initWaves();

	beat1Color.Init(Color::RED);
	allBeatsColor.Init(Color::GREEN);

	for (;;) {
		hw.midi.Listen();
		while (hw.midi.HasEvents()) {
			HandleMidiMessage(hw.midi.PopEvent());
		}
	}
}
