#include "SD.h"
#include <RTClib.h>
#include <SHT1x.h>

#include "GardenMaster.h"

// Enable debug prints to serial monitor
#define MY_DEBUG
#define MY_NODE_ID 2

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

MyMessage msgMoist(MOISTURE_CHILD_ID, V_HUM);
MyMessage msgTemp(TEMP_CHILD_ID, V_TEMP);

static const char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};


const uint16_t SHT10_DATA = 6;				// moisture input (digital)
const uint16_t SHT10_CLK = 7;				//
const uint16_t THRESHOLD_DRYNESS = 50;

SHT1x sht1x(SHT10_DATA, SHT10_CLK);


const uint16_t MAGNET_VALVE_PIN = 8;		// valve control output
static const uint8_t ALARM_LED_PIN = 4;

RTC_DS1307 rtc;

// change this to match your SD shield or module;
// Arduino Ethernet shield: pin 4
// Adafruit SD shields and modules: pin 10
// Sparkfun SD shield: pin 8
const int chipSelect = 10;

int freeRam () {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
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
		Serial1.println("SD card not found");
		digitalWrite( ALARM_LED_PIN, HIGH );

	}
	else
	{
		Serial1.println("SD card OK");
	}

	Serial1.print("at setup: ");
	Serial1.println(freeRam());
}

const uint16_t dailyWaterLimit = 25;
uint16_t consumedToday = 0;

const char fileName[] = "water.txt";

int today = 0;

bool log(const char * buf)
{
	Serial1.print("before logging: ");
	Serial1.println(freeRam());

	Serial1.print( "opening file " );
	Serial1.println( fileName );
	File f = SD.open( fileName, FILE_WRITE);

	Serial1.print("after open: ");
	Serial1.println(freeRam());

	if (f)
	{
		if( !f.println(buf) )
		{
			digitalWrite( ALARM_LED_PIN, HIGH );

			Serial1.println( "Failed to log. Is the SD inserted? Do not forget to reset after insertion" );
			return false;
		}

		Serial1.print("after output: ");
		Serial1.println(freeRam());
		f.close();
	} else {
	// if the file didn't open, print an error:
		digitalWrite( ALARM_LED_PIN, HIGH );

		Serial1.print("error opening ");
		Serial1.print( fileName );
		Serial1.println(" for writing");
		return false;
	}

	return true;
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

	sprintf(buf, "%d%02d%02d %02d:%02d:%02d DRYNESS = %d", now.year(), now.month(), now.day(), now.hour(),
			now.minute(), now.second(), (int)humidity);

	log(buf);

	send(msgMoist.set((int)humidity));
	send(msgTemp.set((int)temp_c));


	Serial1.println( buf );


	if(consumedToday < dailyWaterLimit && humidity <= THRESHOLD_DRYNESS)
	{

		sprintf(buf, "%d%02d%02d %02d:%02d:%02d VALVE OPEN", now.year(), now.month(), now.day(), now.hour(),
				now.minute(), now.second());

		log(buf);

		Serial1.println( buf );

		digitalWrite(MAGNET_VALVE_PIN, HIGH);

		delay(300000L);		// 3 minutes

		digitalWrite(MAGNET_VALVE_PIN, LOW);

		consumedToday += 3;

		sprintf(buf, "%d%02d%02d %02d:%02d:%02d VALVE SHUT. Water time consumed: %d sec", now.year(), now.month(), now.day(), now.hour(),
				now.minute(), now.second(), consumedToday);

		log(buf);

		Serial1.println( buf );

		if(consumedToday > dailyWaterLimit)
		{
			log("Daily limit consumed. Resting till tomorrow");
			Serial1.print("Daily limit consumed. Resting till tomorrow");
		}
	}
	delay(3600000L);	// sleep for one hour

}

