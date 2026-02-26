/*
/*
 * Seven Segment Cascade Control via Shift Registers
 * Target: ATmega328P (Arduino Uno)
 *
 * Описание:
 * Управление двумя семисегментными индикаторами через два регистра 74HC595.
 * Используется Timer1 для подсчета времени и программного SPI (bit-banging)
 * внутри прерывания, что обеспечивает неблокирующую работу.
 *
 * Подключение (настраивается в макросах PORTB):
 * DATA  (DS)    -> Pin 8  (PB0)
 * CLOCK (SH_CP) -> Pin 9  (PB1)
 * LATCH (ST_CP) -> Pin 10 (PB2)
 *
 * Логика каскада:
 * Данные передаются последовательно (MSB first).
 * Первые 8 бит уйдут во ВТОРОЙ регистр (десятки).
 * Вторые 8 бит останутся в ПЕРВОМ регистре (единицы).
 #1#

#include <HardwareSerial.h>
#include <avr/interrupt.h>
#include <avr/io.h>

// Тип индикатора: true для Общего Анода, false для Общего Катода
constexpr bool COMMON_ANODE = true;

// Пины порта B (Arduino D8-D13)
#define DATA_PIN  PB0  // Arduino D8
#define CLOCK_PIN PB1  // Arduino D9
#define LATCH_PIN PB2  // Arduino D10

// Прямое управление портами (макросы для удобства)
#define DATA_HIGH  PORTB |= (1 << DATA_PIN)
#define DATA_LOW   PORTB &= ~(1 << DATA_PIN)
#define CLOCK_HIGH PORTB |= (1 << CLOCK_PIN)
#define CLOCK_LOW  PORTB &= ~(1 << CLOCK_PIN)
#define LATCH_HIGH PORTB |= (1 << LATCH_PIN)
#define LATCH_LOW  PORTB &= ~(1 << LATCH_PIN)

// Кодировка цифр 0-9 для 7-сегментного индикатора
// Формат: ABCDEFG DP (или аналогичный в зависимости от разводки)
// Обычно: A=bit0, B=bit1, ..., G=bit6
const uint8_t digit_map[10] = {
	0b00111111, // 0
	0b00000110, // 1
	0b01011011, // 2
	0b01001111, // 3
	0b01100110, // 4
	0b01101101, // 5
	0b01111101, // 6
	0b00000111, // 7
	0b01111111, // 8
	0b01101111 // 9
};

// Переменные для обмена данными между ISR и Loop
// volatile обязательно, так как они меняются в прерывании
volatile uint8_t current_seconds = 0; // Текущее отображаемое время
volatile uint8_t next_seconds = 1; // Следующее значение (подготовленное)
volatile uint16_t shift_buffer = 0; // Буфер битов для передачи (16 бит)

// Состояния машины состояний внутри таймера
enum State { IDLE, SHIFTING, LATCHING };

volatile State tx_state = IDLE;
volatile int8_t bit_index = 15; // Индекс текущего бита (15 до 0)
volatile uint16_t ms_counter = 0; // Счётчик миллисекунд для секундомера

void setup()
{
	// 1. Настройка пинов на выход
	DDRB |= (1 << DATA_PIN) | (1 << CLOCK_PIN) | (1 << LATCH_PIN);

	// Начальное состояние
	LATCH_LOW;
	CLOCK_LOW;

	// 2. Настройка последовательного порта (UART)
	Serial.begin(9600);
	Serial.println(F("System Ready. Enter start value (00-99):"));

	// 3. Настройка Timer1 (16-бит)
	// Цель: Прерывание с частотой 1 кГц (1 мс)
	// F_CPU = 16 MHz. Prescaler = 64.
	// 16,000,000 / 64 = 250,000 тиков в секунду
	// Для 1000 Гц нужно делить на 250.
	// OCR1A = 250 - 1 = 249.

	cli(); // Отключить прерывания на время настройки

	TCCR1A = 0; // Обычный режим (не PWM)
	TCCR1B = 0;
	TCNT1 = 0; // Сброс счетчика

	OCR1A = 249; // Значение сравнения для 1 мс
	TCCR1B |= (1 << WGM12); // Режим CTC (Clear Timer on Compare Match)
	TCCR1B |= (1 << CS11) | (1 << CS10); // Предделитель 64
	TIMSK1 |= (1 << OCIE1A); // Разрешить прерывание по совпадению A

	sei(); // Включить глобальные прерывания
}

void loop()
{
	// Главный цикл занимается только обработкой пользовательского ввода.
	// Вся логика отображения и счета времени вынесена в таймеры.

	static char input_buf[3]; // Буфер для двух цифр + null terminator
	static uint8_t input_pos = 0;

	if (Serial.available() > 0)
	{
		char c = Serial.read();

		// Если это цифра
		if (c >= '0' && c <= '9') { if (input_pos < 2) { input_buf[input_pos++] = c; } }

		// Если ввод завершен (получено 2 цифры или перевод строки)
		// Упрощенная логика: как только получили 2 цифры -> применяем
		if (input_pos == 2)
		{
			input_buf[2] = '\0';
			int val = atoi(input_buf);

			if (val >= 0 && val <= 99)
			{
				// КРИТИЧЕСКАЯ СЕКЦИЯ: атомарное обновление
				// Мы меняем next_seconds, которое читается в ISR
				cli();
				next_seconds = val;
				sei();

				Serial.print(F("Next value set to: "));
				Serial.println(val);
			}

			// Сброс буфера
			input_pos = 0;
		}

		// Сброс по переводу строки для удобства (если ввели одну цифру и Enter)
		if (c == '\n' || c == '\r') { input_pos = 0; }
	}
}

// --- Обработчик прерывания Timer1 (1 кГц) ---
ISR(TIMER1_COMPA_vect)
{
	ms_counter++;

	// 1. Логика секундомера (раз в 1000 мс)
	if (ms_counter >= 1000)
	{
		ms_counter = 0;

		// Обновляем текущее значение на подготовленное
		current_seconds = next_seconds;

		// Готовим следующее значение (автоинкремент)
		// Если пользователь не вмешается, оно будет использовано через секунду
		next_seconds++;
		if (next_seconds > 99) next_seconds = 0;

		// Подготовка битовой маски для отображения (Decoding)
		const uint8_t tens = current_seconds / 10;
		const uint8_t ones = current_seconds % 10;

		uint8_t seg_tens = digit_map[tens];
		uint8_t seg_ones = digit_map[ones];

		// Инверсия для общего анода
		if (COMMON_ANODE)
		{
			seg_tens = ~seg_tens;
			seg_ones = ~seg_ones;
		}

		// Формируем 16-битное число для сдвига.
		// При каскадировании первый переданный байт (MSB 16-битного слова)
		// "проталкивается" во второй регистр (дальний от Arduino).
		// Поэтому старший байт -> Десятки, Младший байт -> Единицы.
		shift_buffer = (seg_tens << 8) | seg_ones;

		// Запускаем процесс передачи данных
		tx_state = SHIFTING;
		bit_index = 15; // Начинаем со старшего бита
	}

	// 2. Машина состояний для SPI (программная реализация)
	// Работает каждый тик таймера (1мс), пока идет передача.
	// Это обеспечивает обновление регистров без delay().

	if (tx_state == SHIFTING)
	{
		// Установка линии DATA
		if ((shift_buffer >> bit_index) & 0x01) { DATA_HIGH; } else { DATA_LOW; }

		// Импульс Clock (достаточно короткий, можно в одном прерывании)
		// 74HC595 работает на частотах МГц, переключение портов занимает наносекунды
		CLOCK_HIGH;
		CLOCK_LOW;

		bit_index--;

		if (bit_index < 0) { tx_state = LATCHING; }
	} else if (tx_state == LATCHING)
	{
		// Импульс Latch для вывода данных на выходы регистров
		LATCH_HIGH;
		LATCH_LOW;

		tx_state = IDLE; // Передача завершена, ждем следующей секунды
	}
}
*/
