/*
OneLoneCoder.com - Simple Audio Noisy Thing
"Allows you to simply listen to that waveform!" - @Javidx9

License
~~~~~~~
Copyright (C) 2018  Javidx9
This program comes with ABSOLUTELY NO WARRANTY.
This is free software, and you are welcome to redistribute it
under certain conditions; See license for details.
Original works located at:
https://www.github.com/onelonecoder
https://www.onelonecoder.com
https://www.youtube.com/javidx9

GNU GPLv3
https://github.com/OneLoneCoder/videos/blob/master/LICENSE

Author
~~~~~~
Twitter: @javidx9
Blog: www.onelonecoder.com
*/



#include <list>
#include <iostream>
#include <algorithm>
#include <cmath>

using namespace std;

#define FTYPE double
#include "olcNoiseMaker.h"


namespace synth
{
	//////////////////////////////////////////////////////////////////////////////
	// Utilities

	// Converts frequency (Hz) to angular velocity
	FTYPE w(const FTYPE dHertz)
	{
		return dHertz * 2.0 * PI;
	}

	// A basic note
	struct note
	{
		int id;		// Position in scale
		FTYPE on;	// Time note was activated
		FTYPE off;	// Time note was deactivated
		bool active;
		int channel;

		note()
		{
			id = 0;
			on = 0.0;
			off = 0.0;
			active = false;
			channel = 0;
		}
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

		FTYPE dFreq = w(dHertz) * dTime + dLFOAmplitude * dHertz * (sin(w(dLFOHertz) * dTime));// osc(dTime, dLFOHertz, OSC_SINE);

		switch (nType)
		{
		case OSC_SINE: // Sine wave bewteen -1 and +1
			return sin(dFreq);

		case OSC_SQUARE: // Square wave between -1 and +1
			return sin(dFreq) > 0 ? 1.0 : -1.0;

		case OSC_TRIANGLE: // Triangle wave between -1 and +1
			return asin(sin(dFreq)) * (2.0 / PI);

		case OSC_SAW_ANA: // Saw wave (analogue / warm / slow)
		{
			FTYPE dOutput = 0.0;
			for (FTYPE n = 1.0; n < dCustom; n++)
				dOutput += (sin(n * dFreq)) / n;
			return dOutput * (2.0 / PI);
		}

		case OSC_SAW_DIG:
			return (2.0 / PI) * (dHertz * PI * fmod(dTime, 1.0 / dHertz) - (PI / 2.0));

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
			return 256 * pow(1.0594630943592952645618252949463, nNoteID);
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

		//void setAttackTime(FTYPE dNewAttackTime)
		//{
		//	dAttackTime = dNewAttackTime;
		//}

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
			if (dAmplitude <= 0.000)
				dAmplitude = 0.0;

			return dAmplitude;
		}
	};

	FTYPE env(const FTYPE dTime, envelope& env, const FTYPE dTimeOn, const FTYPE dTimeOff)
	{
		return env.amplitude(dTime, dTimeOn, dTimeOff);
	}


	struct instrument_base
	{
		FTYPE dHertz;
		int nOffsetZero;
		FTYPE dLFOHertz;
		FTYPE dLFOAmplitude;
		int nType;
		FTYPE dHertz2 = 0.0;
		int nOffsetOne;
		FTYPE dLFOHertz2 = 0.0;
		FTYPE dLFOAmplitude2 = 0.0;
		int nType2 = 0;
		FTYPE dHertz3 = 0.0;
		int nOffsetTwo;
		FTYPE dLFOHertz3 = 0.0;
		FTYPE dLFOAmplitude3 = 0.0;
		int nType3 = 0;


		FTYPE dVolume;
		synth::envelope_adsr env;
		virtual FTYPE sound(const FTYPE dTime, synth::note n, bool& bNoteFinished) = 0;
		std::vector<std::string> graph;

		void setLFOHertz(FTYPE dNewLFOHertz)
		{
			dLFOHertz = dNewLFOHertz;
			build_graph();
		}

		void setLFOAmplitude(FTYPE dNewdLFOAmplitude)
		{
			dLFOAmplitude = dNewdLFOAmplitude;
			build_graph();
		}

		void setOffsetZero(int nNewOffsetZero)
		{
			nOffsetZero = nNewOffsetZero;
			dHertz = synth::scale(nOffsetZero);
			build_graph();
		}

		void setOffsetOne(int nNewOffsetOne)
		{
			nOffsetOne = nNewOffsetOne;
			dHertz2 = synth::scale(nOffsetOne);
			build_graph();
		}

