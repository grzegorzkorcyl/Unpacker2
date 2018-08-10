#include "TDCChannel.h"

using namespace std;

ClassImp(TDCChannel);

TDCChannel::TDCChannel() {
  channel = -1;
}

TDCChannel::~TDCChannel() {}


void TDCChannel::AddLead(double lead) {
  leadTimes.push_back(lead);
}

void TDCChannel::AddTrail(double trail) {
  trailTimes.push_back(trail);
}

void TDCChannel::Clear(Option_t *){

  channel = -1;
  
  leadTimes.clear();
  trailTimes.clear();

}
