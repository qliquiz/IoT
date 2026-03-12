#include <Arduino.h>

#define DIR_RIGHT 4
#define SPEED_RIGHT 5

#define DIR_LEFT 7
#define SPEED_LEFT 6

#define LEFT_FORWARD LOW
#define LEFT_BACKWARD HIGH

#define RIGHT_FORWARD LOW
#define RIGHT_BACKWARD HIGH

#define TRIG_FRONT 8
#define ECHO_FRONT 9

#define TRIG_LEFT 10
#define ECHO_LEFT 11

#define STOP_DISTANCE_FRONT 20
#define TARGET_DIST_LEFT 15
#define TOLERANCE 3
#define MAX_WALL_DIST 40

#define BASE_SPEED 150
#define CORRECT_SPEED 50
#define TURN_SPEED 150

void move(const bool left_dir, const int left_speed, const bool right_dir, const int right_speed)
{
	digitalWrite(DIR_LEFT, left_dir);
	digitalWrite(DIR_RIGHT, right_dir);
	analogWrite(SPEED_LEFT, left_speed);
	analogWrite(SPEED_RIGHT, right_speed);
}

void forward(const int speed) { move(LEFT_FORWARD, speed, RIGHT_FORWARD, speed); }

void backward(const int speed) { move(LEFT_BACKWARD, speed, RIGHT_BACKWARD, speed); }

void turn_left(const int steepness) { move(LEFT_FORWARD, 0, RIGHT_FORWARD, steepness); }

void turn_right(const int steepness) { move(LEFT_FORWARD, steepness, RIGHT_FORWARD, 0); }

void rotate_left(const int speed) { move(LEFT_BACKWARD, speed, RIGHT_FORWARD, speed); }


void rotate_right(const int speed) { move(LEFT_FORWARD, speed, RIGHT_BACKWARD, speed); }

int getDistance(const int trigPin, const int echoPin)
{
	digitalWrite(trigPin, LOW);
	delayMicroseconds(2);
	digitalWrite(trigPin, HIGH);
	delayMicroseconds(10);
	digitalWrite(trigPin, LOW);

	const long duration = pulseIn(echoPin, HIGH, 30000);
	if (duration == 0) return 999;

	return duration / 58;
}

void setup()
{
	Serial.begin(9600);

	pinMode(DIR_RIGHT, OUTPUT);
	pinMode(SPEED_RIGHT, OUTPUT);
	pinMode(DIR_LEFT, OUTPUT);
	pinMode(SPEED_LEFT, OUTPUT);

	pinMode(TRIG_FRONT, OUTPUT);
	pinMode(ECHO_FRONT, INPUT);

	pinMode(TRIG_LEFT, OUTPUT);
	pinMode(ECHO_LEFT, INPUT);

	delay(3000);
}

void loop()
{
	const int distFront = getDistance(TRIG_FRONT, ECHO_FRONT);
	const int distLeft = getDistance(TRIG_LEFT, ECHO_LEFT);

	if (distFront > 0 && distFront < STOP_DISTANCE_FRONT)
	{
		forward(0);
		delay(150);

		rotate_right(TURN_SPEED);

		while (true)
		{
			if (const int checkFront = getDistance(TRIG_FRONT, ECHO_FRONT);
				checkFront > STOP_DISTANCE_FRONT + 15) { break; }
			delay(40);
		}

		delay(200);

		forward(0);
		delay(150);
	} else
	{
		if (distLeft > 0 && distLeft < MAX_WALL_DIST)
		{
			if (distLeft < (TARGET_DIST_LEFT - TOLERANCE))
			{
				move(LEFT_FORWARD, BASE_SPEED + CORRECT_SPEED, RIGHT_FORWARD, BASE_SPEED - CORRECT_SPEED);
			} else if (distLeft > (TARGET_DIST_LEFT + TOLERANCE))
			{
				move(LEFT_FORWARD, BASE_SPEED - CORRECT_SPEED, RIGHT_FORWARD, BASE_SPEED + CORRECT_SPEED);
			} else { forward(BASE_SPEED); }
		} else { forward(BASE_SPEED); }
	}

	delay(30);
}
