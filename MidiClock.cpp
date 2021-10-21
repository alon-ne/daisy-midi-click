#include "daisy_pod.h"
#include "daisysp.h"
#include <stdio.h>
#include <string.h>

#include "beat1.h"
#include "beatn.h"


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
    BEATN_FREQ = 330
};

DaisyPod   hw;
Oscillator osc;
WavPlayer    sampler;

bool beep = false;
int clock = 0;
int continueClock = 0;
bool playing = false;
State state = STOPPED;
int beat = 0;
int continueBeat = 0;

void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                   AudioHandle::InterleavingOutputBuffer out,
                   size_t                                size)
{
    for(size_t i = 0; i < size; i += 2)
    {
        out[i] = out[i + 1] = osc.Process();
    }
}

#define DEBUG_LOG(s) hw.seed.usb_handle.TransmitInternal((uint8_t *)s, strlen(s))


void HandleMidiMessage(MidiEvent m)
{
    switch (m.type)
    {
        case SystemRealTime:
        {
            switch (m.srt_type)
            {
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
                    //http://midi.teragonaudio.com/tech/midispec/seq.htm
                    switch (state) 
                    {
                        case STARTING:
                            clock = continueClock;
                            beat = continueBeat;
                            state = PLAYING;

                        case PLAYING:
                            if (beat == 0)
                            {
                                osc.SetFreq(BEAT1_FREQ);
                            }
                            else
                            {
                                osc.SetFreq(BEATN_FREQ);
                            }

                            if (clock <= 3) 
                            {
                                osc.SetAmp(1.0);
                                hw.seed.SetLed(true);
                            } 
                            else
                            {
                                osc.SetAmp(0.0);
                                hw.seed.SetLed(false);
                            }
                            break;
                        
                        case STOPPING:
                            continueClock = clock;
                            continueBeat = beat;
                            state = STOPPED;
                            beep = false;
                            hw.seed.SetLed(false);
                            break;
                       
                        case STOPPED:
                            break;
                    }
                    clock++;
                    if (clock >= 24) {
                        clock = 0;
                        beat = (beat + 1) % 4;
                    }


                    break;
                default:
                    break;
            }
        }
        break;

        case SystemCommon:
        {
            switch (m.sc_type)
            {
                case SongPositionPointer:
                    continueClock = 6 * m.AsSongPositionPointer().position;
                    break;
                
                default:
                    break;
            }
        }
        break;

        case ControlChange:
        {
            ControlChangeEvent p = m.AsControlChange();
            switch(p.control_number)
            {
                case 1:
                    break;
                case 2:
                    break;
                default: 
                    break;
            }
            break;
        }
        default: 
            break;
    }
}


// Main -- Init, and Midi Handling
int main(void)
{
    hw.Init();
    hw.seed.usb_handle.Init(UsbHandle::FS_INTERNAL);
    System::Delay(250);

    osc.Init(hw.AudioSampleRate());
    osc.SetWaveform(Oscillator::WAVE_POLYBLEP_TRI);
    osc.SetAmp(0);

    hw.StartAudio(AudioCallback);
    hw.midi.StartReceive();

    for (;;)
    {
        hw.midi.Listen();
        while (hw.midi.HasEvents())
        {
            HandleMidiMessage(hw.midi.PopEvent());
        }
    }
}
