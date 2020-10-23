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
    std::string TDCcalibFile
  );

protected:
  void ParseConfigFile();
  void DistributeEventsSingleStep();
  void BuildEvent(
    EventIII* e, std::map<UInt_t, std::vector<UInt_t> >* m, std::map<UInt_t, UInt_t>* refTimes
  );
  bool loadTDCcalibFile(const char* calibFile);

  // const double kCoarseConstant = 2.6667;
  // const double kCoarseConstant = 2.7027027027;
  const double kCoarseConstant = 2.5;
  // const double kFineConstant = 0.02111484375;
  const double kFineConstant = 0.0201612903;
};

#endif
