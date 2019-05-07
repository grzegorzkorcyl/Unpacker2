#include "Unpacker2.h"
#include "TimeDateDecoder.h"
#include <TObjString.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

using namespace std;

string UIntToString(UInt_t t)
{
  string s = "0000";
  stringstream sstream;
  sstream << hex << t;

  s = s.replace(4 - sstream.str().length(), sstream.str().length(), sstream.str());

  return s;
}

UInt_t ReverseHex(UInt_t n)
{
  UInt_t a, b, c, d, e;
  a = n & 0x000000ff;
  b = n & 0x0000ff00;
  c = n & 0x00ff0000;
  d = n & 0xff000000;

  a <<= 24;
  b <<= 8;
  c >>= 8;
  d >>= 24;

  e = a | b | c | d;

  return e;
}

Unpacker2::Unpacker2() { Init(); }

void Unpacker2::Init()
{
  debugMode = false;

  invertBytes = false;
  fullSetup = true;

  fileSize = 0;
}

void Unpacker2::UnpackSingleStep(const char* hldFile, const char* configFile, int numberOfEvents, int refChannelOffset, const char* TOTcalibFile,
                                 const char* TDCcalibFile)
{

  eventsToAnalyze = numberOfEvents;

  this->refChannelOffset = refChannelOffset;

  //*** PARSING CONFIG FILE
  ParseConfigFile(string(hldFile), string(configFile));

  TOTcalibHist = loadCalibHisto(TOTcalibFile);

  useTDCcorrection = false;
  if (strlen(TDCcalibFile) != 0)
  {
    useTDCcorrection = loadTDCcalibFile(TDCcalibFile);
  }

  //*** READING BINARY DATA AND DISTRIBUTING IT TO APPROPRIATE UNPACKING MODULES
  DistributeEventsSingleStep(string(hldFile));
}

bool Unpacker2::areBytesToBeInverted(string f)
{
  bool invert = false;
  // open the data file to check the byte ordering
  ifstream* file = new ifstream(f.c_str());

  if (file->is_open())
  {

    // skip the file header
    file->ignore(32);

    // read out the header of the event into hdr structure
    pHdr = (UInt_t*)&hdr;
    file->read((char*)(pHdr), getHdrSize());
    subPHdr = (UInt_t*)&subHdr;
    file->read((char*)(subPHdr), getSubHdrSize());
    if (((SubEventHdr*)subPHdr)->decoding == 16777728)
    {
      invert = true;
    }

    // find the size of the file
    file->seekg(0, ios::end);
    fileSize = file->tellg();
  }
  file->close();
  return invert;
}

void Unpacker2::ParseConfigFile(string f, string s)
{

  invertBytes = areBytesToBeInverted(f);

  // parsing xml config file
  boost::property_tree::ptree tree;

  try
  {
    boost::property_tree::read_xml(s, tree);
  }
  catch (boost::property_tree::xml_parser_error e)
  {
    cerr << "ERROR: Failed to read config file" << endl;
    exit(0);
  }

  // get the config options from the config file
  try
  {
    if (tree.get<string>("READOUT.DEBUG") == "ON")
    {
      debugMode = true;
    }
  }
  catch (exception e)
  {
    cerr << "ERROR: Incorrect config file structure" << endl;
    exit(0);
  }

  if (debugMode == true)
    cerr << "DEBUG mode on" << endl;

  // get the first data source entry in the config file
  boost::property_tree::ptree readoutTree = tree.get_child("READOUT");
  string type;
  string address_s;
  UInt_t address;
  string hubAddress;
  string correctionFile;
  int channels = 0;
  int offset = 0;
  // int resolution = 0;
  // int referenceChannel = 0;
  string measurementType("");
  highest_channel_number = 0;

  // iterate through entries and create appropriate unpackers
  for (const auto& readoutEntry : readoutTree)
  {
    // read out values from xml entry
    if ((readoutEntry.first) == "DATA_SOURCE")
    {
      type = (readoutEntry.second).get<string>("TYPE");
      address_s = (readoutEntry.second).get<string>("TRBNET_ADDRESS");
      hubAddress = (readoutEntry.second).get<string>("HUB_ADDRESS");
      // referenceChannel = (readoutEntry.second).get<int>("REFERENCE_CHANNEL");
      correctionFile = (readoutEntry.second).get<string>("CORRECTION_FILE");

      if (correctionFile.compare("raw") != 0)
      {
        cerr << "WARNING: The TDC correction file path was set in the XML config file of the Unpacker!" << endl;
        cerr << "This file path should be defined in the user parameters JSON file instead." << endl;
        cerr << "The setting from the XML file fill be ignored!" << endl;
      }

      if (type == "TRB3_S")
      {

        // create additional unpackers for internal modules
        boost::property_tree::ptree modulesTree = (readoutEntry.second).get_child("MODULES");
        for (const auto& module : modulesTree)
        {
          type = (module.second).get<string>("TYPE");
          address_s = (module.second).get<string>("TRBNET_ADDRESS");
          address = std::stoul(address_s, 0, 16);
          channels = (module.second).get<int>("NUMBER_OF_CHANNELS");
          offset = (module.second).get<int>("CHANNEL_OFFSET");
          // resolution = (module.second).get<int>("RESOLUTION");
          measurementType = (module.second).get<string>("MEASUREMENT_TYPE");

          tdc_offsets[address] = offset;
          if (offset + channels > highest_channel_number)
          {
            highest_channel_number = offset + channels;
          }
        }
      }
      else
      {
        cerr << "Incorrect configuration in the xml file!" << endl;
        cerr << "The DATA_SOURCE entry is missing!" << endl;
      }
    }
  }
}

