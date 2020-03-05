#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include "Unpacker2D.h"
#include "TDCChannel.h"
#include "EventIII.h"
#include <TCanvas.h>
#include <iostream>
#include <TFile.h>
#include <TTree.h>
#include <string>
#include <vector>
#include <map>

//####################################
// DEFINE THE NUMBER OF ENDPOINTS
//####################################
#define ENDPOINTS 4

using namespace std;

UInt_t ReverseHexDJ(UInt_t n)
{
  UInt_t a, b, c, d, e;
  a = n & 0x000000ff;
  b = n & 0x0000ff00;
  c = n & 0x00ff0000;
  d = n & 0xff000000;

  a <<= 8;
  b >>= 8;
  c <<= 8;
  d >>= 8;

  e = a | b | c | d;

  return e;
}

UInt_t ReverseHexTDC(UInt_t n)
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

void Unpacker2D::BuildEvent(
  EventIII* e, map<UInt_t, vector<UInt_t>>* m, map<UInt_t, double>* refTimes
){
  UInt_t data;
  UInt_t fine;
  UInt_t coarse;
  UInt_t rising;
  double refTime = 0;
  double time = 0;
	double time_t = 0;

  map<UInt_t, vector<UInt_t>>::iterator m_it;
  for (m_it = m->begin(); m_it != m->end(); m_it++) {
    TDCChannel* tc = e->AddTDCChannel(m_it->first);
    for (int i = 0; i < m_it->second.size(); i++) {

      data = m_it->second[i];

      fine = data & 0xff;
      coarse = (data >> 8) & 0xffff;
      rising = (data >> 31);

      if (useTDCcorrection) {
        time = coarse * kCoarseConstant
          + ((TDCcorrections[m_it->first]->GetBinContent(fine + 1)) / 1000.0);
      } else {
        time = coarse * kCoarseConstant + fine * kFineConstant;
      }

      refTime = refTimes->find((int)((m_it->first - 2100) / 105) * 105 + 2100)->second;

      if (rising == 0) {
        if (time - refTime < 0) {
					time_t = time + ((0x10000 * kCoarseConstant) - refTime);
        } else {
          time_t = time - refTime;
        }
        tc->AddLead(time_t);
      } else {
        if (time - refTime < 0) {
          time_t = time + ((0x10000 * kCoarseConstant) - refTime);
        } else {
          time_t = time - refTime;
        }
        tc->AddTrail(time_t);
      }
    }
  }
}

void Unpacker2D::ParseConfigFile()
{
  // parsing xml config file
  boost::property_tree::ptree tree;

  try {
    boost::property_tree::read_xml(fConfigFile, tree);
  } catch (boost::property_tree::xml_parser_error e) {
    cerr << "ERROR: Failed to read config file" << endl;
    exit(0);
  }

  // get the config options from the config file
  try {
    if (tree.get<string>("READOUT.DEBUG") == "ON") {
      debugMode = true;
    }
  } catch (exception e) {
    cerr << "ERROR: Incorrect config file structure" << endl;
    exit(0);
  }

  if (debugMode == true)  cerr << "DEBUG mode on" << endl;

  // get the first data source entry in the config file
  boost::property_tree::ptree readoutTree = tree.get_child("READOUT");
  string type;
  string address_s;
  UInt_t address;
  string hubAddress;
  string correctionFile;
  int channels = 0;
  int offset = 0;
  string measurementType("");
  highest_channel_number = 0;

  // iterate through entries and create appropriate unpackers
  for (const auto& readoutEntry : readoutTree) {
    // read out values from xml entry
    if ((readoutEntry.first) == "DATA_SOURCE") {
      type = (readoutEntry.second).get<string>("TYPE");
      address_s = (readoutEntry.second).get<string>("TRBNET_ADDRESS");
      hubAddress = (readoutEntry.second).get<string>("HUB_ADDRESS");
      correctionFile = (readoutEntry.second).get<string>("CORRECTION_FILE");

      if (correctionFile.compare("raw") != 0) {
        cerr << "WARNING: The TDC correction file path was set in the XML config file of the Unpacker!" << endl;
        cerr << "This file path should be defined in the user parameters JSON file instead." << endl;
        cerr << "The setting from the XML file fill be ignored!" << endl;
      }

      if (type == "TRB3_S" || type == "DJPET_ENDP") {
        // create additional unpackers for internal modules
        boost::property_tree::ptree modulesTree = (readoutEntry.second).get_child("MODULES");
        for (const auto& module : modulesTree) {
          type = (module.second).get<string>("TYPE");
          address_s = (module.second).get<string>("TRBNET_ADDRESS");
          address = std::stoul(address_s, 0, 16);
          channels = (module.second).get<int>("NUMBER_OF_CHANNELS");
          offset = (module.second).get<int>("CHANNEL_OFFSET");
          measurementType = (module.second).get<string>("MEASUREMENT_TYPE");

          tdc_offsets[address] = offset;
          if (offset + channels > highest_channel_number) {
            highest_channel_number = offset + channels;
          }
        }
      } else {
        cerr << "Incorrect configuration in the xml file!" << endl;
        cerr << "The DATA_SOURCE entry is missing!" << endl;
      }
    }
  }
}

