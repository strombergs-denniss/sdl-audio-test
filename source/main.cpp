#include <list>
#include <iostream>
#include <algorithm>
#include <cstdint>
#include <map>
#include <vector>
#include <SDL2/SDL.h>

using namespace std;
#define FTYPE double

namespace synth
{
	//////////////////////////////////////////////////////////////////////////////
	// Utilities

	// Converts frequency (Hz) to angular velocity
	FTYPE w(const FTYPE dHertz)
	{
		return dHertz * 2.0 * M_PI;
	}

	struct instrument_base;

	// A basic note
	struct note
	{
		int id;		// Position in scale
		FTYPE on;	// Time note was activated
		FTYPE off;	// Time note was deactivated
		bool active;
		instrument_base *channel;

		note()
		{
			id = 0;
			on = 0.0;
			off = 0.0;
			active = false;
			channel = nullptr;
		}

		//bool operator==(const note& n1, const note& n2) { return n1.id == n2.id; }
	};

	//////////////////////////////////////////////////////////////////////////////
	// Multi-Function Oscillator
	const int OSC_SINE = 0;
	const int OSC_SQUARE = 1;
	const int OSC_TRIANGLE = 2;
	const int OSC_SAW_ANA = 3;
	const int OSC_SAW_DIG = 4;
	const int OSC_NOISE = 5;

	FTYPE osc(const FTYPE dTime, const FTYPE dHertz, const int nType = OSC_SINE,
		const FTYPE dLFOHertz = 0.0, const FTYPE dLFOAmplitude = 0.0, FTYPE dCustom = 50.0)
	{

		FTYPE dFreq = w(dHertz) * dTime + dLFOAmplitude * dHertz * (sin(w(dLFOHertz) * dTime));

		switch (nType)
		{
		case OSC_SINE: // Sine wave bewteen -1 and +1
			return sin(dFreq);

		case OSC_SQUARE: // Square wave between -1 and +1
			return sin(dFreq) > 0 ? 1.0 : -1.0;

		case OSC_TRIANGLE: // Triangle wave between -1 and +1
			return asin(sin(dFreq)) * (2.0 / M_PI);

		case OSC_SAW_ANA: // Saw wave (analogue / warm / slow)
		{
			FTYPE dOutput = 0.0;
			for (FTYPE n = 1.0; n < dCustom; n++)
				dOutput += (sin(n*dFreq)) / n;
			return dOutput * (2.0 / M_PI);
		}

		case OSC_SAW_DIG:
			return (2.0 / M_PI) * (dHertz * M_PI * fmod(dTime, 1.0 / dHertz) - (M_PI / 2.0));

		case OSC_NOISE:
			return 2.0 * ((FTYPE)rand() / (FTYPE)RAND_MAX) - 1.0;

		default:
			return 0.0;
		}
	}

	//////////////////////////////////////////////////////////////////////////////
	// Scale to Frequency conversion

	const int SCALE_DEFAULT = 0;

	FTYPE scale(const int nNoteID, const int nScaleID = SCALE_DEFAULT)
	{
		switch (nScaleID)
		{
		case SCALE_DEFAULT: default:
			return 8 * pow(1.0594630943592952645618252949463, nNoteID);
		}		
	}


	//////////////////////////////////////////////////////////////////////////////
	// Envelopes

	struct envelope
	{
		virtual FTYPE amplitude(const FTYPE dTime, const FTYPE dTimeOn, const FTYPE dTimeOff) = 0;
	};

	struct envelope_adsr : public envelope
	{
		FTYPE dAttackTime;
		FTYPE dDecayTime;
		FTYPE dSustainAmplitude;
		FTYPE dReleaseTime;
		FTYPE dStartAmplitude;

		envelope_adsr()
		{
			dAttackTime = 0.1;
			dDecayTime = 0.1;
			dSustainAmplitude = 1.0;
			dReleaseTime = 0.2;
			dStartAmplitude = 1.0;
		}

