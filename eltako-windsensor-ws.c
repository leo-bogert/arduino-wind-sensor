// Arduino Uno pulse counter for "Eltako Windsensor WS".
// 
// WARNING: I did NOT have the actual physical wind sensor at hand
// when I wrote this code. I merely tested the code by manually
// connecting the pins on the Arudino using a bended paper clip :)
// Also I'm not an Arudino expert so read this code with a grain of
// salt.
// FWIW I tried to write very verbose code so even non-programmers can
// make use of this :)
// 
// Setup:
// - Connect one wire of the wind sensor to "DIGITAL PWM" pin 2
//   of an Arduino Uno.
// - Connect the other wire to GND.
//
// It will print the number of pulses per second and minute every
// 60 seconds. To view that use "Tools / Serial monitor" in the
// Arduino PC software.
//
// Notice that the Internet says that this particular wind sensor
// might do more than one pulse per rotation.
// The onboard LED labeled as "L" of the Arduino will invert its state
// from off to on and vice versa at every pulse of the wind sensor.
// You can use that to determine the number of pulses per rotation by
// hand.
// 
// Also the LED will provide you with nice visual feedback of whether
// the Arduino is still measuring :)

// For constant UINT_MAX / ULONG_MAX
#include <limits.h>
// For log()
#include <math.h>  

// Pin which will connect the wind sensor and be used as source for
// the counter interrupt.
// Arduino Uno supports pin 2 and 3 with interrupts.
// See: https://www.arduino.cc/reference/en/language/functions/external-interrupts/attachinterrupt/
const byte pin = 2;

// The pulse count will be measured for this many seconds before it
// is printed. This defaults to the value of 60 which is uncomfortably
// high for debugging because wind sensors can rotate very slowly so
// we need to measure for a long time to get an accurate measurement.
const unsigned int measurementDelaySeconds = 60;

// Number of pulses as counted by the interrupt in the current
// measurement cycle.
// Must be volatile to prevent the compiler from making the code cache
// it because it is modified off the main thread, i.e. in the
// interrupt function countPulse().
// Unsigned int is large enough as a datatype:
// Its maximum value of 65535 being reached in 60 seconds would mean
// 1000 rotations per second - which a wind sensor cannot do.
volatile unsigned int pulseCount = 0;

// Don't count pulses if the last pulse happened up to this amount of
// milliseconds ago. In other words this is the minimal length of a
// pulse which we can measure (that way of describing it is important
// when trying to understand the documentation of the below
// maxImpulsesPerSecond).
// This is for debouncing the switch in the wind sensor.
// NOTICE: This is the most important value to configure as I did NOT
// have an actual wind sensor at hand when I wrote this code and thus
// was not able to check whether the debounce delay is high enough for
// the particular switch in the wind sensor.
// The default of 10 should at least not be too high: 10ms means that
// we could register up to 100 impulses per second. If the wind sensor
// does 2 impulses per rotation that is 50 rotations per second, which
// seems awfully high for a wind sensor.
const unsigned int debounceDelayMillis = 10;

// Time in milliseconds at which the last pulse was accepted by the
// debounce code. No further impulses will be counted until the
// debounceDelayMillis has passed.
unsigned long lastPulse = 0;

// If the pulse count goes above this value the debounceDelayMillis
// is too high compared to the rotation speed and an error will be
// printed by loop().
// Arbitrarily chosen to be 50 % of the maximum countable pulses as
// determined by the debounce delay.
const float maxImpulsesPerSecond
	= ((float)1000 / debounceDelayMillis) * 0.5f;

byte ledState = LOW;

