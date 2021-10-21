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
	BEAT1_FREQ = 440,
	BEATN_FREQ = 330,
	SONG_POSITION_TO_MIDI_CLOCK_RATIO = 6,
	AUDIO_BLOCK_SIZE = 48,

	BEAT1_WAV_INDEX = 0,
	BEATN_WAV_INDEX = 0,
};

static const int MIDI_CLOCKS_PER_QUARTER_NOTE = 24;
static const int QUARTERS_PER_MEASURE = 4;
DaisyPod hw;
Oscillator osc;
SdmmcHandler sdcard;
WavPlayer sampler;

bool beep = false;
int midiClock = 0;
int continueClock = 0;
bool playing = false;
State state = STOPPED;
int beat = 0;
int continueBeat = 0;
Color beat1Color;
Color beatnColor;

void HandleTimingClock();

void HandleSystemCommon(MidiEvent &m);

void HandleSystemRealTime(MidiEvent &m);

void HandleControlChange(MidiEvent &m);

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
			if (beat == 0) {
				osc.SetFreq(BEAT1_FREQ);
			} else {
				osc.SetFreq(BEATN_FREQ);
			}

			if (midiClock == 0) {
				sampler.Open(beat == 0);
				sampler.Restart();
			} else if (midiClock <= 3) {
				osc.SetAmp(1.0);
				if (beat == 0) {
					hw.led1.SetColor(beat1Color);
				}
				hw.led2.SetColor(beatnColor);
			} else {
				osc.SetAmp(0);
				hw.led1.Set(0, 0, 0);
				hw.led2.Set(0, 0, 0);
			}
			hw.UpdateLeds();
			break;

		}
		case STOPPING:
			continueClock = midiClock;
			continueBeat = beat;
			state = STOPPED;
			beep = false;
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


// Main -- Init, and Midi Handling
int main(void) {
	hw.Init();
	hw.seed.usb_handle.Init(UsbHandle::FS_INTERNAL);
	System::Delay(250);

    SdmmcHandler::Config sd_cfg;
    sd_cfg.Defaults();
    sdcard.Init(sd_cfg);

	osc.Init(hw.AudioSampleRate());
	osc.SetWaveform(Oscillator::WAVE_POLYBLEP_TRI);
	osc.SetAmp(0);
	sampler.Init();
	if (sampler.GetNumberFiles() < 2) {
		hw.led1.SetRed(1.0);
		hw.led2.SetRed(1.0);
		hw.UpdateLeds();
		return -1;
	}

	hw.SetAudioBlockSize(AUDIO_BLOCK_SIZE);
	hw.StartAudio(AudioCallback);
	hw.midi.StartReceive();

	beat1Color.Init(Color::RED);
	beatnColor.Init(Color::GREEN);

	for (;;) {
		sampler.Prepare();
		hw.midi.Listen();
		while (hw.midi.HasEvents()) {
			HandleMidiMessage(hw.midi.PopEvent());
		}
	}
}
