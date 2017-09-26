#include "SD.h"
#include <RTClib.h>
#include <SHT1x.h>

#include "GardenMaster.h"

// Enable debug prints to serial monitor
#define MY_DEBUG
#define MY_NODE_ID 2

//#define FREE_RAM_DEBUG

#define SKETCH_NAME "GardenMaster"
#define SKETCH_MAJOR_VER "2"
#define SKETCH_MINOR_VER "0"

#define MOISTURE_CHILD_ID 1
#define TEMP_CHILD_ID 2


// Enable and select radio type attached
#define MY_RADIO_NRF24
#define MY_RF24_CE_PIN 49
#define MY_RF24_CS_PIN 53

#define MY_TRANSPORT_WAIT_READY_MS 3000		// if the GW is down, carry on after 3 sec

#include <MySensors.h>

static MyMessage msgMoist(MOISTURE_CHILD_ID, V_HUM);
static MyMessage msgTemp(TEMP_CHILD_ID, V_TEMP);

const char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};


const uint16_t SHT10_DATA = 6;				// moisture input (digital)
const uint16_t SHT10_CLK = 7;				//

// the dryness_threshold will be populated from the config.txt file on SD-card
// if the dryness_threshold is not properly configured, the alarm
// pin goes off and the event is logged down to SD log if possible

static uint16_t dryness_threshold = 0;
const uint16_t DRYNESS_LOWEST = 50;
const uint16_t DRYNESS_HIGHEST = 95;
const uint16_t TEMP_LOWEST = 10;

static SHT1x sht1x(SHT10_DATA, SHT10_CLK);


const uint16_t MAGNET_VALVE_PIN = 8;		// valve control output
const uint8_t ALARM_LED_PIN = 4;

static RTC_DS1307 rtc;

// change this to match your SD shield or module;
// Arduino Ethernet shield: pin 4
// Adafruit SD shields and modules: pin 10
// Sparkfun SD shield: pin 8
const int chipSelect = 10;


const uint16_t dailyWaterLimit = 15;
static uint16_t consumedToday = 0;

const char fileName[] = "water.txt";
static char configFileName[] = "config.txt";

static int today = 0;