		virtual FTYPE amplitude(const FTYPE dTime, const FTYPE dTimeOn, const FTYPE dTimeOff)
		{
			FTYPE dAmplitude = 0.0;
			FTYPE dReleaseAmplitude = 0.0;

			if (dTimeOn > dTimeOff) // Note is on
			{
				FTYPE dLifeTime = dTime - dTimeOn;

				if (dLifeTime <= dAttackTime)
					dAmplitude = (dLifeTime / dAttackTime) * dStartAmplitude;

				if (dLifeTime > dAttackTime && dLifeTime <= (dAttackTime + dDecayTime))
					dAmplitude = ((dLifeTime - dAttackTime) / dDecayTime) * (dSustainAmplitude - dStartAmplitude) + dStartAmplitude;

				if (dLifeTime > (dAttackTime + dDecayTime))
					dAmplitude = dSustainAmplitude;
			}
			else // Note is off
			{
				FTYPE dLifeTime = dTimeOff - dTimeOn;

				if (dLifeTime <= dAttackTime)
					dReleaseAmplitude = (dLifeTime / dAttackTime) * dStartAmplitude;

				if (dLifeTime > dAttackTime && dLifeTime <= (dAttackTime + dDecayTime))
					dReleaseAmplitude = ((dLifeTime - dAttackTime) / dDecayTime) * (dSustainAmplitude - dStartAmplitude) + dStartAmplitude;

				if (dLifeTime > (dAttackTime + dDecayTime))
					dReleaseAmplitude = dSustainAmplitude;

				dAmplitude = ((dTime - dTimeOff) / dReleaseTime) * (0.0 - dReleaseAmplitude) + dReleaseAmplitude;
			}

			// Amplitude should not be negative
			if (dAmplitude <= 0.01)
				dAmplitude = 0.0;

			return dAmplitude;
		}
	};

	FTYPE env(const FTYPE dTime, envelope &env, const FTYPE dTimeOn, const FTYPE dTimeOff)
	{
		return env.amplitude(dTime, dTimeOn, dTimeOff);
	}


	struct instrument_base
	{
		FTYPE dVolume;
		synth::envelope_adsr env;
		FTYPE fMaxLifeTime;
		wstring name;
		virtual FTYPE sound(const FTYPE dTime, synth::note n, bool &bNoteFinished) = 0;
	};

	struct instrument_bell : public instrument_base
	{
		instrument_bell()
		{
			env.dAttackTime = 0.01;
			env.dDecayTime = 1.0;
			env.dSustainAmplitude = 0.0;
			env.dReleaseTime = 1.0;
			fMaxLifeTime = 3.0;
			dVolume = 1.0;
			name = L"Bell";
		}

		virtual FTYPE sound(const FTYPE dTime, synth::note n, bool &bNoteFinished)
		{
			FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
			if (dAmplitude <= 0.0) bNoteFinished = true;

			FTYPE dSound =
				+ 1.00 * synth::osc(dTime - n.on, synth::scale(n.id + 12), synth::OSC_SINE, 5.0, 0.001)
				+ 0.50 * synth::osc(dTime - n.on, synth::scale(n.id + 24))
				+ 0.25 * synth::osc(dTime - n.on, synth::scale(n.id + 36));

			return dAmplitude * dSound * dVolume;
		}

	};

	struct instrument_bell8 : public instrument_base
	{
		instrument_bell8()
		{
			env.dAttackTime = 0.01;
			env.dDecayTime = 0.5;
			env.dSustainAmplitude = 0.8;
			env.dReleaseTime = 1.0;
			fMaxLifeTime = 3.0;
			dVolume = 1.0;
			name = L"8-Bit Bell";
		}

		virtual FTYPE sound(const FTYPE dTime, synth::note n, bool &bNoteFinished)
		{
			FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
			if (dAmplitude <= 0.0) bNoteFinished = true;

			FTYPE dSound =
				+1.00 * synth::osc(dTime - n.on, synth::scale(n.id), synth::OSC_SQUARE, 5.0, 0.001)
				+ 0.50 * synth::osc(dTime - n.on, synth::scale(n.id + 12))
				+ 0.25 * synth::osc(dTime - n.on, synth::scale(n.id + 24));

			return dAmplitude * dSound * dVolume;
		}

	};

	struct instrument_harmonica : public instrument_base
	{
		instrument_harmonica()
		{
			env.dAttackTime = 0.00;
			env.dDecayTime = 1.0;
			env.dSustainAmplitude = 0.95;
			env.dReleaseTime = 0.5;
			fMaxLifeTime = -1.0;
			name = L"Harmonica";
			dVolume = 0.3;
		}

		virtual FTYPE sound(const FTYPE dTime, synth::note n, bool &bNoteFinished)
		{
			FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
			if (dAmplitude <= 0.0) bNoteFinished = true;

			FTYPE dSound =
				+ 1.0  * synth::osc(n.on - dTime, synth::scale(n.id-12), synth::OSC_SAW_ANA, 5.0, 0.001, 100)
				+ 1.00 * synth::osc(dTime - n.on, synth::scale(n.id), synth::OSC_SQUARE, 5.0, 0.001)
				+ 0.50 * synth::osc(dTime - n.on, synth::scale(n.id + 12), synth::OSC_SQUARE)
				+ 0.05  * synth::osc(dTime - n.on, synth::scale(n.id + 24), synth::OSC_NOISE);

			return dAmplitude * dSound * dVolume;
		}

	};


