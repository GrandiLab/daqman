
#include "APFinder.hh"
#include "ConvertData.hh"
#include "BaselineFinder.hh"
#include "Integrator.hh"
#include "RootWriter.hh"
#include "PulseFinder.hh"

APFinder::APFinder() :

    BaseModule(GetDefaultName(), "Search for afterpulses in waveforms")

{

    AddDependency<ConvertData>();
    AddDependency<BaselineFinder>();

    RegisterParameter("peak_start", peak_start = false, "Index at which the waveform exceeds the threshold value");
    RegisterParameter("amp_threshold", amp_threshold = -10);

}

APFinder::~APFinder()
{
}

int APFinder::Initialize()
{
    return 0;
}

int APFinder::Finalize()
{
    return 0;
}

int APFinder::Process(EventPtr evt)
{

    EventDataPtr event = evt->GetEventData();

    for (size_t ch = 0 ; ch < event->channels.size() ; ch++)
    {

        ChannelData* chdata = &(event->channels[ch]);

        if(_skip_channels.find(chdata->channel_id) != _skip_channels.end())
            continue;

        if(! chdata->baseline.found_baseline)
            continue;

        EvalWav(chdata);

    }
    return 0;
}

int APFinder::EvalWav(ChannelData* chdata) const
{

    double* wav = chdata->GetBaselineSubtractedWaveform();
    double* integral = chdata->GetIntegralWaveform();

    int n = chdata->nsamps;

    // Define the second derivative of the integral (will look for where it becomes positive)
    std::vector<double> curve(n);
    curve[0] = integral[1];
    for (int i = 1; i < n-1; i++) {
        curve[i] = integral[i-1] + integral[i+1] - 2*integral[i];
    }

    int count = 0;
    bool pstart = false;
    int pstart_ind = 0;
    int pend_ind = 0;

    for (int i = 0; i < n-1; i++)
    {

        if (wav[i] < amp_threshold) {
            pstart = true;
        }

        if (wav[i] < amp_threshold && wav[i] < wav[i-1] && wav[i] < wav[i+1]) {

            Pulse pulse;

            pulse.found_start = true;
            pulse.start_index = i - 3;
            pulse.start_time = chdata->SampleToTime(pulse.start_index);
            pulse.found_end = true;
            pulse.end_index = i + 3;
            pulse.end_time = chdata->SampleToTime(pulse.end_index);
            pulse.found_peak = true;

            pulse.peak_index = i;
            pulse.peak_time = chdata->SampleToTime(pulse.peak_index);

            pulse.peak_amplitude =  -wav[i];

            pulse.start_clean = true;
            pulse.end_clean = true;
            pulse.is_clean = true;

            chdata->npulses++;

            chdata->pulses.push_back(pulse);


        }

    }

    return 0;
}
