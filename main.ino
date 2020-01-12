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
#define CAP_TOUCH_PIN 6				// connect to the OUT pin on touch sensor

#define GPS_RX_PIN 8					// Use RX and TX pins as labeled *on the GPS*
#define GPS_TX_PIN

#define LCD_I2C_ADDRESS 0x3F	//I2C address found via https://tinyurl.com/yyz4npoz
#define LCD_ROWS 4
#define LCD_COLS 20

// MPU6050 address is known by the library
// accelerometer calibration
// when setFullScaleAccessRange(1) (i.e, 1G = 8196)
#define ACCEL_X_OFFSET -20
#define ACCEL_Y_OFFSET -1587
#define ACCEL_Z_OFFSET 664

// end pin number and stuff, begin configuration
//****END OF REDONE CODE***
// How many samples of acceleration data to use. Higher values are more noise
// resistant but may ignore real strokes if too high.
#define ACCEL_SAMPLES 20

#define STROKE_START_ACCEL 4000
#define STROKE_END_ACCEL 2000
#define STROKE_END_DEG 30



//	initialization
LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, LCD_COLS, LCD_ROWS);	//LCD initialize
MPU6050 accel;																							//Gyro initialize
TinyGPS gps;																								//Gps initialize(1)
SoftwareSerial ss(GPS_TX_PIN, GPS_RX_PIN);									//Gps start serial connection

int16_t accel_samples[ACCEL_SAMPLES];
int accel_sample_idx = 0;

void setup() {
	for (int i = 0; i < ACCEL_SAMPLES; i++) {
		accel_samples[i] = 8196; // force of gravity, theoretically
	}

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
	}else{
		#ifdef DEBUG
			lcd.setCursor(0,0)
			lcd.print("Acceleromete Work")
			delay(1000);
			lcd.clear()
		#endif
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

	lcd.setCursor(0, 0);
	lcd.print("Ready!");
	delay(1000); // Give the user time to read
}

int16_t x_start, y_start, z_start;
bool in_stroke = false;

void loop() {
	update_acceleration();
	int16_t acceleration = average_acceleration();
	if (!in_stroke && acceleration > STROKE_START_ACCEL) {
		in_stroke = true;
		accel.getAcceleration(&x_start, &y_start, &z_start);
	}
}

// Read the latest acceleration data from the sensor and add it to the moving
// average.
void update_acceleration() {
	// maybe TODO: Use the interrupt to only update when new data is available.
	int16_t x, y, z;
	accel.getAcceleration(&x, &y, &z);
	accel_samples[accel_sample_idx++] = sqrt((int32_t)x*x + (int32_t)y*y + (int32_t)z*z);
}

int16_t average_acceleration() {
	int16_t result = 0;
	for (int i = 0; i < ACCEL_SAMPLES; i++) {
		result += accel_samples[i] / ACCEL_SAMPLES;
	}
}
