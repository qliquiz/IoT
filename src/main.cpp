#include <Arduino.h>

/** Pins **/
constexpr int PIN_RX = 2; // INT0
constexpr int PIN_TX = 3; // OUT
constexpr int PIN_SR_DATA = 4; // 74HC595 DS
constexpr int PIN_SR_LATCH = 5; // 74HC595 ST_CP
constexpr int PIN_SR_CLOCK = 6; // 74HC595 SH_CP
constexpr int PIN_BTN = 7; // button
/** Timings **/
constexpr unsigned long UNIT_TIME = 200;
constexpr unsigned long TIMEOUT_GAP = UNIT_TIME * 3;
constexpr unsigned long TIMEOUT_RX = UNIT_TIME * 6;

/** Masks **/
byte digits[] = {
	0b11111010, // 0
	0b01100000, // 1
	0b11011100, // 2
	0b11110100, // 3
	0b01100110, // 4
	0b10110110, // 5
	0b10111110, // 6
	0b11100000, // 7
	0b11111110, // 8
	0b11110110 // 9
};

byte P_S = 0b10110110;
byte P_E = 0b10011110;
byte P_O = 0b11111010;
byte P_G = 0b10111010;
byte P_L = 0b00011010;
byte P_P = 0b11001110;
byte P_I = 0b01100000;
byte P_U = 0b01111100;
byte P_F = 0b10001110;
byte P_H = 0b00101110;
byte P_J = 0b01111000;
byte P_B = 0b00111110;
byte P_C = 0b10011010;
byte P_ERR = 0b00000001;
byte P_BLK = 0b00000000;

const char *LOOKUP_CODES[] = {
	"...", ".", "---", "--.", ".-..", ".--.", "..",
	"..-", "..-.", "....", ".---", "-...", "-.-.",
	".----", "..---", "...--", "....-", ".....",
	"-....", "--...", "---..", "----.", "-----"
};

const byte LOOKUP_PATTERNS[] = {
	P_S, P_E, P_O, P_G, P_L, P_P, P_I,
	P_U, P_F, P_H, P_J, P_B, P_C,
	digits[1], digits[2], digits[3], digits[4], digits[5],
	digits[6], digits[7], digits[8], digits[9], digits[0]
};

constexpr int DB_SIZE = 23;

/** RX ISR Variables **/
volatile unsigned long rxStartTime = 0;
volatile bool rxActive = false;
volatile char isrBuf[50]; // ring buffer
volatile int isrHead = 0; // write to
volatile int isrTail = 0; // read from

/** RX Logic Variables **/
String rxString = "";
unsigned long lastRxTime = 0;

/** TX FSM Variables **/
String txQueue = ""; // text queue
String txCurrentMorse = ""; // Morse code
int txIndex = 0; // index of `txQueue`
int txSignalIndex = 0; // index of `txCurrentMorse`
unsigned long txTimer = 0; // event timer
bool txStateHigh = false; // diode state

/** Sender state **/
enum TxState { TX_IDLE, TX_GAP, TX_SIGNAL };

TxState txState = TX_IDLE;

/** Interrupt, it's called automatically when the voltage on `PIN_RX` changes **/
void rxISR()
{
	const bool pin = digitalRead(PIN_RX);
	const unsigned long now = millis();

	if (pin == HIGH && !rxActive)
	{
		rxStartTime = now;
		rxActive = true;
	} else if (pin == LOW && rxActive)
	{
		const unsigned long dur = now - rxStartTime;
		rxActive = false;

		char sym = 0;
		// Rattle filter
		if (dur > 20)
		{
			if (static_cast<double>(dur) < UNIT_TIME * 2.5) sym = '.';
			else sym = '-';
		}

		// Writes to `isrBuf`
		if (sym != 0)
		{
			const int next = (isrHead + 1) % 50;
			if (next != isrTail)
			{
				isrBuf[isrHead] = sym;
				isrHead = next;
			}
		}
	}
}

/** Utils **/
void showByte(const byte data)
{
	digitalWrite(PIN_SR_LATCH, LOW);
	shiftOut(PIN_SR_DATA, PIN_SR_CLOCK, LSBFIRST, data);
	digitalWrite(PIN_SR_LATCH, HIGH);
}