int freeRam () {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

bool isValidDrynessThreshold(uint16_t dryness_threshold)
{
	return dryness_threshold >= DRYNESS_LOWEST && dryness_threshold <= DRYNESS_HIGHEST;
}

void log(const char * buf)
{
#ifdef FREE_RAM_DEBUG
	Serial1.print("before logging: ");
	Serial1.println(freeRam());
#endif

	Serial1.print( "opening file " );
	Serial1.print( fileName );
	Serial1.print( " for logging ... " );
	File f = SD.open( fileName, FILE_WRITE);

#ifdef FREE_RAM_DEBUG
	Serial1.print("after open: ");
	Serial1.println(freeRam());
#endif

	if (f)
	{
		Serial1.println( "SUCCESS" );

		if( !f.println(buf) )
		{
			digitalWrite( ALARM_LED_PIN, HIGH );

			Serial1.println( "Failed to log. Is the SD inserted? Do not forget to reset after insertion" );
		}

#ifdef FREE_RAM_DEBUG
		Serial1.print("after output: ");
		Serial1.println(freeRam());
#endif
		f.close();
	} else {
		Serial1.println( "FAILED" );

	// if the file didn't open, print an error:
		digitalWrite( ALARM_LED_PIN, HIGH );

		const char errMsg[] = "ERROR: error opening file for writing";
		Serial1.println(errMsg);
		log(errMsg);
	}
}


void setup()
{
	// debugging channel

	Serial1.begin(9600);

	while (!Serial1)
	{
	  delay(100);
	}

	Serial1.println("Welcome to GardenMaster 2.0");

	Serial1.println("Configuring soil moisture sensor pins");

	Serial1.println("Configuring magnetic valve control pin");

	pinMode(MAGNET_VALVE_PIN, OUTPUT);
	digitalWrite(MAGNET_VALVE_PIN, LOW);

	// ALRAM
	pinMode(ALARM_LED_PIN, OUTPUT);
	digitalWrite( ALARM_LED_PIN, LOW );



	// The Real Time Clock

	rtc.begin();

	if(rtc.isrunning())
		Serial1.println("RTC is running");
	else
	{
		Serial1.println("RTC is NOT running");
		rtc.adjust(DateTime(__DATE__, __TIME__));		// setup the current date and time initially
	}

	//rtc.adjust(DateTime(__DATE__, __TIME__));		// setup the current date and time initially

    DateTime now = rtc.now();

    Serial1.print(now.year(), DEC);
    Serial1.print('/');
    Serial1.print(now.month(), DEC);
    Serial1.print('/');
    Serial1.print(now.day(), DEC);
    Serial1.print(" (");
    Serial1.print(daysOfTheWeek[now.dayOfTheWeek()]);
    Serial1.print(") ");
    Serial1.print(now.hour(), DEC);
    Serial1.print(':');
    Serial1.print(now.minute(), DEC);
    Serial1.print(':');
    Serial1.print(now.second(), DEC);
    Serial1.println();

	// On the Ethernet Shield, CS is pin 4. It's set as an output by default.
	// Note that even if it's not used as the CS pin, the hardware SS pin
	// (10 on most Arduino boards, 53 on the Mega) must be left as an output
	// or the SD library functions will not work.
	pinMode(SS, OUTPUT);


	if ( !SD.begin(chipSelect, 11, 12, 13) )
	{
		// this error condition apparently cannot be logged to the SD-card
		Serial1.println("ERROR: SD card not found");
		digitalWrite( ALARM_LED_PIN, HIGH );

	}
	else
	{
		Serial1.println("SD card OK");

		// obtain dryness_threshold

		// open the file. note that only one file can be open at a time,
		// so you have to close this one before opening another.

		// check if the file exists.

		bool cfgFileExists = SD.exists(configFileName);

		if( cfgFileExists )
		{
			Serial1.println( "config.txt found" );

			File cfgFile = SD.open(configFileName);

			if ( cfgFile && cfgFile.available() )
			{
				Serial1.println("config.txt:");

				char buf[128];
				char * bufPtr = buf;

				// read from the file until there's nothing else in it:
				do
				{
					int c = cfgFile.read();

					if(c == '\r')
						continue;

					if(c == '\n' || !cfgFile.available() /*EOF*/)
					{
						if( !cfgFile.available() )
						{
							Serial1.println( "EOF" );
							*bufPtr++ = c;
						}

						*bufPtr = 0;
						bufPtr = buf;

					    Serial1.println( buf );

					    // now parsing. Currently, it is only one value on the very first line
					    // the dryness_threshold in %, the range 50 to 95 allowed.

					    dryness_threshold = atoi(buf);

					    if(!isValidDrynessThreshold(dryness_threshold))
					    {
					    	const char errMsg[] = "ERROR: invalid value for dryness_threshold (allowed 50-95)";
							Serial1.println(errMsg);
							log(errMsg);
							digitalWrite( ALARM_LED_PIN, HIGH );
					    }
					    break;
					}
					else
					{
						*bufPtr++ = c;
					}

				} while ( cfgFile.available() );

				// close the file:
				cfgFile.close();
			}
			else
			{
				// if the file didn't open, print an error. Someone probably
				// removed the SD card. It is an error situation. Raise an alarm
				const char errMsg[] = "ERROR: error opening config.txt for reading";
				Serial1.println(errMsg);
				log(errMsg);
				digitalWrite( ALARM_LED_PIN, HIGH );

			}
		}
		else
		{
			const char errMsg[] = "ERROR: config.txt does not exist";
			Serial1.println(errMsg);
			log(errMsg);
			digitalWrite( ALARM_LED_PIN, HIGH );

		}

	}

#ifdef FREE_RAM_DEBUG
	Serial1.print("at setup end: ");
	Serial1.println(freeRam());
#endif
}

void presentation()
{
	// Send the sketch version information to the gateway and Controller
	sendSketchInfo(SKETCH_NAME, SKETCH_MAJOR_VER "." SKETCH_MINOR_VER);

	// Register binary input sensor to sensor_node (they will be created as child devices)
	// You can use S_DOOR, S_MOTION or S_LIGHT here depending on your usage.
	// If S_LIGHT is used, remember to update variable type you send in. See "msg" above.
	present(MOISTURE_CHILD_ID, S_HUM);
	present(TEMP_CHILD_ID, S_TEMP);
}



// The loop function is called in an endless loop
void loop()
{
	DateTime now = rtc.now();

	if(!today)
	{
		today = now.day();
	}

	if(today != now.day())
	{
		consumedToday = 0;		// reset daily safety limit
		today = now.day();
	}

	char buf[128];

	float temp_c = 0;
	float humidity = 100;

	// Read values from the sensor
	temp_c = sht1x.readTemperatureC();
	humidity = sht1x.readHumidity();

	// Print the values to the serial port
	Serial1.print("Temperature: ");
	Serial1.print(temp_c, DEC);
	Serial1.print("C. Humidity: ");
	Serial1.print(humidity);
	Serial1.println("%");

	sprintf(buf, "%d%02d%02d %02d:%02d:%02d DRYNESS = %d, TEMP = %d", now.year(), now.month(), now.day(), now.hour(),
			now.minute(), now.second(), (int)humidity, (int)temp_c);

	log(buf);

	send(msgMoist.set((int)humidity));
	send(msgTemp.set((int)temp_c));


	Serial1.println( buf );

	if(temp_c < TEMP_LOWEST)
	{
		const char tempMsg[] = "Too low soil temperature. No watering will take place";
		log(tempMsg);
		Serial1.println(tempMsg);
	}

	if(temp_c >= TEMP_LOWEST &&
			consumedToday < dailyWaterLimit &&
			isValidDrynessThreshold(dryness_threshold) &&
			humidity <= dryness_threshold)
	{

		sprintf(buf, "%d%02d%02d %02d:%02d:%02d VALVE OPEN", now.year(), now.month(), now.day(), now.hour(),
				now.minute(), now.second());

		log(buf);

		Serial1.println( buf );

		digitalWrite(MAGNET_VALVE_PIN, HIGH);

		delay(300000L);		// 3 minutes

		digitalWrite(MAGNET_VALVE_PIN, LOW);

		consumedToday += 3;

		now = rtc.now();

		sprintf(buf, "%d%02d%02d %02d:%02d:%02d VALVE SHUT. Water time consumed: %d sec", now.year(), now.month(), now.day(), now.hour(),
				now.minute(), now.second(), consumedToday);

		Serial1.println( buf );
		log(buf);

		if(consumedToday > dailyWaterLimit)
		{
			const char msg[] = "Daily limit consumed. Resting till tomorrow";
			Serial1.println(msg);
			log(msg);
		}
	}
	delay(3600000L);	// sleep for one hour

}

