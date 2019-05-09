#ifndef __UNPACKER2D__
#define __UNPACKER2D__

#include <TObject.h>
#include <map>
#include <string>
#include "Unpacker2.h"

class Unpacker2D : public Unpacker2 {
public:
  void UnpackSingleStep(const char *hldFile, const char *configFile,
                        int numberOfEvents, int refChannelOffset,
                        const char *TOTcalibFile, const char *TDCcalibFile);

  void ParseConfigFile(std::string f, std::string s);
  void DistributeEventsSingleStep(std::string file);
};

#endif
