#include <Arduino.h>

const uint8_t led_pins[] = {PB0, PB1, PB2, PB4, PB5};
const int led_periods[] = {10, 20, 30, 40, 50};
constexpr int NUM_LEDS = 5;

volatile unsigned long timer_ticks = 0;

void setup()
{
	uint8_t pin_mask = 0;
	for (int i = 0; i < NUM_LEDS; i++) { pin_mask |= 1 << led_pins[i]; }

	DDRB |= pin_mask;
	PORTB &= ~pin_mask;

	cli();

	TCCR1A = 0;
	TCCR1B = 0;

	TCCR1B |= 1 << WGM12;
	TCCR1B |= 1 << CS11 | 1 << CS10;

	OCR1A = 9999;

	TIMSK1 |= 1 << OCIE1A;

	sei();
}

ISR(TIMER1_COMPA_vect)
{
	timer_ticks++;

	for (int i = 0; i < NUM_LEDS; i++) { if (timer_ticks % led_periods[i] == 0) { PORTB ^= 1 << led_pins[i]; } }
}

void loop()
{
}
