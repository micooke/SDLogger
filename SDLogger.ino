#include <Arduino.h>

//*************************************
// WIRING UP / PIN ALLOCATION
//*************************************
// SD uses the following pin mapping
// [CS,MOSI,MISO,CLK] = [D10,D11,D12,D13]
//
// DS18B20 is attached to D9 (a 4.7K resistor is necessary)
//

//*************************************
// SETUP
//*************************************
#ifndef _DEBUG
#define _DEBUG 1 // Change to _DEBUG 0 when you dont want Serial output messages
#endif

#define UPDATE_INTERVAL_ms 1000 // log data every 1000ms / 1sec

#define NO_SD_CARD // Uncomment to test without the SD card attached
#define SD_SAFE_WRITE

// Macros to easily include/exclude debug code
#if (_DEBUG == 0)
#define debugPrint(...)
#define debugPrintln(...)
#else
#define debugPrint(...) Serial.print(__VA_ARGS__)
#define debugPrintln(...) Serial.println(__VA_ARGS__)
#endif

// log timing
uint32_t tLast = 0;
uint32_t tFirst = 0;

//*************************************
// SD CARD
//*************************************
// SD uses the following pin mapping
//[CS,MOSI,MISO,CLK] = [D10,D11,D12,D13]
#include <SPI.h>
#include <SD.h>

File dataFile;
uint16_t num_files = 0;
char LOG_FILENAME[13] = {"LOG_0000.TXT"};
bool SD_INITIALISED = false;

// SD function declarations
void logFilename(const uint16_t &_num, char (&_filename)[13]); // Function declaration
uint16_t get_log_count(File dir);
void logTempSensor(float &temp_degC);

//*************************************
// TEMP SENSOR
//*************************************
#include <OneWire.h> // https://github.com/PaulStoffregen/OneWire.git
OneWire  ds(9);  // on pin 9 (a 4.7K resistor is necessary)
uint8_t present = 0;
uint8_t type_s;
uint8_t data[12];
uint8_t addr[8];

// Temp sensor function declarations
bool setupTempSensor();
float readTempSensor();

//*************************************
// SETUP
//*************************************
void setup()
{
  // SD *******************************
  #ifndef NO_SD_CARD
  	// Initialise the SD card
  	if (SD.begin(SS)) // SS = D10 (arduino built-in), chip select pin
  	{
  		SD_INITIALISED = true;
  	}
  	dataFile = SD.open("/");
  	num_files = get_log_count(dataFile);
  	dataFile.close();
  #else
    num_files = 0;
  #endif

  // Pre-generate an auto-incremented log filename
	logFilename(++num_files, LOG_FILENAME);

  // Open up the new log file for writing
  #ifndef NO_SD_CARD
  dataFile = SD.open(LOG_FILENAME, FILE_WRITE);
  dataFile.print(",");
  #if SD_SAFE_WRITE
    dataFile.close();
  #endif
  #endif

  // DEBUG ****************************
	// Start the Serial port (for debug)
  #if (_DEBUG > 0)
	Serial.begin(9600);
  debugPrint("Log file : ");
  debugPrintln(LOG_FILENAME);
  #endif
 
  // TEMP SENSOR **********************
  // setup the temperature sensor
  setupTempSensor();

  // SD *******************************
  // log first temp sensor read here (setup first log - tFirst)
  tFirst = millis();
  float temp_degC = readTempSensor();
  logTempSensor(temp_degC);
  tLast = tFirst;
}

//*************************************
// LOOP
//*************************************
void loop()
{
	if (millis() - tLast > UPDATE_INTERVAL_ms)
  {
    tLast = millis();
    float temp_degC = readTempSensor();
    logTempSensor(temp_degC);
  }
}

//*************************************
// SD FUNCTIONS
//*************************************
void logFilename(const uint16_t &_num, char (&_filename)[13])
{
	uint16_t num_ = _num;
	const uint8_t th = num_ / 1000; num_ -= th*1000;
	const uint8_t hu = num_ / 100;  num_ -= hu*100;
	const uint8_t te = num_ / 10;  num_ -= te*10;

	_filename[0] = 'L';
	_filename[1] = 'O';
	_filename[2] = 'G';
	_filename[3] = '_';
	_filename[4] = '0'+th;
	_filename[5] = '0'+hu;
	_filename[6] = '0'+te;
	_filename[7] = '0'+num_;
	_filename[8] = '.';
	_filename[9] = 'T';
	_filename[10] = 'X';
	_filename[11] = 'T';
	_filename[12] = '\0';
}

