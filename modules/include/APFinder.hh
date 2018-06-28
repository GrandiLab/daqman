/**
Defining the afterpulse finding/evalution module
Author: Mark
Creation: 6/25/2018
**/


#ifndef APFINDER_h
#define APFINDER_h

#include "BaseModule.hh"

/**
Will start with simple amplitude threshold
**/


class APFinder : public BaseModule{
public:

    APFinder();
    ~APFinder();

    int Initialize();
    int Finalize();
    int Process(EventPtr event);

    int EvalWav(ChannelData* chdata) const;

    // I don't think we need currently any info about pulse
    // other than peak location so following function is currently unused.
    // void PulseWindow(ChannelData* chdata, std::vector<int>& start_index, std::vector<int>& end_index);

    static const std::string GetDefaultName() { return "APFinder" ; }

private:

    // Relevant parameters for amplitude threshold

    bool peak_start;
    double amp_threshold;

};

#endif