		void build_graph()
		{
			int size = 100, height = 30;

			// Start with an empty chart (lots of spaces and a line in the middle)
			std::vector<std::string> chart(height, std::string(size, ' '));
			chart[height / 2] = std::string(size, '-');

			// Then just put x-es where the function should be plotted
			for (int i = 0; i < size; ++i) {

				int adj = 500;
				int multiplier = 5;
				int offset = 10;

				double eq = multiplier *
					1.00 * get_eq(dHertz, dLFOHertz, dLFOAmplitude, nType, adj, i) +
					0.50 * get_eq(dHertz2, dLFOHertz2, dLFOAmplitude2, nType2, adj, i) +
					0.25 * get_eq(dHertz3, dLFOHertz3, dLFOAmplitude3, nType3, adj, i)
				+ offset;

				chart[static_cast<int>(std::round(eq))][i] = 'x';
			}

			graph = chart;
		}

		double get_eq(FTYPE dHertz, FTYPE dLFOHertz, FTYPE dLFOAmplitude, int nType, int adj, int i, FTYPE dCustom = 50.0)
		{
			switch (nType)
			{
			case OSC_SINE: // Sine wave bewteen -1 and +1
				return sin((dHertz * 2.0 * PI * i + dLFOAmplitude * dHertz * sin(dLFOHertz * 2.0 * PI * i)) / adj);

			case OSC_SQUARE: // Square wave between -1 and +1
				return sin((dHertz * 2.0 * PI * i + dLFOAmplitude * dHertz * sin(dLFOHertz * 2.0 * PI * i)) / adj) > 0 ? 1.0 : -1.0;

			case OSC_TRIANGLE: // Triangle wave between -1 and +1
				return asin(sin((dHertz * 2.0 * PI * i + dLFOAmplitude * dHertz * sin(dLFOHertz * 2.0 * PI * i)) / adj)) * (2.0 / PI);

			case OSC_SAW_ANA: // Saw wave (analogue / warm / slow)
			{
				FTYPE dOutput = 0.0;
				for (FTYPE n = 1.0; n < dCustom; n++)
					dOutput += (sin(n * (dHertz * 2.0 * PI * i + dLFOAmplitude * dHertz * sin(dLFOHertz * 2.0 * PI * i)) / adj)) / n;
				return dOutput * (2.0 / PI);
			}

			case OSC_SAW_DIG:
				return (2.0 / PI) * (dHertz * PI * fmod(i, 1.0 / dHertz) - (PI / 2.0));

			case OSC_NOISE:
				return 2.0 * ((FTYPE)rand() / (FTYPE)RAND_MAX) - 1.0;

			default:
				return 0.0;
			}
		}
	};

	struct instrument_bell : public instrument_base
	{
		instrument_bell()
		{
			env.dAttackTime = 0.01;
			env.dDecayTime = 1.0;
			env.dSustainAmplitude = 0.0;
			env.dReleaseTime = 1.0;

			nOffsetZero = 12;
			dHertz = synth::scale(nOffsetZero);
			dLFOHertz = 5;
			dLFOAmplitude = 0.001;
			nType = OSC_SINE;
			nOffsetOne = 24;
			dHertz2 = synth::scale(nOffsetOne);
			nType2 = OSC_SINE;
			nOffsetTwo = 36;
			dHertz3 = synth::scale(nOffsetTwo);

			dVolume = 1.0;

			build_graph();
			//build_graph(synth::scale(12), 5, 0.001, OSC_SINE);
		}

		virtual FTYPE sound(const FTYPE dTime, synth::note n, bool& bNoteFinished)
		{
			FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
			if (dAmplitude <= 0.0) bNoteFinished = true;

			FTYPE dSound =
				+ 1.00 * synth::osc(n.on - dTime, synth::scale(n.id + nOffsetZero), nType, dLFOHertz, dLFOAmplitude)
				+ 0.50 * synth::osc(n.on - dTime, synth::scale(n.id + nOffsetOne))
				+ 0.25 * synth::osc(n.on - dTime, synth::scale(n.id + nOffsetTwo));
			
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

			dVolume = 1.0;
		}

		virtual FTYPE sound(const FTYPE dTime, synth::note n, bool& bNoteFinished)
		{
			FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
			if (dAmplitude <= 0.0) bNoteFinished = true;

			FTYPE dSound =
				+ 1.00 * synth::osc(n.on - dTime, synth::scale(n.id), synth::OSC_SQUARE, 5.0, 0.001)
				+ 0.50 * synth::osc(n.on - dTime, synth::scale(n.id + 12))
				+ 0.25 * synth::osc(n.on - dTime, synth::scale(n.id + 24));

			return dAmplitude * dSound * dVolume;
		}

	};

	struct instrument_harmonica : public instrument_base
	{
		instrument_harmonica()
		{
			env.dAttackTime = 0.05;
			env.dDecayTime = 1.0;
			env.dSustainAmplitude = 0.95;
			env.dReleaseTime = 0.1;

			dVolume = 1.0;
			nOffsetZero = 0;
			dHertz = synth::scale(nOffsetZero);
			dLFOHertz = 5;
			dLFOAmplitude = 0.001;
			nType = OSC_SQUARE;
			nOffsetOne = 12;
			dHertz2 = synth::scale(nOffsetOne);
			nType2 = OSC_SQUARE;
			nOffsetTwo = 24;
			dHertz3 = synth::scale(nOffsetTwo);

			build_graph();
			//build_graph(synth::scale(12), 5, 0.001, 1);
		}

