#ifndef Unpacker2_h
#define Unpacker2_h

#include "EventIII.h"
#include <TFile.h>
#include <TH1F.h>
#include <TObject.h>
#include <TObjectTable.h>
#include <TTree.h>
#include <map>
#include <string>

#define REF_CHANNELS_NUMBER 50

class Unpacker2 : public TObject {

public:
  Unpacker2();
  ~Unpacker2() {}
  void Init();
  void UnpackSingleStep(
    std::string inputFile, std::string inputPath, std::string outputPath,
    std::string configFile, int numberOfEvents, int refChannelOffset,
    std::string TOTcalibFile, std::string TDCcalibFile
  );

  TH1F *loadCalibHisto(const char *calibFile);
  bool loadTDCcalibFile(const char *calibFile);


  struct EventHdr {
    UInt_t fullSize;
    UInt_t decoding;
    UInt_t id;
    UInt_t seqNr;
    UInt_t date;
    UInt_t time;
    UInt_t runNr;
    UInt_t pad;
  } hdr;

  struct SubEventHdr {
    UInt_t size;
    UInt_t decoding;
    UInt_t hubAddress;
    UInt_t trgNr;
  } subHdr;

  UInt_t *pHdr;
  UInt_t *subPHdr;

  std::string getHubAddress();
  size_t getHdrSize() const { return sizeof(EventHdr); }
  size_t getSubHdrSize() const { return sizeof(SubEventHdr); }
  UInt_t getFullSize() const { return ((EventHdr *)pHdr)->fullSize; }
  size_t getDataSize();
  size_t getDataLen() const { return ((getFullSize() - getHdrSize()) + 3) / 4; }
  size_t align8(const size_t i) const { return 8 * size_t((i - 1) / 8 + 1); }
  size_t getPaddedSize() { return align8(getDataSize()); }
  size_t getPaddedEventSize() { return align8(getFullSize()); }
  size_t ReverseHex(size_t n);

protected:
  void ParseConfigFile();
  void DistributeEventsSingleStep();

  std::string fInputFile;
  std::string fInputFilePath;
  std::string fOutputFilePath;
  std::string fConfigFile;
  std::string fTOTCalibFile;
  std::string fTDCCalibFile;
  std::map<UInt_t, UInt_t> tdc_offsets;
  int highest_channel_number = -1;
  int fRefChannelOffset;
  int fEventsToAnalyze;
  bool debugMode;

private:
  bool areBytesToBeInverted();
  size_t reverseHex(size_t n);

  const static int kMaxAllowedRepetitions = 1;
  TH1F **TDCcorrections = nullptr;
  TH1F *TOTcalibHist = nullptr;
  bool useTDCcorrection = false;
  long int fileSize;
  bool invertBytes;
  bool fullSetup;
};

#endif
