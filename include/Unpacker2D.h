#ifndef __UNPACKER2D__
#define __UNPACKER2D__

#include <TObject.h>
#include <map>
#include <string>
#include "Unpacker2.h"

class Unpacker2D : public Unpacker2 {
public:
  void UnpackSingleStep(
    std::string inputFile, std::string inputPath, std::string outputPath,
    std::string configFile, int numberOfEvents, int refChannelOffset,
    std::string TOTcalibFile, std::string TDCcalibFile
  );

protected:
  void ParseConfigFile();
  void DistributeEventsSingleStep();
};

#endif
