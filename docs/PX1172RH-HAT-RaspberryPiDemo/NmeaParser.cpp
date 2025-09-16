#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "SkyTraqNmeaParser.h"
#include <wiringSerial.h>
//Build
//g++ -o NmeaParser NmeaParser.cpp SkyTraqNmeaParser.cpp -lwiringPi

// The SkyTraqNmeaParser object
SkyTraqNmeaParser parser;
// The SkyTraqNmeaParser result
const GnssData* gdata;
// Notification of SkyTraqNmeaParser
U32 gnssUpdateFlag = 0;

//Callback function for SkyTraqNmeaParser notification
bool GnssUpdated(U32 f, const char* buf, SkyTraqNmeaParser::ParsingType type);
// Display parsing result
bool Display(U32 f, const char* buf, SkyTraqNmeaParser::ParsingType type);
// Show satellites information(from GSV token, and need turn on _SUPPORT_GPS_SATELLITES_, 
// _SUPPORT_GLONASS_SATELLITES_ or _SUPPORT_BEIDOU_SATELLITES_)
void ShowSatellites(const char* gs, const SatelliteInfo* si);

int main()
{
	int fd;
	if((fd = serialOpen("/dev/serial0", 115200)) < 0)
	{
	  fprintf(stderr, "Unable to open serial device: %s\n", strerror(errno));
	  return 1;
	}
	parser.SetNotify(GnssUpdated);

	for(;;)
	{
		
		if(serialDataAvail(fd)) 
		{
			parser.Encode(serialGetchar(fd));
		}
    
	}
  
  return 0;
}
bool GnssUpdated(U32 f, const char* buf, SkyTraqNmeaParser::ParsingType type)
{
  gnssUpdateFlag |= f;
  gdata = parser.GetGnssData();
  Display(f, buf, type);
  //return true to clear the flag in SkyTraqNmeaParseryTraq
  return true;
}

bool Display(U32 f, const char* buf, SkyTraqNmeaParser::ParsingType type)
{

  U32 i = 0;
  const GnssData& gnss = *gdata;
//#if(1)  //Display every second
  char strbuf[96] = {0};
  char sts[32] = {0};
  GnssData::QualityMode m = gnss.GetQualitMode();
  GnssData::NavigationMode n = gnss.GetNavigationMode();

  if((f & SkyTraqNmeaParser::UpdateTime) == 0)
  {
    return true;
  }
  
  if(m == GnssData::QM_Autonomous)
  {
    if(n == GnssData::NM_3DFix)
      strcpy(sts, "Position fix 3D");
    else
      strcpy(sts, "Position fix 2D");
  }
  else if(m == GnssData::QM_Differential)
    strcpy(sts, "DGPS mode");
   else if(m == GnssData::QM_FloatRtk)
    strcpy(sts, "Float RTK");
  else if(m == GnssData::QM_RealTimeKinematic)
    strcpy(sts, "Fix RTK");
  else
    strcpy(sts, "Position fix unavailable!");
    
  printf("%04d/%02d/%02d %02d:%02d:%5.2f(%12.8f,%12.8f,%5.2f) %s\n", 
    gnss.GetYear(),
    gnss.GetMonth(),
    gnss.GetDay(),
    gnss.GetHour(),
    gnss.GetMinute(),
    gnss.GetSecond(),
    gnss.GetLongitude(),
    gnss.GetLatitude(),
    gnss.GetAltitudeInMeter(),
    sts);

  printf("True Heading %5.2f, Heading %5.2f, Pitch %5.2f, Roll %5.2f)\n",
    gnss.GetTrueHeadingInDegree(),
	gnss.GetHeadingInDegree(),
	gnss.GetPitchInDegree(),
	gnss.GetRollInDegree());

  return true;
}