void Unpacker2::DistributeEventsSingleStep(string filename)
{
  ifstream* file = new ifstream(filename.c_str());

  if (file->is_open())
  {

    // skip the file header
    file->ignore(32);

    int analyzedEvents = 0;

    EventIII* eventIII = 0;

    // open a new file
    string newFileName = filename + ".root";
    TFile* newFile = new TFile(newFileName.c_str(), "RECREATE");
    TTree* newTree = new TTree("T", "Tree");
    Int_t split = 2;
    Int_t bsize = 64000;

    TH1F* h_rep = new TH1F("h_rep", "data repetition", 1000, -0.5, 999.5);

    newTree->Branch("eventIII", "EventIII", &eventIII, bsize, split);

    eventIII = new EventIII();

    size_t eventSize = 0;
    size_t initialEventSize = 0;

    // iterate through all the events in the file
    while (true)
    {

      if (debugMode == true)
        cerr << "Unpacker2.cc: Position in file at " << file->tellg();

      // read out the header of the event into hdr structure
      pHdr = (UInt_t*)&hdr;
      file->read((char*)(pHdr), getHdrSize());

      eventSize = (size_t)getFullSize();

      if (debugMode == true)
        cerr << " current event size " << eventSize << endl << "Unpacker2.cc: Starting new event analysis, going over subevents" << endl;

      if (eventSize == 32)
        continue;

      initialEventSize = eventSize;

      while (true)
      {
        subPHdr = (UInt_t*)&subHdr;
        file->read((char*)(subPHdr), getSubHdrSize());

        // read out the entire data of the event
        UInt_t* pData = new UInt_t[getDataSize()];
        UInt_t* data = pData;
        file->read((char*)(pData), getDataSize());

        if (debugMode == true)
        {
          cerr << "Unpacker2.cc: Subevent data size: " << getDataSize() << endl;
          cerr << "Unpacker2.cc: Subevent details: " << ((SubEventHdr*)subPHdr)->decoding << " " << ((SubEventHdr*)subPHdr)->hubAddress << " "
               << ((SubEventHdr*)subPHdr)->trgNr << endl;
        }

        UInt_t data_i = 0;
        size_t dataSize = getDataSize();
        UInt_t tdcNumber = 0;
        size_t internalSize = 0;
        size_t is = 0;

        int header;
        int channel;
        int coarse;
        int fine;
        int epoch;
        bool isRising;
        double fullTime;

        int prev_fine = -1;
        int prev_coarse = -1;
        int prev_channel = -1;
        int repetition_counter = 0;

        double refTime;
        bool gotRef;

        bool firstHitOnCh;
        TDCChannel* new_ch;

        int channelOffset;

        size_t initialDataSize = dataSize;

        if ((*data) != 0)
        {

          if (debugMode == true)
          {
            cerr << "Unpacker2.cc: Processing event " << analyzedEvents << " on " << getHubAddress() << endl;
            cerr << "Unpacker_TRB3.cc: Receiving " << dataSize << " words to analyze" << endl;
          }

          while (dataSize > 0)
          {

            if (dataSize > initialDataSize)
            {
              cerr << "ERROR: Incorrect data size encountered, the input file is likely corrupted." << endl;
              cerr << "Stopping the subevent loop." << endl;
              break;
            }

            data_i = ReverseHex((*data));
            tdcNumber = data_i & 0xffff;
            internalSize = data_i >> 16;

            gotRef = false;
            firstHitOnCh = true;

            if (tdc_offsets.count(tdcNumber) > 0)
            {
              channelOffset = tdc_offsets.at(tdcNumber);

              is = internalSize + 1;
              while (is > 0)
              {
                data_i = ReverseHex((*data));

                header = (data_i >> 29);

                switch (header)
                {

                case 3: // epoch ctr
                  epoch = data_i & 0xfffffff;
                  break;

                case 4: // time data
                  if (channel >= 0 && (unsigned(channel) != ((data_i >> 22) & 0x7f)))
                  {
                    firstHitOnCh = true;
                  }

                  channel = (data_i >> 22) & 0x7f;
                  coarse = (data_i & 0x7ff);
                  fine = ((data_i >> 12) & 0x3ff);
                  isRising = ((data_i >> 11) & 0x1);

                  if (channel > 0)
                  { // skip reference channels
                    if (fine == prev_fine && coarse == prev_coarse && channel == prev_channel)
                    {
                      repetition_counter++;
                    }
                    else
                    {
                      if (repetition_counter > 0)
                      {
                        h_rep->Fill(repetition_counter);
                      }
                      if (repetition_counter > kMaxAllowedRepetitions)
                      {
                        // reset the TDC time contents of the TDCChannel object
                        // but keep not the channel numner
                        int tmp_ch = new_ch->GetChannel();
                        new_ch->Clear("");
                        new_ch->SetChannel(tmp_ch);
                        cerr << "WARNING: corrupted data detected on channel " << tmp_ch << ". Skipping event data for this channel." << endl;
                      }
                      repetition_counter = 0;
                    }
                  }

                  prev_channel = channel;
                  prev_fine = fine;
                  prev_coarse = coarse;

                  if (useTDCcorrection == true && TDCcorrections[channel + channelOffset] != nullptr)
                  {
                    fine = TDCcorrections[channel + channelOffset]->GetBinContent(fine + 1);
                  }
                  else
                  {
                    fine = fine * 10;
                  }

                  if (fine != 0x3ff)
                  {
                    fullTime = (double)(((epoch << 11) * 5.0));
                    fullTime += (((coarse * 5000.) - fine) / 1000.);

                    if (channel == 0)
                    {

                      refTime = fullTime;
                      gotRef = true;
                    }
                    else
                    {
                      if (gotRef == true)
                      {

                        if (firstHitOnCh == true)
                        {
                          new_ch = eventIII->AddTDCChannel(channel + channelOffset);
                        }

                        fullTime = fullTime - refTime;

                        if (isRising == false)
                        {
                          fullTime -= TOTcalibHist->GetBinContent(channel + channelOffset + 1);
                          new_ch->AddTrail(fullTime);
                        }
                        else
                        {
                          new_ch->AddLead(fullTime);
                        }

                        firstHitOnCh = false;
                      }
                    }
                  }

                  break;

                default:
                  break;
                }

                data++;
                is--;
              }
            }
            else
            {
              if (debugMode == true)
                cerr << "Unpacker_TRB3.cc: No Unpacker found for module " << tdcNumber << " skipping " << internalSize << " bytes" << endl;

              data += internalSize + 1;
            }

            dataSize -= (internalSize + 1) * 4;
          }
        }
        else if ((*data) == 0)
        {
          cerr << "WARNING: First data word empty, skipping event nr " << analyzedEvents << endl;
        }

        if (debugMode == true)
          cerr << "Unpacker2.cc: Ignoring " << (getPaddedSize() - getDataSize()) << " bytes and reducing eventSize by " << getDataSize();

        delete[] pData;

        // remove the padding bytes
        file->ignore(getPaddedSize() - getDataSize());

        eventSize -= getDataSize();

        if (debugMode == true)
          cerr << " leaving eventSize of " << eventSize << endl;

        if (eventSize > initialEventSize)
        {
          cerr << "ERROR: Incorrect event size, the input file is likely corrupted." << endl;
          cerr << "Stopping event loop after " << analyzedEvents << " events read." << endl;
          break;
        }

        if (eventSize <= 48 && fullSetup == false)
        {
          break;
        }

        eventSize -= getPaddedSize() - getDataSize();

        if ((eventSize <= 64) && fullSetup == true)
        {
          break;
        }

        if ((eventSize <= 176) && fullSetup == true)
        {
          break;
        }

      } // end of major loop

      newTree->Fill();

      if (analyzedEvents % 10000 == 0)
      {
        cerr << analyzedEvents << endl;
      }

      analyzedEvents++;

      eventIII->Clear();

      if (debugMode == true)
      {
        cerr << "Unpacker2.cc: Ignoring padding of the event " << (align8(eventSize) - eventSize) << endl;
        cerr << "Unpacker2.cc: File pointer at " << file->tellg() << " of " << fileSize << " bytes" << endl;
      }

      if (fullSetup == false)
      {
        file->ignore(align8(eventSize) - eventSize);
      }
      // check the end of loop conditions (end of file)
      if ((fileSize - ((int)file->tellg())) < 500)
      {
        break;
      }
      if ((file->eof() == true) || ((int)file->tellg() == fileSize))
      {
        break;
      }
      if (analyzedEvents == eventsToAnalyze)
      {
        break;
      }
    }

    h_rep->Write();

    newFile->Write();

    TObjString timestamp(TimeDateDecoder::formatTimeString(((EventHdr*)pHdr)->date, ((EventHdr*)pHdr)->time).c_str());
    timestamp.Write();

    delete newTree;
  }
  else
  {
    cerr << "ERROR:failed to open data file" << endl;
  }

  file->close();

  //*** END OF READING BINARY DATA
}

