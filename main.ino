/**
 * SpeedCoach. From nwHacks 2020 with Ryan Barclay and Mark Polyakov.
 */

#include <Wire.h>
#include <SoftwareSerial.h>
#include <I2Cdev.h>
#include <MPU6050.h>
#include <TinyGPS.h>
#include <LiquidCrystal_I2C.h>

// connect to the OUT pin on touch sensor
#define CAP_TOUCH_PIN 6

// Use RX and TX pins as labeled *on the GPS*
#define GPS_RX_PIN 8
#define GPS_TX_PIN 9

#define LCD_I2C_ADDRESS 0x3F
// MPU6050 address is known by the library

#define LCD_ROWS 4
#define LCD_COLS 20

// accelerometer calibration
// when setFullScaleAccessRange(1) (i.e, 1G = 8196)
#define ACCEL_X_OFFSET -20
#define ACCEL_Y_OFFSET -1587
#define ACCEL_Z_OFFSET 664

// end pin number and stuff, begin configuration

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
#define STROKE_START_ACCEL 4000
#define STROKE_END_ACCEL 2000

enum stroke_detect_state {
	STROKE_DETECT_NULL, // in between strokes or initial
	STROKE_DETECT_STARTED,  // The start of the stroke was detected, waiting for
                        	// point of maximum acceleration
	STROKE_DETECT_STROKING, // Partway through the stroke, waiting for the end of
													// the stroke.
};

enum stroke_detect_state stroke_detect_state = STROKE_DETECT_NULL;

// maximum accelerations, at the apexes of the strokes.
int16_t x_max, y_max, z_max,
	x_last, y_last, z_last,
	acceleration_last,
	state_change_millis = 0;


LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, LCD_COLS, LCD_ROWS);
MPU6050 accel;
TinyGPS gps;
SoftwareSerial ss(GPS_TX_PIN, GPS_RX_PIN);

int16_t accel_samples[ACCEL_SAMPLES];
int accel_sample_idx = 0;

int16_t speed_samples[SPEED_SAMPLES] = { 0 };
int speed_sample_idx = 0;

// Resets things like distance, speed, strokes/min, etc.
void setup_trip() {
	for (int i = 0; i < ACCEL_SAMPLES; i++) {
		accel_samples[i] = 8196; // force of gravity, theoretically
	}
}

void setup() {
	Serial.begin(38400);
	ss.begin(9600);

	Wire.begin();
	
	lcd.init();
	lcd.backlight();

	lcd.setCursor(0, 0);
	lcd.print("Init: Accelerometer");

	accel.initialize();
	if (!accel.testConnection()) {
		lcd.setCursor(0,0);
		lcd.print("Accelerometer fail");
		while (1) { delay(1000); }
	}
	accel.setFullScaleAccelRange(1);
	accel.setXAccelOffset(ACCEL_X_OFFSET);
	accel.setYAccelOffset(ACCEL_Y_OFFSET);
	accel.setZAccelOffset(ACCEL_Z_OFFSET);

	lcd.setCursor(0, 0);
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

	setup_trip();

	lcd.setCursor(0, 0);
	lcd.print("Ready!");
	delay(1000); // Give the user time to read
}

// Return the absolute value of the acceleration and updates [xyz]-last
// variables.
int16_t update_acceleration() {
	// maybe TODO: Use the interrupt to only update when new data is available.
	accel.getAcceleration(&x_last, &y_last, &z_last);
	accel_samples[accel_sample_idx++] = sqrt(
		(int32_t)x_last*x_last +
		(int32_t)y_last*y_last +
		(int32_t)z_last*z_last);
	int acceleration = 0;
	for (int i = 0; i < ACCEL_SAMPLES; i++) {
		// we don't need to worry about the integer division stuff because 
		acceleration += accel_samples[i] / ACCEL_SAMPLES;
	}
	return abs(acceleration);
}

// Update GPS stuff and the display after each stroke.
void end_of_stroke() {
	// TODO
}

void loop() {
	update_acceleration();
	int16_t acceleration = update_acceleration();
	switch (stroke_detect_state) {
	case STROKE_DETECT_NULL:
	{
		if (acceleration > STROKE_START_ACCEL) {
			stroke_detect_state = STROKE_DETECT_STARTED;
			state_change_millis = millis();
		}
		break;
	}
	case STROKE_DETECT_STARTED:
	{
		if (acceleration < acceleration_last) {
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
				(int32_t)x_last * x_max +
				(int32_t)y_last * y_max +
				(int32_t)z_last * z_max) {
			end_of_stroke();
			stroke_detect_state = STROKE_DETECT_NULL;
			// TODO: some holdoff so that a new stroke, in the wrong direction, isn't
			// triggered immediately.
		}
	}
	}
	acceleration_last = acceleration;

	if (millis() > state_change_millis + STROKE_DETECT_TIMEOUT) {
		stroke_detect_state = STROKE_DETECT_NULL;
	}
}
