#include "ConfigHandler.hh"

#include "TMath.h"
#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"

#include "EventData.hh"

#include "DatabaseConfigurator.hh"
#include "VDatabaseInterface.hh"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <fstream>

#include <string>
#include <map>

double ComputeOccupancy(TH1F noise, TH1F laser);
double ComputeSPEMean(double occupancy, TH1F noise, TH1F laser);

int ProcessLaserRun(std::string filename, VDatabaseInterface* db)
{
    TFile* file = new TFile(filename.c_str(), "UPDATE");

    if(file->IsZombie())
    {
        std::cout << "File not opened." << std::endl;
        return 1;
    }

    TTree* events = (TTree*) file->Get("Events");

    if(!events)
    {
        std::cout << "Events not opened." << std::endl;
        return 1;
    }

    EventData* event = 0;

    events->SetBranchAddress("event", &event);

    unsigned int nevents = events->GetEntries();

    std::cout << "There are " << nevents << " entries in this tree." << std::endl;

    std::map<unsigned int, TH1F> noisemap;
    std::map<unsigned int, TH1F> lasermap;

    for(unsigned int i = 0; i < nevents; i++)
    {
        events->GetEntry(i);

        unsigned int nchannels = event->channels.size();

        for(unsigned int j = 0; j < nchannels; j++)
        {
            ChannelData* channel = &(event->channels.at(j));
            unsigned int channelid = channel->channel_id;
            if(i == 0)
            {
                std::stringstream noiselabel;
                noiselabel << "Noise Ch. " << channelid;

                std::stringstream laserlabel;
                laserlabel << "Laser Ch. " << channelid;

                TH1F noise(noiselabel.str().c_str(), noiselabel.str().c_str(), 10000, -100.0, 900.0);
                TH1F laser(laserlabel.str().c_str(), laserlabel.str().c_str(), 10000, -100.0, 900.0);

                noisemap[channelid] = noise;
                lasermap[channelid] = laser;
            }

            if(channel->regions.size() > 0)
            {
                noisemap[channelid].Fill(-1.0 * channel->regions.at(0).integral);
                lasermap[channelid].Fill(-1.0 * channel->regions.at(1).integral);
            }
       }
    }

    unsigned int runid = event->run_id;

    std::map<unsigned int, TH1F>::iterator it;
    for(it = noisemap.begin(); it != noisemap.end(); it++)
    {
        unsigned int channelid = it->first;
        if(it->second.GetEntries() > 0)
        {
            double occupancy = ComputeOccupancy(it->second, lasermap[it->first]);
            double spemean = ComputeSPEMean(occupancy, it->second, lasermap[it->first]);

            if(occupancy > 0.1 && occupancy < 5 && db)
            {
                std::cout << "Run ID: " << runid << std::endl;
                std::cout << "Channel ID: " << channelid << std::endl;
                std::cout << "Occupancy: " << occupancy << std::endl;
                std::cout << "SPE Mean: " << spemean << std::endl;
                db->StoreChannelinfo(runid, channelid, spemean, occupancy);
                db->StoreChannelinfo(runid, channelid + 1, spemean / 10.0, occupancy);
            }

            it->second.Write("", TObject::kOverwrite);
            lasermap[it->first].Write("", TObject::kOverwrite);
        }
    }

    return 0;
}

double ComputeOccupancy(TH1F noise, TH1F laser)
{
    unsigned int qfracbin = 0;
    double qfrac;
    double totnoiseint = noise.GetEntries();

    do
    {
        ++qfracbin;
        qfrac = noise.Integral(0, qfracbin) / totnoiseint;
    } while(qfrac < 0.5);

    return (-1.0 * TMath::Log(laser.Integral(0, qfracbin) / (qfrac * laser.GetEntries())));
}

double ComputeSPEMean(double occupancy, TH1F noise, TH1F laser)
{
    return ((laser.GetMean() - noise.GetMean()) / occupancy);
}

int main(int argc, char** argv)
{
    bool access_database = false;
    bool reprocess = false;

    DatabaseConfigurator dbconfig;

    ConfigHandler* config = ConfigHandler::GetInstance();

    config->RegisterParameter("reprocess", reprocess, "Enable or disable reprocessing of already processed runs");
    config->RegisterParameter("access_database", access_database, "Enable or disable db access. Must configure to work!");
    config->RegisterParameter("configure_database", dbconfig, "Init and configure a concrete database interface");

    config->SetProgramUsageString("laserrun [options] <run number>");

    if(config->ProcessCommandLine(argc,argv))
    {
        return -1;
    }

    if(argc != 2)
    {
        config->PrintSwitches(true);
    }

    int runid = atoi(argv[1]);
    std::stringstream rawfilename, procfilename;

    rawfilename << "/data/relax/incoming/rawdata/Run" << std::setw(6) << std::setfill('0') << runid;
    procfilename << "/data/relax/prodata/Run" << std::setw(6) << std::setfill('0') << runid << ".root";

    std::ifstream file(procfilename.str().c_str());
    if(!reprocess && file.is_open())
    {
        std::cout << "The processed root file exists and reprocessing is set to false." << std::endl;
        return ProcessLaserRun(procfilename.str(), dbconfig.GetDB());
    }

    std::string command = "./genroot --cfg laserrun_analysis.cfg " + rawfilename.str();

    if(system(command.c_str()))
    {
        std::cerr << "Unable to generate rootfile; aborting." << std::endl;
        return 2;
    }

    return ProcessLaserRun(procfilename.str(), dbconfig.GetDB());
}