TH1F* Unpacker2::loadCalibHisto(const char* calibFile)
{

  TH1F* tmp;
  TH1F* returnHisto;

  // load zero offsets in case no file is specified
  string calibFileName = string(calibFile);
  if (calibFileName.find(".root") == string::npos)
  {
    returnHisto =
        new TH1F("stretcher_offsets", "stretcher_offsets", REF_CHANNELS_NUMBER * refChannelOffset, 0, REF_CHANNELS_NUMBER * refChannelOffset);
    for (int i = 0; i < REF_CHANNELS_NUMBER * refChannelOffset; i++)
    {
      returnHisto->SetBinContent(i + 1, 0);
    }
    cerr << "Zero offsets and calib loaded" << endl;
  }
  // load the stretcher offsets calibration
  else
  {
    TFile* file = new TFile();
    ifstream my_file(calibFileName.c_str());
    file = new TFile(calibFileName.c_str(), "READ");
    TDirectory* dir = gDirectory->GetDirectory("Rint:/");
    tmp = (TH1F*)file->Get("stretcher_offsets");

    returnHisto = (TH1F*)(tmp->Clone("stretcher_offsets"));
    returnHisto->SetDirectory(dir);

    if (debugMode)
    {
      cerr << "Calculated  calib loaded" << endl;
    }

    file->Close();
  }

  return returnHisto;
}