void setup() 
{
	Serial.begin(9600); 
	
	pinMode(pin, INPUT);
	// Tells the Arduino to attach the pin to +5V by a 20 kOhm
	// resistor and thereby define its idle state as HIGH.
	// This is necessary because when the wind sensor contact is not
	// closed the pin will not have an electrical connection to
	// anything. That means that its voltage and thus HIGH/LOW state
	// would have to be considered as undefined and thus we couldn't
	// measure it. In other words:
	// If the pin isn't connected to anything it may show random
	// on/off behavior due to electrical noise, e.g. from nearby
	// electromagnetic fields inducing a voltage.
	// See:
	// https://www.arduino.cc/reference/en/language/functions/digital-io/pinmode/
	// https://www.arduino.cc/en/Tutorial/DigitalPins
	pinMode(pin, INPUT_PULLUP);
	// Count voltage going from HIGH to LOW as a pulse by
	// configurating the interrupt controller to call countPulse()
	// on a falling signal edge.
	// Change to LOW is the pulse because we configured the pin to
	// be at HIGH by default, and because the other wire of the wind
	// sensor is attached to GND, i.e. LOW. So if the wind sensor
	// contact is closed then the pin will be pulled to low.
	// See: https://www.arduino.cc/reference/en/language/functions/external-interrupts/attachinterrupt/
	attachInterrupt(digitalPinToInterrupt(pin), countPulse, FALLING);
	// Disable counting of pulses at first to ensure the first
	// measurement in loop() is correct.
	noInterrupts();
	
	pinMode(LED_BUILTIN, OUTPUT);
	// Initialize the LED to match the ledState variable.
	digitalWrite(LED_BUILTIN, ledState);
}

void countPulse() {
	// WARNING: millis() is NOT updated during an interrupt!
	// We can only measure the value at the start of it - which is
	// sufficient for our purposes here.
	unsigned long time = millis();
	unsigned long timeSinceLastPulse;
	
	if(time >= lastPulse)
		timeSinceLastPulse = time - lastPulse;
	else {
		// The Arduino has been running for so long that the time
		// counter reached ULONG_MAX and overflowed back to 0.
		// This happens after ~ 50 days.
		// See: https://www.arduino.cc/reference/en/language/functions/time/millis/
		timeSinceLastPulse = (ULONG_MAX - lastPulse) + time;
	}
	
	if(timeSinceLastPulse <= debounceDelayMillis)
		return;
	
	lastPulse = time;
	
	++pulseCount;
	// If you plan to use this with something faster than a wind
	// sensor you should check for overflow!
	/*
	if(pulseCount == UINT_MAX)
		Serial.print("ERROR: RPM too high, counter will overflow!!");
	*/
	
	ledState = (ledState == HIGH ? LOW : HIGH);
	digitalWrite(LED_BUILTIN, ledState);
}

void loop()
{ 
	pulseCount = 0; 
	// Enable counting of pulses.
	interrupts();
	// Cast variable up to unsigned long to leave some room for the
	// multiplication by 1000. delay() consumes ulong anyway.
	delay((unsigned long)measurementDelaySeconds * 1000); 
	// Disable counting of pulses.
	noInterrupts();
	
	float pulsesPerSecond
		= (float)pulseCount / measurementDelaySeconds;
	float pulsesPerMinute
		= (float)pulseCount*60 / measurementDelaySeconds;
	
	Serial.print("Pulses measured: "); 
	Serial.println(pulseCount);
	Serial.print("Pulses per second: "); 
	// Print the pulses per second with the number of decimals chosen
	// as needed to represent the maximal impulse frequency we can
	// reliably measure as determined by the debounce delay.
	// In other words: Don't show more decimals than we're capable
	// of measuring accurately.
	int displayedDecimals
		= numberOfDecimalsNeeded((float)1 / maxImpulsesPerSecond);
	Serial.println(pulsesPerSecond, displayedDecimals);
	Serial.print("Pulses per minute: ");
	// Same number of decimals as with seconds: We're using the same
	// input value to calculate this as with the seconds so the
	// precision has not changed as the input has not changed.
	Serial.println(pulsesPerMinute, displayedDecimals);
	
	// The wind sensor is producing more impulses than we could
	// measure at most with the given debounce delay.
	if(pulsesPerSecond >= maxImpulsesPerSecond) {
		Serial.println(
			"ERROR: Debounce delay too high for impulse speed!");
	}
	
	Serial.println("-----------------------------------------------");
}

// Returns the number of decimal places needed to display all numbers
// >= smallestNumber
int numberOfDecimalsNeeded(float smallestNumber) {
	// The logarithm of base 10 of X is the number which satisfies
	///     10 ^ log10(X) = X.
	// Thus if our X is 0.001, we get log10(x) = -3.
	// So at first glance abs(log(x))) is the number of decimal places
	// needed to display all numbers >= smallestNumber.
	// However if smallestNumber is not a power of 10 then this may be
	// something such as e.g. 1.2345 - then 2 decimal places would be
	// better. Thus we always round up - which is what ceil() is for.
	return ceil(abs(log10(smallestNumber)));
}