void Unpacker2D::UnpackSingleStep(
  string inputFile, string inputPath, string outputPath, string configFile,
  int numberOfEvents, int refChannelOffset, string totCalibFile, std::string tdcCalibFile
) {
  fInputFile = inputFile;
  fInputFilePath = inputPath;
  fOutputFilePath = outputPath;
  fConfigFile = configFile;
  fTOTCalibFile = totCalibFile;
  fTDCCalibFile = tdcCalibFile;
  fEventsToAnalyze = numberOfEvents;
  fRefChannelOffset = refChannelOffset;
  ParseConfigFile();

  if (!fTDCCalibFile.empty()) {
    useTDCcorrection = loadTDCcalibFile(fTDCCalibFile.c_str());
  }

  DistributeEventsSingleStep();
}

void Unpacker2D::DistributeEventsSingleStep()
{

  string fileName = fInputFilePath + fInputFile;
  ifstream* file = new ifstream(fileName.c_str());

  if (file->is_open()) {

    EventIII* eventIII = new EventIII();

    string newFileName = fOutputFilePath + fInputFile + ".root";
    TFile* newFile = new TFile(newFileName.c_str(), "RECREATE");
    TTree* newTree = new TTree("T", "Tree");

    newTree->Branch("eventIII", "EventIII", &eventIII, 64000, 99);

    TH1F* wrongFtabID = new TH1F("wrong_ftab_id", "Wrong FTAB IDs", 70000, 0.5, 70000.05);
    TH1F* h_errors = new TH1F("h_errors", "h_errors", 10, 0, 10);
		TH1F* h_ts_width = new TH1F("h_ts_width", "h_ts_width", 10000, 0, 1000000);
		TH1F* h_ts_diff = new TH1F("h_ts_diff", "h_ts_diff", 100000, -250, 250);
		TH1F* h_coarse = new TH1F("h_coarse", "h_coarse", 65535, 0, 65535);
		TH1F* h_fine = new TH1F("h_fine", "h_fine", 128, 0, 128);

    UInt_t data4;
    Int_t nBytes;
    UInt_t nEvents = 0;
    UInt_t queueSize = 0;
    UInt_t queueDecoding = 0;
    UInt_t subSize = 0;
    UInt_t subDecoding = 0;
    UInt_t ftabId = 0;
    UInt_t ftabSize = 0;
    UInt_t ftabTrgn = 0;
    UInt_t ftabDbg = 0;
    UInt_t dataCtr = 0;
    UInt_t channel = 0;
    UInt_t currentOffset = 0;
    UInt_t ftabWords = 0;
		UInt_t badIdCtr = 0;
    UInt_t prevTrgId = 0;
    bool trgSequence = false;
		bool trgSequenceHold = false;
    int built = 0;
    int skip = 0;
    bool missing_ref = false;
		int refTimesCtr = 0;
		int refTimesCtrPrevious = 0;
		int missingRefCtr = 0;

    map<UInt_t, UInt_t>::iterator offsets_it;
    map<UInt_t, vector<UInt_t>> tdc_channels;
    map<UInt_t, double> refTimes;
    map<UInt_t, double> refTimes_previous;

    // skip the first entry
    file->ignore(32);

    unsigned int packet_buf[1024];
    int packet_buf_ctr = 0;

    while (!file->eof()) {
      nBytes = 0;
      if (nEvents % 10000 == 0) {
        std::cout << std::string(30, '\b');
        std::cout << std::string(30, ' ');
        std::cout << '\r' << "Unpacker2D: " << nEvents << " time slots unpacked " << std::flush;
      }

      eventIII->Clear();

      // queue headers
      // size
      file->read((char*)&data4, 4);
      nBytes += 4;
      queueSize = data4 / 4;

      nEvents++;

      // skip bad entries
      if (queueSize < 0x10) {
        file->ignore(28);
        nBytes += 28;
        if (debugMode) {
          printf("Skipping too small queue\n");
        }
        h_errors->Fill(2);
        newTree->Fill();
        continue;
      }

      // decoding
      file->read((char*)&data4, 4);
      nBytes += 4;
      queueDecoding = data4;

      // skip some headers
      file->ignore(8);
      nBytes += 8;

      // subevent
      // sub size
      file->read((char*)&data4, 4);
      nBytes += 4;
      subSize = data4 & 0xffff;

      // sub decoding
      file->read((char*)&data4, 4);
      nBytes += 4;
      subDecoding = data4;

      // skip some headers
      file->ignore(24);
      nBytes += 24;

      queueSize -= 12;

      refTimesCtrPrevious = refTimesCtr;
			refTimesCtr = 0;
			packet_buf_ctr = 0;
			prevTrgId = ftabTrgn;

      while (!file->eof()) {
        // ftab header
        // ftab size and id
        file->read((char*)&data4, 4);
        nBytes += 4;

        data4 = ReverseHexDJ(data4);
        ftabSize = data4 >> 16;
        ftabId = data4 & 0xffff;

				ftabWords = ftabSize - 2;

        // ftab trigger number and debug
        file->read((char*)&data4, 4);
        nBytes += 4;
        data4 = ReverseHexDJ(data4);
        ftabDbg = data4 >> 16;
        ftabTrgn = data4 & 0xffff;
        queueSize -= 2;

        offsets_it = tdc_offsets.find(ftabId);
        if (offsets_it == tdc_offsets.end()) {
          wrongFtabID->Fill(ftabId);
          h_errors->Fill(3);
          file->ignore((ftabSize - 2) * 4);
          nBytes += (ftabSize - 2) * 4;
          queueSize -= ftabSize - 2;
          badIdCtr++;
          trgSequence = false;
          if (queueSize == 0) {
            break;
          } else if (queueSize == 1) {
            file->ignore(4); nBytes += 4; queueSize -= 1; break;
          }
          continue;
        }

        currentOffset = offsets_it->second;

        if (nEvents > 0) {
          refTimes_previous[currentOffset] = refTimes[currentOffset];
        }

        // ftab data
        while (!file->eof()) {
					file->read((char*)&data4, 4);
					nBytes += 4;
					queueSize--;
					data4 = ReverseHexTDC(data4);

					ftabWords--;

					if (ftabWords == 1) {
						file->read((char*)&data4, 4);
						nBytes += 4;
						queueSize--;
						break;
					}

					if ((data4 >> 24) != 0xfc) {
						channel = (data4 >> 24) & 0x7f;

						h_coarse->Fill((data4 >> 8) & 0xffff);
						h_fine->Fill(data4 & 0xff);

						if (channel == 104) {
							if (useTDCcorrection == true) {
								refTimes[currentOffset] = (((data4 >> 8) & 0xffff) * kCoarseConstant) + ((TDCcorrections[channel
												+ currentOffset]->GetBinContent((data4 & 0xff) + 1)) / 1000.0);
								refTimesCtr++;
							}	else {
								refTimes[currentOffset] = (((data4 >> 8) & 0xffff) * kCoarseConstant) + ((data4 & 0xff) * kFineConstant);
								refTimesCtr++;
							}
						}	else {
							tdc_channels[channel + currentOffset].push_back(data4);
						}
					}
					dataCtr++;
				}

        if (queueSize < 4) {
					if (queueSize == 1)	file->ignore(4);
					if (refTimesCtr == ENDPOINTS) {
						if (nEvents > 0) {
							if (!missing_ref && (ftabTrgn - prevTrgId) == 1
                && trgSequence && !trgSequenceHold
              ) {
								h_ts_diff->Fill((refTimes[2100] - refTimes[2205]) - (refTimes_previous[2100] - refTimes_previous[2205]));
								h_ts_width->Fill(refTimes[2100] - refTimes_previous[2100]);
								map<UInt_t, double>::iterator ref_it = refTimes_previous.begin();
								while(ref_it != refTimes_previous.end()) {
									TDCChannel* tc = eventIII->AddTDCChannel(channel + ref_it->first);
									tc->AddLead(refTimes_previous[ref_it->first]);
									ref_it++;
								}
                built++;
								BuildEvent(eventIII, &tdc_channels, &refTimes_previous);
								h_errors->Fill(0); // event ok
								trgSequence = true;
							}	else {
								skip++;
							}
							missing_ref = false;
						}

						if ((ftabTrgn - prevTrgId) == 1) {
							trgSequence = true;
						}	else if((ftabTrgn - prevTrgId) != 1) {
							trgSequence = false;
						}

						if (refTimesCtrPrevious == ENDPOINTS) {
							trgSequenceHold = false;
						}
            
					}	else {
						skip++;
						h_errors->Fill(1); // missing ref time
						missing_ref = true;
						missingRefCtr++;
						trgSequenceHold = true;
						trgSequence = false;
					}

					tdc_channels.clear();
					break;
				}
      }
			newTree->Fill();
      if (nEvents == fEventsToAnalyze) {
        printf("Max timeslots reached\n");
        break;
      }
    }
    h_fine->Write();
		h_coarse->Write();
		h_errors->Write();
    wrongFtabID->Write();
    newFile->Write();
    delete newTree;
    file->close();
  } else {
    cerr << "ERROR: failed to open data file" << endl;
  }
}