bool Unpacker2::loadTDCcalibFile(const char* calibFile)
{

  TFile* f = new TFile(calibFile, "READ");
  TDirectory* dir = gDirectory->GetDirectory("Rint:/");

  if (f->IsOpen())
  {

    TDCcorrections = new TH1F*[highest_channel_number];
    for (int i = 0; i < highest_channel_number; ++i)
    {

      TH1F* tmp = dynamic_cast<TH1F*>(f->Get(Form("correction%d", i)));

      if (tmp)
      {
        TDCcorrections[i] = dynamic_cast<TH1F*>(tmp->Clone(tmp->GetName()));
        TDCcorrections[i]->SetDirectory(dir);
      }
      else
      {
        TDCcorrections[i] = nullptr;
      }
    }

    if (debugMode)
    {
      cerr << "Loaded TDC nonlinearity corrections." << endl;
    }
  }
  else
  {
    if (debugMode)
    {
      cerr << "The TDC calibration file " << calibFile << " could not be properly opened." << endl;
      cerr << "TDC nonlinearity correction will not be used!" << endl;
    }

    f->Close();
    delete f;

    return false;
  }

  f->Close();
  delete f;

  return true;
}

size_t Unpacker2::getDataSize()
{
  if (invertBytes == false)
  {
    return (size_t)(((SubEventHdr*)subPHdr)->size - 16);
  }
  else
  {
    return (size_t)(ReverseHex(((SubEventHdr*)subPHdr)->size) - 16);
  }

  return -1;
}

std::string Unpacker2::getHubAddress()
{
  string s = "0000";
  stringstream sstream;
  if (invertBytes == false)
  {
    sstream << hex << ((SubEventHdr*)subPHdr)->hubAddress;
  }
  else
  {
    sstream << hex << ReverseHex((UInt_t)((SubEventHdr*)subPHdr)->hubAddress);
  }
  s = s.replace(4 - sstream.str().length(), sstream.str().length(), sstream.str());

  return s;
}

size_t Unpacker2::ReverseHex(size_t n)
{
  size_t a, b, c, d, e;
  a = n & 0x000000ff;
  b = n & 0x0000ff00;
  c = n & 0x00ff0000;
  d = n & 0xff000000;

  a <<= 24;
  b <<= 8;
  c >>= 8;
  d >>= 24;

  e = a | b | c | d;

          return e;
}