		virtual FTYPE sound(const FTYPE dTime, synth::note n, bool& bNoteFinished)
		{
			FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
			if (dAmplitude <= 0.0) bNoteFinished = true;

			FTYPE dSound =
				//+ 1.0  * synth::osc(n.on - dTime, synth::scale(n.id-12), synth::OSC_SAW_ANA, 5.0, 0.001, 100)
				+ 1.00 * synth::osc(n.on - dTime, synth::scale(n.id + nOffsetZero), synth::OSC_SQUARE, 5.0, 0.001)
				+ 0.50 * synth::osc(n.on - dTime, synth::scale(n.id + nOffsetOne), synth::OSC_SQUARE)
				+ 0.05 * synth::osc(n.on - dTime, synth::scale(n.id + nOffsetTwo), synth::OSC_NOISE);

			return dAmplitude * dSound * dVolume;
		}

	};

}

vector<synth::note> vecNotes;
mutex muxNotes;
synth::instrument_bell instBell;
synth::instrument_harmonica instHarm;



typedef bool(*lambda)(synth::note const& item);
template<class T>
void safe_remove(T& v, lambda f)
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
FTYPE MakeNoise(int nChannel, FTYPE dTime)
{
	unique_lock<mutex> lm(muxNotes);
	FTYPE dMixedOutput = 0.0;

	for (auto& n : vecNotes)
	{
		bool bNoteFinished = false;
		FTYPE dSound = 0;
		if (n.channel == 2)
			dSound = instBell.sound(dTime, n, bNoteFinished);
		if (n.channel == 1)
			dSound = instHarm.sound(dTime, n, bNoteFinished) * 0.5;
		dMixedOutput += dSound;

		if (bNoteFinished && n.off > n.on)
			n.active = false;
	}

	// Woah! Modern C++ Overload!!!
	safe_remove<vector<synth::note>>(vecNotes, [](synth::note const& item) { return item.active; });

	return dMixedOutput * 0.2;
}

void graph(int iChannel)
{
	// **********
	// Graph code
	// **********

	std::vector<std::string> chart;

	if (iChannel == 1) {
		chart = instHarm.graph;
	}
	else {
		chart = instBell.graph;
	}

	// and print the whole shebang
	for (auto&& s : chart) {
		std::cout << s << '\n';
	}
}

int main()
{
	// Get all sound hardware
	vector<wstring> devices = olcNoiseMaker<short>::Enumerate();

	// Display findings
	for (auto d : devices) wcout << "Found Output Device: " << d << endl;
	wcout << "Using Device: " << devices[0] << endl;

	// Create sound machine!!
	olcNoiseMaker<short> sound(devices[0], 44100, 1, 8, 512);

	// Link noise function with sound machine
	sound.SetUserFunction(MakeNoise);

	char keyboard[129];
	memset(keyboard, ' ', 127);
	keyboard[128] = '\0';

	auto clock_old_time = chrono::high_resolution_clock::now();
	auto clock_real_time = chrono::high_resolution_clock::now();
	double dElapsedTime = 0.0;

	// Change this to change instrument
	// 1 = harmonica (square)
	// 2 = bell (sine)
	int iChannel = 1;

	graph(iChannel);

	while (1)
	{
		for (int k = 0; k < 16; k++)
		{
			short nKeyState = GetAsyncKeyState((unsigned char)("ZSXCFVGBNJMK\xbcL\xbe\xbf"[k]));

			double dTimeNow = sound.GetTime();

			// Check if note already exists in currently playing notes
			muxNotes.lock();
			auto noteFound = find_if(vecNotes.begin(), vecNotes.end(), [&k](synth::note const& item) { return item.id == k; });
			if (noteFound == vecNotes.end())
			{
				// Note not found in vector

				if (nKeyState & 0x8000)
				{
					// Key has been pressed so create a new note
					synth::note n;
					n.id = k;
					n.on = dTimeNow;
					n.channel = iChannel;
					n.active = true;

					// Add note to vector
					vecNotes.emplace_back(n);

					//instHarm.setOffsetZero(12);
					//graph(iChannel);
				}
				else
				{
					// Note not in vector, but key has been released...
					// ...nothing to do
				}
			}
			else
			{
				// Note exists in vector
				if (nKeyState & 0x8000)
				{
					// Key is still held, so do nothing
					if (noteFound->off > noteFound->on)
					{
						// Key has been pressed again during release phase
						noteFound->on = dTimeNow;
						noteFound->active = true;
					}
				}
				else
				{
					// Key has been released, so switch off
					if (noteFound->off < noteFound->on)
					{
						noteFound->off = dTimeNow;
					}
				}
			}
			muxNotes.unlock();
		}
		wcout << "\rNotes: " << vecNotes.size() << "    ";

		//this_thread::sleep_for(5ms);
	}

	return 0;
}