String getCode(const char c)
{
	if (c == 'S') return LOOKUP_CODES[0];
	if (c == 'E') return LOOKUP_CODES[1];
	if (c == 'O') return LOOKUP_CODES[2];
	if (c == 'G') return LOOKUP_CODES[3];
	if (c == 'L') return LOOKUP_CODES[4];
	if (c == 'P') return LOOKUP_CODES[5];
	if (c == 'I') return LOOKUP_CODES[6];
	if (c == 'U') return LOOKUP_CODES[7];
	if (c == 'F') return LOOKUP_CODES[8];
	if (c == 'H') return LOOKUP_CODES[9];
	if (c == 'J') return LOOKUP_CODES[10];
	if (c == 'B') return LOOKUP_CODES[11];
	if (c == 'C') return LOOKUP_CODES[12];
	if (c == '1') return LOOKUP_CODES[13];
	if (c == '2') return LOOKUP_CODES[14];
	if (c == '3') return LOOKUP_CODES[15];
	if (c == '4') return LOOKUP_CODES[16];
	if (c == '5') return LOOKUP_CODES[17];
	if (c == '6') return LOOKUP_CODES[18];
	if (c == '7') return LOOKUP_CODES[19];
	if (c == '8') return LOOKUP_CODES[20];
	if (c == '9') return LOOKUP_CODES[21];
	if (c == '0') return LOOKUP_CODES[22];
	return "";
}

byte getPattern(const String &code)
{
	for (int i = 0; i < DB_SIZE; i++) if (String(LOOKUP_CODES[i]) == code) return LOOKUP_PATTERNS[i];
	return P_ERR;
}

/** Main **/
void setup()
{
	pinMode(PIN_RX, INPUT);
	pinMode(PIN_TX, OUTPUT);
	pinMode(PIN_BTN, INPUT);
	pinMode(PIN_SR_DATA, OUTPUT);
	pinMode(PIN_SR_LATCH, OUTPUT);
	pinMode(PIN_SR_CLOCK, OUTPUT);

	digitalWrite(PIN_TX, LOW);

	attachInterrupt(digitalPinToInterrupt(PIN_RX), rxISR, CHANGE); // interrupt ON

	Serial.begin(9600);
	Serial.println("SYSTEM IS READY");

	// test
	/*showByte(digits[8]);
	delay(500);
	showByte(P_BLK);*/
}

void loop()
{
	const unsigned long now = millis();

	// --- RX: READ THE BUFFER ---
	while (isrHead != isrTail)
	{
		const char c = isrBuf[isrTail];
		isrTail = (isrTail + 1) % 50;
		rxString += c;
		lastRxTime = now;
		Serial.print(c);
	}

	// --- RX: TIMEOUT DECODE ---
	if (rxString.length() > 0 && now - lastRxTime > TIMEOUT_RX)
	{
		Serial.print(" -> ");
		const byte pattern = getPattern(rxString);
		showByte(pattern);
		Serial.println(pattern == P_ERR ? "?" : "OK");
		rxString = "";
	}

	// --- TX: MANUAL INPUT (Button) ---
	if (txState == TX_IDLE)
	{
		const bool btn = digitalRead(PIN_BTN);
		digitalWrite(PIN_TX, btn);
	}

	// --- TX: AUTO INPUT (Serial) ---
	if (Serial.available() && txState == TX_IDLE)
	{
		String input = Serial.readStringUntil('\n');
		input.trim();
		input.toUpperCase();
		if (input.length() > 0)
		{
			txQueue = input;
			txIndex = 0;
			txState = TX_GAP;
			txTimer = now;
			Serial.print("Sending: ");
			Serial.println(txQueue);
		}
	}

	// --- TX: FSM ---
	switch (txState)
	{
		case TX_IDLE:
			break;

		case TX_GAP: // the silence between the past and the new letters
			if (now >= txTimer) // time for the next letter
			{
				if (txIndex >= txQueue.length())
				{
					txState = TX_IDLE;
					Serial.println("Done.");
				} else
				{
					const char c = txQueue[txIndex];
					txCurrentMorse = getCode(c);
					txSignalIndex = 0;

					if (txCurrentMorse == "")
					{
						txIndex++;
						txTimer = now + UNIT_TIME * 4; // big pause between words
					} else
					{
						txState = TX_SIGNAL;
						txStateHigh = false;
					}
				}
			}
			break;

		case TX_SIGNAL:
			if (now >= txTimer)
			{
				if (txStateHigh)
				{
					// pause between elements
					digitalWrite(PIN_TX, LOW);
					txStateHigh = false;
					txTimer = now + UNIT_TIME;
				} else
				{
					if (txSignalIndex >= txCurrentMorse.length())
					{
						// letter ended
						txState = TX_GAP;
						txTimer = now + UNIT_TIME * 3; // pause between letters
						txIndex++;
					} else
					{
						// next element
						const char s = txCurrentMorse[txSignalIndex++];
						digitalWrite(PIN_TX, HIGH);
						txStateHigh = true;
						if (s == '.') txTimer = now + UNIT_TIME;
						else txTimer = now + UNIT_TIME * 3;
					}
				}
			}
			break;
	}
}
