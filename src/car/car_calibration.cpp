#include <EEPROM.h>
#include <SoftwareSerial.h>

// --- Конфигурация портов ---
constexpr uint8_t R_DIR_PIN = 4;
constexpr uint8_t R_PWM_PIN = 5;
constexpr uint8_t L_PWM_PIN = 6;
constexpr uint8_t L_DIR_PIN = 7;

SoftwareSerial bluetooth(2, 3);

// --- Системные константы ---
constexpr int INITIAL_SPEED = 200;
constexpr int T_STEP_90 = 400;
constexpr int T_STEP_180 = 800;
constexpr int T_STEP_270 = 1200;
constexpr int T_STEP_360 = 1600;
constexpr uint8_t STORAGE_KEY = 0xAB;

// --- Адресация памяти (EEPROM) ---
enum MemoryAddr
{
	ADDR_STATUS = 0,
	ADDR_FWD_L, ADDR_FWD_R,
	ADDR_BWD_L, ADDR_BWD_R,
	ADDR_L_ROT_L, ADDR_L_ROT_R,
	ADDR_R_ROT_L, ADDR_R_ROT_R,
	ADDR_VAL_L, ADDR_VAL_R,
	ADDR_TIME_BASE = 11
};

// --- Состояния и Перечисления ---
enum MachineState { CALIBRATE_DIR, BALANCE_SPEED, CALIBRATE_TIME, ACTIVE_RUN };

enum MoveType { MOVE_FWD = 0, MOVE_BWD = 1, MOVE_ROT_L = 2, MOVE_ROT_R = 3 };

constexpr bool PIN_STATES[4][2] = {
	{LOW, LOW}, {HIGH, LOW}, {LOW, HIGH}, {HIGH, HIGH}
};

// --- Глобальные переменные ---
bool dir_map_l[4] = {LOW, HIGH, HIGH, LOW};
bool dir_map_r[4] = {LOW, HIGH, LOW, HIGH};
int pwm_l = INITIAL_SPEED;
int pwm_r = INITIAL_SPEED;
uint16_t rotation_intervals[4] = {T_STEP_90, T_STEP_180, T_STEP_270, T_STEP_360};

MachineState active_mode = CALIBRATE_DIR;
MoveType active_move = MOVE_FWD;
uint8_t ptr_combo = 0;
uint8_t ptr_time = 0;
bool focus_left = true;

// --- Базовое управление моторами ---
void drive(const bool l_dir, const int l_spd, const bool r_dir, const int r_spd)
{
	digitalWrite(L_DIR_PIN, l_dir);
	digitalWrite(R_DIR_PIN, r_dir);
	analogWrite(L_PWM_PIN, l_spd);
	analogWrite(R_PWM_PIN, r_spd);
}

void brakes() { drive(LOW, 0, LOW, 0); }

void execMove(const MoveType m) { drive(dir_map_l[m], pwm_l, dir_map_r[m], pwm_r); }

// --- Работа с энергонезависимой памятью ---
void saveSettings()
{
	EEPROM.write(ADDR_FWD_L, dir_map_l[MOVE_FWD]);
	EEPROM.write(ADDR_FWD_R, dir_map_r[MOVE_FWD]);
	EEPROM.write(ADDR_BWD_L, dir_map_l[MOVE_BWD]);
	EEPROM.write(ADDR_BWD_R, dir_map_r[MOVE_BWD]);
	EEPROM.write(ADDR_L_ROT_L, dir_map_l[MOVE_ROT_L]);
	EEPROM.write(ADDR_L_ROT_R, dir_map_r[MOVE_ROT_L]);
	EEPROM.write(ADDR_R_ROT_L, dir_map_l[MOVE_ROT_R]);
	EEPROM.write(ADDR_R_ROT_R, dir_map_r[MOVE_ROT_R]);
	EEPROM.write(ADDR_VAL_L, static_cast<uint8_t>(pwm_l));
	EEPROM.write(ADDR_VAL_R, static_cast<uint8_t>(pwm_r));

	for (int i = 0; i < 4; i++)
	{
		const int cell = ADDR_TIME_BASE + i * 2;
		EEPROM.write(cell, static_cast<uint8_t>(rotation_intervals[i] >> 8));
		EEPROM.write(cell + 1, static_cast<uint8_t>(rotation_intervals[i] & 0xFF));
	}
	EEPROM.write(ADDR_STATUS, STORAGE_KEY);
}

