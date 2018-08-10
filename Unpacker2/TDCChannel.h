#ifndef TDCChannel_h
#define TDCChannel_h

#include <fstream>
#include <TObject.h>
#include <TClonesArray.h>
#include <iostream>
#include <vector>

class TDCChannel : public TObject {

protected:
  Int_t channel;
  
  std::vector<double> leadTimes;
  std::vector<double> trailTimes;
    
public:

  TDCChannel();
  ~TDCChannel();
  
  void SetChannel(Int_t channel) { this->channel = channel; }
  int GetChannel() { return channel; }
  int GetLeadHitsNum() { return leadTimes.size(); }
  int GetTrailHitsNum() { return trailTimes.size(); }
  
  void AddLead(double lead);
  void AddTrail(double trail);

  double GetLeadTime(unsigned int mult) {
    if( mult >= leadTimes.size() ){
      std::cout<<"asked for lead time out of range."<<std::endl;
      return 0.;
    }
    return leadTimes[mult];
  }

  double GetTrailTime(unsigned int mult) {
    if( mult >= trailTimes.size() ){
      std::cout<<"asked for trail time out of range."<<std::endl;
      return 0.;
    }
    return trailTimes[mult];
  }

  void Clear(Option_t * opt);
  
  ClassDef(TDCChannel,2);
};

#endif
