/**
 * SpeedCoach. From nwHacks 2020 with Ryan Barclay and Mark Polyakov.
 */


 // Libraries

#include <Wire.h>                //This is for I2C (screen & accelerometer)
#include <SoftwareSerial.h>     //This is for the GPS unit
#include <I2Cdev.h>             //This is for I2C (reffer to above)
#include <MPU6050.h>            //This is for (accelerometer & force unit)
#include <TinyGPS.h>            //This is for the GPS unit
#include <LiquidCrystal_I2C.h>  //This is for the LCD with I2C circut

//Globals to edit for set for user

#define GPS_PING_GAP 5          //Change this number to higher one if you have a
																//lower number of satellites where you normaly
																//row.
//If you know what you're doing you should also calibrate your gyro


//Globals for user not to touch
#define GRAVITY 16500

#define CAP_TOUCH_PIN 6       // connect to the OUT pin on touch sensor

#define GPS_RX_PIN 3          // Use RX and TX pins as labeled *on the GPS*
#define GPS_TX_PIN 2

#define LCD_I2C_ADDRESS 0x3F  //I2C address found via https://tinyurl.com/yyz4npoz
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
#define ACCEL_SAMPLES 50

// When acceleration above the acceleration threshold is detected, we start
// scanning for a maximum. As soon as acceleration starts to decrease, we record
// the maximum acceleration vector. Then,
#define STROKE_START_ACCEL 3000
#define STROKE_RECOVERY_TOLERANCE 150

#define STARTUP_TIMEOUT (10 * 1000)

//  initialization
LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, LCD_COLS, LCD_ROWS); //LCD initialize
MPU6050 accel;                                              //Gyro initialize
TinyGPS gps;                                                //Gps initialize(1)
SoftwareSerial ss(GPS_TX_PIN, GPS_RX_PIN);                  //Gps start serial connection

//Global ints, floats, ect.
int stroke_parity = 0;
bool stroking = false;
bool isPull = false;
// maximum accelerations, at the apexes of the strokes.
int16_t x_max, y_max, z_max,
  x_last, y_last, z_last;

int16_t accel_samples[ACCEL_SAMPLES];
unsigned distance = 0;

float last_lat  = 0, last_lon  = 0;
unsigned long init_trip_time  = 0,
	last_tick_time = 0,
	last_split_time = 0;
//end of Globals

// Resets things like distance, speed, strokes/min, etc.
void setup_trip() {
  for (int i = 0; i < ACCEL_SAMPLES; i++) {
    accel_samples[i] = GRAVITY; // force of gravity, theoretically
  }
	distance = 0;
	init_trip_time = 0;
	last_tick_time = 0;
	last_split_time = 0;
}

void startup(){
  lcd.setCursor(0 ,0);
  lcd.print("  DIY SPEED COACH!  ");
  lcd.setCursor(0 ,1);
  lcd.print("  Accelerometer:    ");
  lcd.setCursor(0 ,2);
  lcd.print(" GPS: Sat Count:    ");
  lcd.setCursor(0 ,3);
  lcd.print("Time:   :           ");
	delay(1000);
  accel.initialize();
  if (!accel.testConnection()) {
    lcd.setCursor(17 ,1);
    lcd.print("X");
  } else {
    lcd.setCursor(17,1);
    lcd.print("Y");
  }
  accel.setFullScaleAccelRange(0);
  accel.setXAccelOffset(ACCEL_X_OFFSET);
  accel.setYAccelOffset(ACCEL_Y_OFFSET);
  accel.setZAccelOffset(ACCEL_Z_OFFSET);
  setup_trip();

  unsigned long initTime = millis();
  unsigned long deltaTime = 0;
  while((10 > gps.satellites() || gps.satellites() == 255) && (deltaTime < STARTUP_TIMEOUT)){
    deltaTime = millis() - initTime; //in milliseconds
    int deltaTimeMin = (deltaTime/1000)/60; //(ms / 1000)/60 = 1s /60 = 1min
    int deltaTimeSec = (deltaTime/1000) - (deltaTimeMin*60);  //(ms / 1000) = 1s
    lcd.setCursor(6,3);
		char min_str[3];
		char sec_str[3];
		sprintf(min_str, "%02d", deltaTimeMin);
		sprintf(sec_str, "%02d", deltaTimeSec);
    lcd.print(String(min_str));
    lcd.setCursor(9,3);
    lcd.print(String(sec_str));
    lcd.setCursor(17,2);
		while (ss.available()) {
			gps.encode(ss.read());
		}
		if (gps.satellites() == 255) {
			lcd.print(0);
		}else{
			lcd.print(gps.satellites());
		}
  }
}

// Update GPS stuff and the display after each stroke.
void update_splits() {
	float lat, lon;
	gps.f_get_position(&lat, &lon);
	float split_distance = gps.distance_between(lat, lon, last_lat, last_lon);
	int split_secs, strokes_per_minute;

	if (last_split_time != 0) {
		distance += split_distance;
		split_secs = (int)((float)((millis() - last_split_time) / 2) / split_distance);
		int secs_since_last_split = (int)((millis() - last_split_time) / 1000);
		strokes_per_minute = GPS_PING_GAP * 60 / secs_since_last_split;
	} else {
		split_secs = 0;
		strokes_per_minute = 0;
	}
	
	last_lat = lat;
	last_lon = lon;
	last_split_time = millis();

	lcd.setCursor(10, 0);
	lcd.print(distance);
	lcd.setCursor(7, 1);
	lcd.print(split_secs);
	lcd.setCursor(14, 2);
	lcd.print(strokes_per_minute);
}

void setup() {
  Serial.begin(38400);
  ss.begin(9600);

  Wire.begin();

  lcd.begin();
  lcd.backlight();
	lcd.clear();

  startup();

  delay(1000);
  lcd.clear();
  lcd.setCursor(0, 0);
	lcd.print("Distance: ");
	lcd.setCursor(0, 1);
	lcd.print("Split: ");
	lcd.setCursor(0, 2);
	lcd.print("Strokes/Min: ");

	update_splits();
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

void loop() {
  update_acceleration();
  int16_t acceleration = update_acceleration();

  if (stroking) {
    if (abs(acceleration - GRAVITY) < STROKE_RECOVERY_TOLERANCE) {
      stroking = false;
			lcd.setCursor(0, 3);
			lcd.print(isPull);
      if (!isPull) {
				if (++stroke_parity == GPS_PING_GAP) {
					stroke_parity = 0;
					update_splits();
				}
			}
      isPull = !isPull;
    }
  } else {
    if (abs(acceleration - GRAVITY) > STROKE_START_ACCEL) {
      stroking = true;
    }
  }

	while (ss.available()) {
		gps.encode(ss.read());
	}

  delay(1);
}