void loadSettings()
{
	dir_map_l[MOVE_FWD] = EEPROM.read(ADDR_FWD_L);
	dir_map_r[MOVE_FWD] = EEPROM.read(ADDR_FWD_R);
	dir_map_l[MOVE_BWD] = EEPROM.read(ADDR_BWD_L);
	dir_map_r[MOVE_BWD] = EEPROM.read(ADDR_BWD_R);
	dir_map_l[MOVE_ROT_L] = EEPROM.read(ADDR_L_ROT_L);
	dir_map_r[MOVE_ROT_L] = EEPROM.read(ADDR_L_ROT_R);
	dir_map_l[MOVE_ROT_R] = EEPROM.read(ADDR_R_ROT_L);
	dir_map_r[MOVE_ROT_R] = EEPROM.read(ADDR_R_ROT_R);
	pwm_l = EEPROM.read(ADDR_VAL_L);
	pwm_r = EEPROM.read(ADDR_VAL_R);

	for (int i = 0; i < 4; i++)
	{
		const int cell = ADDR_TIME_BASE + i * 2;
		rotation_intervals[i] = static_cast<uint16_t>(EEPROM.read(cell)) << 8 | EEPROM.read(cell + 1);
	}
}

// --- Обратная связь в Serial ---
void displayInfo()
{
	switch (active_mode)
	{
		case CALIBRATE_DIR:
			Serial.print(F("MODE:DIR_CAL | Act:"));
			Serial.print(active_move);
			Serial.print(F(" | Set:"));
			Serial.print(ptr_combo);
			Serial.print(F(" L/R:"));
			Serial.print(PIN_STATES[ptr_combo][0]);
			Serial.print(F("/"));
			Serial.println(PIN_STATES[ptr_combo][1]);
			break;
		case BALANCE_SPEED:
			Serial.print(F("MODE:SPEED | Edit:"));
			Serial.print(focus_left ? F("L") : F("R"));
			Serial.print(F(" | Spd L:"));
			Serial.print(pwm_l);
			Serial.print(F(" R:"));
			Serial.println(pwm_r);
			break;
		case CALIBRATE_TIME:
			Serial.print(F("MODE:TIME | Deg:"));
			Serial.print(ptr_time * 90 + 90);
			Serial.print(F(" | MS:"));
			Serial.println(rotation_intervals[ptr_time]);
			break;
		case ACTIVE_RUN:
			Serial.println(F("STATUS: RUNNING"));
			break;
	}
}