	struct instrument_drumkick : public instrument_base
	{
		instrument_drumkick()
		{
			env.dAttackTime = 0.01;
			env.dDecayTime = 0.15;
			env.dSustainAmplitude = 0.0;
			env.dReleaseTime = 0.0;
			fMaxLifeTime = 1.5;
			name = L"Drum Kick";
			dVolume = 1.0;
		}

		virtual FTYPE sound(const FTYPE dTime, synth::note n, bool &bNoteFinished)
		{
			FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
			if(fMaxLifeTime > 0.0 && dTime - n.on >= fMaxLifeTime)	bNoteFinished = true;

			FTYPE dSound =
				+ 0.99 * synth::osc(dTime - n.on, synth::scale(n.id - 36), synth::OSC_SINE, 1.0, 1.0)
				+ 0.01 * synth::osc(dTime - n.on, 0, synth::OSC_NOISE);
				
			return dAmplitude * dSound * dVolume;
		}

	};

	struct instrument_drumsnare : public instrument_base
	{
		instrument_drumsnare()
		{
			env.dAttackTime = 0.0;
			env.dDecayTime = 0.2;
			env.dSustainAmplitude = 0.0;
			env.dReleaseTime = 0.0;
			fMaxLifeTime = 1.0;
			name = L"Drum Snare";
			dVolume = 1.0;
		}

		virtual FTYPE sound(const FTYPE dTime, synth::note n, bool &bNoteFinished)
		{
			FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
			if (fMaxLifeTime > 0.0 && dTime - n.on >= fMaxLifeTime)	bNoteFinished = true;

			FTYPE dSound =
				+ 0.5 * synth::osc(dTime - n.on, synth::scale(n.id - 24), synth::OSC_SINE, 0.5, 1.0)
				+ 0.5 * synth::osc(dTime - n.on, 0, synth::OSC_NOISE);

			return dAmplitude * dSound * dVolume;
		}

	};


	struct instrument_drumhihat : public instrument_base
	{
		instrument_drumhihat()
		{
			env.dAttackTime = 0.01;
			env.dDecayTime = 0.05;
			env.dSustainAmplitude = 0.0;
			env.dReleaseTime = 0.0;
			fMaxLifeTime = 1.0;
			name = L"Drum HiHat";
			dVolume = 0.5;
		}

		virtual FTYPE sound(const FTYPE dTime, synth::note n, bool &bNoteFinished)
		{
			FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
			if (fMaxLifeTime > 0.0 && dTime - n.on >= fMaxLifeTime)	bNoteFinished = true;

			FTYPE dSound =
				+ 0.1 * synth::osc(dTime - n.on, synth::scale(n.id -12), synth::OSC_SQUARE, 1.5, 1)
				+ 0.9 * synth::osc(dTime - n.on, 0, synth::OSC_NOISE);

			return dAmplitude * dSound * dVolume;
		}

	};


	struct sequencer
	{
	public:
		struct channel
		{
			instrument_base* instrument;
			wstring sBeat;
		};

	public:
		sequencer(float tempo = 120.0f, int beats = 4, int subbeats = 4)
		{
			nBeats = beats;
			nSubBeats = subbeats;
			fTempo = tempo;
			fBeatTime = (60.0f / fTempo) / (float)nSubBeats;
			nCurrentBeat = 0;
			nTotalBeats = nSubBeats * nBeats;
			fAccumulate = 0;
		}


		int Update(FTYPE fElapsedTime)
		{
			vecNotes.clear();

			fAccumulate += fElapsedTime;
			while(fAccumulate >= fBeatTime)
			{
				fAccumulate -= fBeatTime;
				nCurrentBeat++;

				if (nCurrentBeat >= nTotalBeats)
					nCurrentBeat = 0;

				int c = 0;
				for (auto v : vecChannel)
				{
					if (v.sBeat[nCurrentBeat] == L'X')
					{
						note n;
						n.channel = vecChannel[c].instrument;
						n.active = true;
						n.id = 64;
						vecNotes.push_back(n);
					}
					c++;
				}
			}

			

			return vecNotes.size();
		}

		void AddInstrument(instrument_base *inst)
		{
			channel c;
			c.instrument = inst;
			vecChannel.push_back(c);
		}

		public:
		int nBeats;
		int nSubBeats;
		FTYPE fTempo;
		FTYPE fBeatTime;
		FTYPE fAccumulate;
		int nCurrentBeat;
		int nTotalBeats;