uint16_t get_log_count(File dir)
{
	uint16_t count = 0, t_count;

	debugPrintln("+--------------");
	debugPrintln("| SD files");
	debugPrintln("+--------------");

	while(true)
	{
		File entry =  dir.openNextFile();
		if (! entry)
		{
			// no more files
			debugPrintln("+--------------");
			debugPrint("| ");
			debugPrint(count); debugPrintln(" log files");
			debugPrintln("+--------------");
      
			return count;
		}
		
		// Check if its a log file
		char * filename = entry.name();
		
		#if (_DEBUG == 1)
		debugPrint("| ");
		#endif
		if (strncmp(filename, "LOG_", 4) == 0)
		{
			t_count = filename[4] - '0'; // thousands
			t_count =10*t_count + (filename[5] - '0'); // hundreds
			t_count =10*t_count + (filename[6] - '0'); // tens
			t_count =10*t_count + (filename[7] - '0'); // units
			
			count = max(count, t_count);
			debugPrint("* ");
		}
		else
		{
			debugPrint("  ");
		}
		debugPrintln(entry.name());

		entry.close();
	}
}

void logTempSensor(float &temp_degC)
{
  #ifndef NO_SD_CARD
  #if SD_SAFE_WRITE
  dataFile = SD.open(LOG_FILENAME, O_APPEND); // if O_APPEND isnt found change it for 0x04 (defined line 65 of SdFat.h)
  #endif
  dataFile.print(millis() - tFirst);
  dataFile.print(",");
  dataFile.println(temp_degC);
  #if SD_SAFE_WRITE
  dataFile.close();
  #endif
  #endif

  // Debug output
  debugPrint(millis() - tFirst);
  debugPrint(",");
  debugPrintln(temp_degC);
}

//*************************************
// TEMP SENSOR FUNCTIONS
//*************************************
bool setupTempSensor()
{
  present = 0;
  
  // search for the temp sensor address
  if ( !ds.search(addr))
  {
    debugPrintln("No more addresses.");
    debugPrintln();
    ds.reset_search();
    delay(250);
    return false;
  }

  // print out the temp sensor address
  #if (_DEBUG > 0)
  debugPrint("ROM =");
  for(uint8_t i = 0; i < 8; ++i)
  {
    Serial.write(' ');
    debugPrint(addr[i], HEX);
  }
  #endif

  // CRC check on the address
  if (OneWire::crc8(addr, 7) != addr[7])
  {
      debugPrintln("CRC is not valid!");
      return false;
  }
  debugPrintln();
 
  // Identify the temp sensor Chip
  // The first ROM uint8_t indicates which chip
  switch (addr[0]) {
    case 0x10:
      debugPrintln("  Chip = DS18S20");  // or old DS1820
      type_s = 1;
      break;
    case 0x28:
      debugPrintln("  Chip = DS18B20");
      type_s = 0;
      break;
    case 0x22:
      debugPrintln("  Chip = DS1822");
      type_s = 0;
      break;
    default:
      debugPrintln("Device is not a DS18x20 family device.");
      return false;
  }

  // start conversion, with parasite power on at the end
  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);
  
  delay(1000); // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.

  return true;
}

float readTempSensor()
{
  /*
  // NOTE : May need to do this each read? ask @mr-sneezy
  // start conversion, with parasite power on at the end
  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);
  
  delay(1000); // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  */
  
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE); // Read Scratchpad

  debugPrint("  Data = ");
  debugPrint(present, HEX);
  debugPrint(" ");
  
  // we need 9 bytes
  for (uint8_t i = 0; i < 9; ++i)
  {
    data[i] = ds.read();
    debugPrint(data[i], HEX);
    debugPrint(" ");
  }
  debugPrint(" CRC=");
  debugPrint(OneWire::crc8(data, 8), HEX);
  debugPrintln();

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s)
  {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10)
    {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  }
  else
  {
    uint8_t cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    // default is 12 bit resolution, 750 ms conversion time
  }
  float celsius = (float)raw / 16.0;
  //float fahrenheit = celsius * 1.8 + 32.0;
  debugPrint("  Temperature = ");
  debugPrint(celsius);
  debugPrintln(" Celsius");
  
  return celsius;
}