// --- Логика обработки команд ---
void processSerial(const char cmd)
{
	if (active_mode == CALIBRATE_DIR)
	{
		if (cmd == 'F') drive(PIN_STATES[ptr_combo][0], pwm_l, PIN_STATES[ptr_combo][1], pwm_r);
		else if (cmd == 'B') brakes();
		else if (cmd == 'X')
		{
			brakes();
			ptr_combo = (ptr_combo + 1) % 4;
			drive(PIN_STATES[ptr_combo][0], pwm_l, PIN_STATES[ptr_combo][1], pwm_r);
		} else if (cmd == 'S')
		{
			dir_map_l[active_move] = PIN_STATES[ptr_combo][0];
			dir_map_r[active_move] = PIN_STATES[ptr_combo][1];
			brakes();
			if (active_move < MOVE_ROT_R)
			{
				active_move = static_cast<MoveType>(active_move + 1);
				ptr_combo = 0;
			}
		} else if (cmd == 'R')
		{
			brakes();
			if (active_move < MOVE_ROT_R)
			{
				active_move = static_cast<MoveType>(active_move + 1);
				ptr_combo = 0;
			}
		} else if (cmd == 'C')
		{
			brakes();
			if (active_move > MOVE_FWD)
			{
				active_move = static_cast<MoveType>(active_move - 1);
				ptr_combo = 0;
			}
		} else if (cmd == 'T')
		{
			brakes();
			active_mode = BALANCE_SPEED;
			focus_left = true;
		}
	} else if (active_mode == BALANCE_SPEED)
	{
		if (cmd == 'F') execMove(MOVE_FWD);
		else if (cmd == 'B') brakes();
		else if (cmd == 'X') focus_left = !focus_left;
		else if (cmd == 'R')
		{
			if (focus_left) pwm_l = constrain(pwm_l + 5, 0, 255);
			else pwm_r = constrain(pwm_r + 5, 0, 255);
		} else if (cmd == 'L')
		{
			if (focus_left) pwm_l = constrain(pwm_l - 5, 0, 255);
			else pwm_r = constrain(pwm_r - 5, 0, 255);
		} else if (cmd == 'C') pwm_l = pwm_r = INITIAL_SPEED;
		else if (cmd == 'T')
		{
			brakes();
			active_mode = CALIBRATE_TIME;
			ptr_time = 0;
		}
	} else if (active_mode == CALIBRATE_TIME)
	{
		if (cmd == 'F')
		{
			execMove(MOVE_ROT_L);
			delay(rotation_intervals[ptr_time]);
			brakes();
		} else if (cmd == 'R')
			rotation_intervals[ptr_time] = constrain((int)rotation_intervals[ptr_time] + 50, 50,
			                                         9999);
		else if (cmd == 'L') rotation_intervals[ptr_time] = constrain((int)rotation_intervals[ptr_time] - 50, 50, 9999);
		else if (cmd == 'S') { if (ptr_time < 3) ptr_time++; } else if (cmd == 'X')
		{
			constexpr uint16_t defaults[4] = {T_STEP_90, T_STEP_180, T_STEP_270, T_STEP_360};
			rotation_intervals[ptr_time] = defaults[ptr_time];
		} else if (cmd == 'C') { if (ptr_time > 0) ptr_time--; } else if (cmd == 'T')
		{
			brakes();
			saveSettings();
			active_mode = ACTIVE_RUN;
		}
	} else if (active_mode == ACTIVE_RUN)
	{
		if (cmd == 'F') execMove(MOVE_FWD);
		else if (cmd == 'B') execMove(MOVE_BWD);
		else if (cmd == 'L') execMove(MOVE_ROT_L);
		else if (cmd == 'R') execMove(MOVE_ROT_R);
		else if (cmd == 'S') brakes();
		else if (cmd == 'X')
		{
			brakes();
			EEPROM.write(ADDR_STATUS, 0x00);
			active_mode = CALIBRATE_DIR;
			active_move = MOVE_FWD;
			pwm_l = pwm_r = INITIAL_SPEED;
		}
	}
	displayInfo();
}

void setup()
{
	Serial.begin(57600);

	pinMode(R_DIR_PIN, OUTPUT);
	pinMode(R_PWM_PIN, OUTPUT);
	pinMode(L_DIR_PIN, OUTPUT);
	pinMode(L_PWM_PIN, OUTPUT);

	bluetooth.begin(9600);
	brakes();

	if (EEPROM.read(ADDR_STATUS) == STORAGE_KEY)
	{
		loadSettings();
		active_mode = ACTIVE_RUN;
		Serial.println(F("Settings loaded. RUN mode."));
	} else { Serial.println(F("No config. CALIBRATION mode.")); }
	displayInfo();
}

void loop()
{
	if (bluetooth.available() > 0)
	{
		const char input = static_cast<char>(bluetooth.read());
		if (isspace(input)) return;

		Serial.print(F("Input: "));
		Serial.println(input);
		processSerial(input);
	}
}
