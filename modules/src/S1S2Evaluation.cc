#include "S1S2Evaluation.hh"
#include "SumChannels.hh"
#include "PulseFinder.hh"
#include "Integrator.hh"
#include "RootWriter.hh"
#include "EventHandler.hh"
#include <string>
#include <stdexcept>

S1S2Evaluation::S1S2Evaluation() :
  BaseModule(GetDefaultName(), "Evaluate S1/S2, fprompt, etc for all channels")
{
  AddDependency<PulseFinder>();
  AddDependency<SumChannels>();
  AddDependency<Integrator>();

}


int S1S2Evaluation::Initialize()
{
  _pulse_finder = EventHandler::GetInstance()->GetModule<PulseFinder>();
  if(!_pulse_finder){
    Message(ERROR)<<"S1S2Evaluator::Initialize(): No PulseFinder module!\n";
    return 1;
  }
  return 0;
}

int S1S2Evaluation::Finalize()
{
  return 0;
}

int S1S2Evaluation::Process(EventPtr evt)
{
  EventDataPtr data = evt->GetEventData();
  //find the region of interest based on the sumchannel pulses
  ChannelData* sumch = data->GetChannelByID(ChannelData::CH_SUM);
  if(!sumch)
    return 0;
  //need at least 1 pulse
  if(sumch->npulses < 1)
    return 0;
  
  //make sure s1/s2 for the pulse is initialized to 0
  data->s1_full = 0;
  data->s2_full = 0;
  data->s1_fixed = 0;
  data->s2_fixed = 0;
  data->max_s1 = 0;
  data->max_s2 = 0;
  data->max_s1_chan = -1;
  data->max_s2_chan = -1;
  data->f90_full = 0;
  data->f90_fixed = 0;
  //double fast = 0;

  //skip if there is no baseline
  if(!sumch->baseline.found_baseline) 
  {
      //needs baseline to do the integral
      data->s1_valid = false;
      data->s1_fixed_valid = false;
      data->s2_valid = false;
      data->s2_fixed_valid = false;
      data->s1s2_valid = false;
      data->s1s2_fixed_valid = false;
      return 0;
  }

  for(size_t pulsenum = 0; pulsenum < sumch->pulses.size(); pulsenum++)
  {
      Pulse pulse = sumch->pulses[pulsenum]; //note: this is a copy!
      
      double* integral = sumch->GetIntegralWaveform();
      int ratio_samps = (int)(0.02*sumch->sample_rate);
      if (pulse.peak_index >= ratio_samps && 
	  pulse.peak_index < sumch->nsamps-ratio_samps)
      {
	  pulse.ratio1 = (integral[pulse.peak_index+ratio_samps] - 
			  integral[pulse.peak_index-ratio_samps]) / 
	      pulse.integral;
	  pulse.ratio2 = (integral[pulse.peak_index-ratio_samps] - 
			  integral[pulse.start_index]) / 
	      pulse.integral;
	  //pulse.ratio3 = - pulse.peak_amplitude / 
	  //(integral[pulse.peak_index] - integral[pulse.peak_index-50]);
      }
      
      //std::cout<<"Pulse: "<<pulsenum<<" "<<pulse.ratio1<<" "<<pulse.ratio2<<std::endl;
      if (pulse.ratio1 > 0.05 && pulse.ratio2 < 0.02)
      {
	  //this is a valid s1 event
	  pulse.is_s1 = true;
	  sumch->pulses[pulsenum].is_s1 = true;

	  //sumch->s1_full = -pulse.integral/sumch->spe_mean;
	  sumch->s1_full = pulse.npe;
	  sumch->s1_fixed = -pulse.fixed_int1/sumch->spe_mean;
	  /*
	  if (data->pulses_aligned == true || data->channels.size() == 1)
	  {
	      data->s1_full += sumch->s1_full;
	      data->s1_fixed += sumch->s1_fixed;
	      if(sumch->s1_full > data->max_s1)
	      {
		  data->max_s1 = sumch->s1_full;
		  data->max_s1_chan = sumch->channel_id;
	      }
	      int s1_90 = sumch->TimeToSample(pulse.start_time+0.09, true);
	      fast += -(sumch->integral[s1_90] - sumch->integral[pulse.start_index]) /
		  sumch->spe_mean;
	  }
	  */
      }
      else
      {
	  //this is s2
	  sumch->s2_full = -pulse.integral/sumch->spe_mean;
	  sumch->s2_fixed = - pulse.fixed_int2/sumch->spe_mean;
	  /*
	  if (data->pulses_aligned == true || data->channels.size() == 1)
	  {
	      data->s2_full += sumch->s2_full;
	      data->s2_fixed += sumch->s2_fixed;
	      if(sumch->s2_full > data->max_s2)
	      {
		  data->max_s2 = sumch->s2_full;
		  data->max_s2_chan = sumch->channel_id;
	      }
	  }
	  */
      }
  }//end loop over pulses
  //evaluate the total f90
  //data->f90_full = fast/data->s1_full;
  //data->f90_fixed = fast/data->s1_fixed;
  return 0;
}