	public:
		vector<channel> vecChannel;
		vector<note> vecNotes;
		

	private:
	
	};
	
}

vector<synth::note> vecNotes;
synth::instrument_bell instBell;
synth::instrument_harmonica instHarm;
synth::instrument_drumkick instKick;
synth::instrument_drumsnare instSnare;
synth::instrument_drumhihat instHiHat;

typedef bool(*lambda)(synth::note const& item);
template<class T>
void safe_remove(T &v, lambda f)
{
	auto n = v.begin();
	while (n != v.end())
		if (!f(*n))
			n = v.erase(n);
		else
			++n;
}

// Function used by olcNoiseMaker to generate sound waves
// Returns amplitude (-1.0 to +1.0) as a function of time
FTYPE MakeNoise(FTYPE dTime)
{	
	FTYPE dMixedOutput = 0.0;

	// Iterate through all active notes, and mix together
	for (auto &n : vecNotes)
	{
		bool bNoteFinished = false;
		FTYPE dSound = 0;

		// Get sample for this note by using the correct instrument and envelope
		if(n.channel != nullptr)
			dSound = n.channel->sound(dTime, n, bNoteFinished);
		
		// Mix into output
		dMixedOutput += dSound;

		if (bNoteFinished) // Flag note to be removed
			n.active = false;
	}
	// Woah! Modern C++ Overload!!! Remove notes which are now inactive
	safe_remove<vector<synth::note>>(vecNotes, [](synth::note const& item) { return item.active; });
	return dMixedOutput * 0.2;
}

struct Data {
    uint64_t sampleCount = 0;
    double time = 0.0;
};

void audioCallback(void* userdata, uint8_t* stream, int length) {
    Data* data = (Data*) userdata;
    uint64_t* sampleCount = &data->sampleCount;
    float* fstream = (float*) stream;

    for (int sid = 0; sid < length / 8; ++sid) {
        double time = (*sampleCount + sid) / 44100.0;
        data->time = time;
        double value = MakeNoise(time);


        fstream[2 * sid + 0] = value;
        fstream[2 * sid + 1] = value;
    }

    *sampleCount += length / 8;
}

int main() {
    Data data;
    bool isActive = true;

    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        std::cout << "SDL error: " << SDL_GetError() << std::endl;

        return -1;
    }

    SDL_Window* window = SDL_CreateWindow("SDL Audio Test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 512, 512, 0);

    SDL_AudioSpec audioSpecDesired, audioSpecObtained;
    SDL_memset(&audioSpecDesired, 0, sizeof(audioSpecDesired));
    audioSpecDesired.freq = 44100;
    audioSpecDesired.format = AUDIO_F32;
    audioSpecDesired.channels = 2;
    audioSpecDesired.samples = 4096;
    audioSpecDesired.callback = audioCallback;
    audioSpecDesired.userdata = (void*) &data;

    SDL_AudioDeviceID audioDeviceId = SDL_OpenAudioDevice(NULL, 0, &audioSpecDesired, &audioSpecObtained, SDL_AUDIO_ALLOW_ANY_CHANGE);
    SDL_PauseAudioDevice(audioDeviceId, 0);

    std::vector<SDL_Scancode> notes = {
        SDL_SCANCODE_Z,
        SDL_SCANCODE_X,
        SDL_SCANCODE_C,
        SDL_SCANCODE_V,
        SDL_SCANCODE_B,
        SDL_SCANCODE_N,
        SDL_SCANCODE_M
    };


    while (isActive) {
        SDL_Event event;
double time = data.time;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                isActive = false;
            }
            
            for (int k = 0; k < notes.size(); ++k) {
                auto noteFound = find_if(vecNotes.begin(), vecNotes.end(), [&k](synth::note const& item) { return item.id == k+64 && item.channel == &instHarm; });
                SDL_Scancode code = notes[k];

                if (event.type == SDL_KEYDOWN && event.key.keysym.scancode == code && event.key.repeat == 0) {
                    if (noteFound == vecNotes.end()) {
                        synth::note n;
                        n.id = k + 64;
                        n.on = time;
                        n.active = true;
                        n.channel = &instHarm;

                        // Add note to vector
                        vecNotes.emplace_back(n);
                    } else {
                        if (noteFound->off > noteFound->on)
                        {
                            // Key has been pressed again during release phase
                            noteFound->on = time;
                            noteFound->active = true;
                        }
                    }
                }

                if (event.type == SDL_KEYUP && event.key.keysym.scancode == code) {
                        if (noteFound->off < noteFound->on)
                            noteFound->off = time;
                }
            }


        }
    }

    SDL_CloseAudioDevice(audioDeviceId);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
