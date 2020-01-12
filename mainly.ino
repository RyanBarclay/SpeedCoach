/**
 * SpeedCoach. From nwHacks 2020 with Ryan Barclay and Mark Polyakov.
 */


 // Libraries

#include <Wire.h>								//This is for I2C (screen & accelerometer)
#include <SoftwareSerial.h>			//This is for the GPS unit
#include <I2Cdev.h>							//This is for I2C (reffer to above)
#include <MPU6050.h>						//This is for (accelerometer & force unit)
#include <TinyGPS.h>						//This is for the GPS unit
#include <LiquidCrystal_I2C.h>	//This is for the LCD with I2C circut

//Globals

//Comment if not debuging
#define DEBUG
#define SKIP_GPS

#define GRAVITY 16500

#define CAP_TOUCH_PIN 6				// connect to the OUT pin on touch sensor

#define GPS_RX_PIN 3					// Use RX and TX pins as labeled *on the GPS*
#define GPS_TX_PIN 2

#define LCD_I2C_ADDRESS 0x3F	//I2C address found via https://tinyurl.com/yyz4npoz
#define LCD_ROWS 4
#define LCD_COLS 20

// MPU6050 address is known by the library
// accelerometer calibration
// when setFullScaleAccessRange(1) (i.e, 1G = GRAVITY)
/* #define ACCEL_X_OFFSET -20 */
/* #define ACCEL_Y_OFFSET -1587 */
/* #define ACCEL_Z_OFFSET 664 */
#define ACCEL_X_OFFSET -40
#define ACCEL_Y_OFFSET -3200
#define ACCEL_Z_OFFSET 1300

// end pin number and stuff, begin configuration
//****END OF REDONE CODE***
// How many samples of acceleration data to use. Higher values are more noise
// resistant but may ignore real strokes if too high.
#define ACCEL_SAMPLES 20

#define SPEED_SAMPLES 5

// Cancels stroke detection if this many milliseconds have elapsed since the
// start of a stroke.
#define STROKE_DETECT_TIMEOUT 5000

// When acceleration above the acceleration threshold is detected, we start
// scanning for a maximum. As soon as acceleration starts to decrease, we record
// the maximum acceleration vector. Then, 
#define STROKE_START_ACCEL 2000
#define STROKE_END_ACCEL 1000
#define STROKE_RECOVERY_TOLERANCE 500

enum stroke_detect_state {
	STROKE_DETECT_NULL,     // in between strokes or initial
	STROKE_DETECT_STARTED,  // The start of the stroke was detected, waiting for
                        	// point of maximum acceleration
	STROKE_DETECT_STROKING, // Partway through the stroke, waiting for the end of
													// the stroke.
	STROKE_DETECT_RECOVERY, // The stroke is ended. We are waiting for the
													// stroke-end acceleration to subside.
};

enum stroke_detect_state stroke_detect_state = STROKE_DETECT_NULL;

// maximum accelerations, at the apexes of the strokes.
int16_t x_max, y_max, z_max,
	x_last, y_last, z_last,
	acceleration_last,
	state_change_millis = 0; // set when a state might never naturally end.

//	initialization
LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, LCD_COLS, LCD_ROWS);	//LCD initialize
MPU6050 accel;																							//Gyro initialize
TinyGPS gps;																								//Gps initialize(1)
SoftwareSerial ss(GPS_TX_PIN, GPS_RX_PIN);									//Gps start serial connection

int16_t accel_samples[ACCEL_SAMPLES];
int16_t speed_samples[SPEED_SAMPLES] = { 0 };
unsigned distance;

// Resets things like distance, speed, strokes/min, etc.
void setup_trip() {
	for (int i = 0; i < ACCEL_SAMPLES; i++) {
		accel_samples[i] = GRAVITY; // force of gravity, theoretically
	}
}

