#ifndef EventIII_h
#define EventIII_h

#include "TDCChannel.h"
#include <TClonesArray.h>
#include <TObject.h>
#include <TTree.h>
#include <fstream>
#include <iostream>

class TDCChannel;

class EventIII : public TNamed
{

private:
  Int_t totalNTDCChannels;

public:
  TClonesArray* TDCChannels;

  EventIII();
  virtual ~EventIII() { Clear(); }

  TDCChannel* AddTDCChannel(int channel);

  Int_t GetTotalNTDCChannels() { return totalNTDCChannels; }

  TClonesArray* GetTDCChannelsArray() { return TDCChannels; }

  void Clear(void);

  ClassDef(EventIII, 1);
};

#endif
