#include <Rtypes.h>
#include <iomanip>
#include <sstream>

namespace TimeDateDecoder
{
static unsigned int day(UInt_t x) { return (x & 0x00FF); }

static unsigned int sec(UInt_t x) { return (x & 0x00FF); }

static unsigned int month(UInt_t x) { return ((x & 0x0F00) >> 8) + 1; }

static unsigned int min(UInt_t x) { return (x & 0xFF00) >> 8; }

static unsigned int hour(UInt_t x) { return (x & 0x00FF0000) >> 16; }

static unsigned int year(UInt_t x) { return ((x & 0xFFFF0000) >> 16) - 100; }

static std::string formatTimeString(UInt_t date_encoded, UInt_t time_encoded)
{
  std::stringstream ss;
  ss << std::setfill('0') << std::setw(2) << year(date_encoded);
  ss << "-";
  ss << std::setfill('0') << std::setw(2) << month(date_encoded);
  ss << "-";
  ss << std::setfill('0') << std::setw(2) << day(date_encoded);
  ss << " ";
  ss << std::setfill('0') << std::setw(2) << hour(time_encoded);
  ss << ":";
  ss << std::setfill('0') << std::setw(2) << min(time_encoded);
  ss << ":";
  ss << std::setfill('0') << std::setw(2) << sec(time_encoded);
  return ss.str();
}

} // namespace TimeDateDecoder