void setup() {
	Serial.begin(38400);
	ss.begin(9600);

	Wire.begin();

	lcd.init();
	lcd.backlight();

	lcd.print("Init: Accelerometer");
	accel.initialize();
	if (!accel.testConnection()) {
#ifdef DEBUG
		lcd.setCursor(0,0);
		lcd.print("Accelerometer fail");
		while (1) { delay(1000); }
#endif
	} else {
#ifdef DEBUG
		lcd.setCursor(0,0);
		lcd.print("Accelerometer Works");
		delay(1000);
		lcd.clear();
#endif
	}


	accel.setFullScaleAccelRange(0);
	accel.setXAccelOffset(ACCEL_X_OFFSET);
	accel.setYAccelOffset(ACCEL_Y_OFFSET);
	accel.setZAccelOffset(ACCEL_Z_OFFSET);

	lcd.clear();
	lcd.setCursor(0, 0);
#ifndef SKIP_GPS
	// TODO: remove old text first
	lcd.print("Init: GPS");
	bool haveFix = false;
	while (!haveFix) {
		while (ss.available()) {
			if (gps.encode(ss.read())) {
				haveFix = true;
			}
		}
	}
#endif

	setup_trip();

	lcd.setCursor(0, 0);
	lcd.print("Ready!");
	delay(1000); // Give the user time to read
}

// inserts the given element at the end of the given array, shifting all other
// elements one to the left.
void shiftAndInsert(int16_t *arr, int length, int16_t new_elt) {
	for (int i = 0; i < length - 1; i++) arr[i] = arr[i + 1];
	arr[length - 1] = new_elt;
}

// Return the absolute value of the acceleration and updates [xyz]-last
// variables.
int16_t update_acceleration() {
	// maybe TODO: Use the interrupt to only update when new data is available.
	accel.getAcceleration(&x_last, &y_last, &z_last);
	shiftAndInsert(accel_samples, ACCEL_SAMPLES,
								 sqrt((int32_t)x_last*x_last +
											(int32_t)y_last*y_last +
											(int32_t)z_last*z_last));
	int acceleration = 0;
	for (int i = 0; i < ACCEL_SAMPLES; i++) {
		// we don't need to worry about the integer division stuff because 
		acceleration += accel_samples[i] / ACCEL_SAMPLES;
	}
	return abs(acceleration);
}

// Update GPS stuff and the display after each stroke.
void end_of_stroke() {
	/* lcd.clear(); */
	/* lcd.setCursor(0, 0); */
	/* lcd.print("") */
}

void loop() {
	update_acceleration();
	int16_t acceleration = update_acceleration();
	switch (stroke_detect_state) {
	case STROKE_DETECT_NULL:
	{
		if (abs(acceleration - GRAVITY) > STROKE_START_ACCEL) {
#ifdef DEBUG
			Serial.write("STROKE: NULL->STARTED\n");
#endif
			stroke_detect_state = STROKE_DETECT_STARTED;
			state_change_millis = millis();
		}
		break;
	}
	case STROKE_DETECT_STARTED:
	{
		if (acceleration < acceleration_last) {
#ifdef DEBUG
			Serial.write("STROKE: STARTED->STROKING\n");
#endif
			x_max = x_last;
			y_max = y_last;
			z_max = z_last;
			stroke_detect_state = STROKE_DETECT_STROKING;
		}
		break;
	}
	case STROKE_DETECT_STROKING:
	{
		// find the component of the acceleration that's opposite the maximum
		// acceleration from earlier
		if (-STROKE_END_ACCEL >
				((int32_t)x_last * x_max +
				 (int32_t)y_last * y_max +
				 (int32_t)z_last * z_max) /
			sqrt((int32_t)x_max*x_max +
					 (int32_t)y_max*y_max +
					 (int32_t)z_max*z_max)) {
#ifdef DEBUG
			Serial.write("STROKE: STARTED->RECOVERY\n");
#endif
			end_of_stroke();
			stroke_detect_state = STROKE_DETECT_RECOVERY;
			// If the acceleration really quickly changes from negative to positive,
			// it might never be close to 0/gravity.
			state_change_millis = millis();
		}
		break;
	}
	case STROKE_DETECT_RECOVERY:
	{
		if (abs(acceleration - GRAVITY) < STROKE_RECOVERY_TOLERANCE) {
#ifdef DEBUG
			Serial.write("STROKE: RECOVERY->NULL\n");
#endif
			stroke_detect_state = STROKE_DETECT_NULL;
		}
		break;
	}
	}
	acceleration_last = acceleration;

	if (stroke_detect_state != STROKE_DETECT_NULL &&
			millis() > state_change_millis + STROKE_DETECT_TIMEOUT) {
#ifdef DEBUG
		Serial.write("STROKE: TIMEOUT ->NULL\n");
#endif
		stroke_detect_state = STROKE_DETECT_NULL;
	}

	delay(100);
}
