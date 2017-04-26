/*
	:DV company 2017

Программа для кнопки зажигания
https://github.com/dima201246/ignition

Версия: 2.0 stable
*/

// #define DEBUG
/*Включает сокращённое время, для отладки в протеусе*/

// #define MK_WITHOUT_LOCK
/*Отключает блокировку дверей, после запуска двигателя (!!!Использовать только с MK_WITHOUT_UNLOCK!!!)*/

#define MK_WITHOUT_UNLOCK
/*Отключает разблокировку дверей по нажатию той же кнопки,
что и запуск двигателя, также включает функцию быстрого мигания, когда активен режим "бесконечного старта"*/

#define MK_UNLIMITE_START
/*Отключает режим бесконечного стартера, при котором, после определённого количества неуспешного
запуска двигателя, стартер можно использовать неопределённое время*/

// #define MK_NO_BEEP
/*Отключает пищалку*/

#ifdef MK_WITHOUT_LOCK
#define MK_WITHOUT_UNLOCK
#endif

#define F_CPU	4800000UL

#include <avr/io.h>
#include <util/delay.h>

#define set_bit(m,b)	(m |= (1 << b))
#define unset_bit(m,b)	(m &= ~(1 << b))
#define bit_seted(m,b)	(m & (1 << b))

#define PIN_SH		PB0
#define PIN_ST		PB1
#define PIN_SIGNAL	PB1	// При высоком уровне - активно
#define PIN_DS		PB2
#define PIN_ENGINE	PB3	// Призапущенном уровень высокий
#define PIN_START	PB4	// При нажатой уровень низкий

#define SYS_BEEP	0	// Пищалка
#define SYS_LED		1	// Светодиод на кнопке
#define SYS_LOCK	2	// Реле блокировки дверей
#define SYS_STATE_0	3	// 1 фаза зажигания
#define SYS_UNLOCK	4	// Реле разблокировки дверей
#define SYS_ENGINE	5	// Логический сигнал работает ли двигатель
#define SYS_STATE_1	6	// Стартер

#define STATUS_LOCK	0	// Статус блокировки дверей (0 - не заблокированно)

#define COUNT_ENGINE_FAIL	3	// Сколько раз не должен запуститься двигатель для отключения ограничения времени работы стартера
#define COUNT_SIGNAL_FAIL	3	// Сколько раз пищать при провале попытки запуска двигателя

#ifndef DEBUG
#define DELAY_BEEP_BUTTON 250	// Время писка при нажатии кнопки
#define DELAY_BEEP_ENGINE 500	// Как долго пищать при готовности завести двигатель
#define DELAY_LED_SHORT	500	// Задержка моргания светодиода при отключении ограничении времени работы стартера
#define DELAY_BUTTON	1500	// Задержка для кнопки на включение двигателя
#define DELAY_ENGINE 1000	// Задержка для двигателя при пуске и остановке
#define DELAY_TIME	5000	// Задержка для первого этапа зажигания и максимальное время кручения стартера
#define DELAY_LOCK	1000	// Время подачи питания на замки дверей
#define DELAY_BEEP 1000	// Как долго будет писк при ошибке
#define DELAY_LED	1000	// Задержка моргания светодиода
#else
#define DELAY_BEEP_BUTTON 25	// Время писка при нажатии кнопки
#define DELAY_BEEP_ENGINE 50	// Как долго пищать при готовности завести двигатель
#define DELAY_LED_SHORT	50	// Задержка моргания светодиода при отключении ограничении времени работы стартера
#define DELAY_BUTTON	150	// Задержка для кнопки на включение двигателя
#define DELAY_ENGINE 100	// Задержка для двигателя при пуске и остановке
#define DELAY_TIME	500	// Задержка для первого этапа зажигания и максимальное время кручения стартера
#define DELAY_LOCK	100	// Время подачи питания на замки дверей
#define DELAY_BEEP 100	// Как долго будет писк при ошибке
#define DELAY_LED	100	// Задержка моргания светодиода
#endif

volatile uint8_t	sysByte = 0,	// Байт, который будет записан в сдвиговый регистр
					statusByte = 0,	// Байт для внутренней системы
					fail_count = 0;	// Счётчик "незапущенного" двигателя

volatile uint16_t	time_delay = 0,
					start_delay = 0,
					led_delay = DELAY_LED;	// Задержка моргания светодиода

void writeRegister()
{
	DDRB |= (1 << PIN_ST);

	for (uint8_t i = 0; i < 8; ++i)
	{
		if (sysByte & (1 << i))
			PORTB |= (1 << PIN_DS);
		else
			PORTB &= ~(1 << PIN_DS);

		PORTB |= (1 << PIN_SH);	// Запись данных
		PORTB &= ~(1 << PIN_SH);
	}

	PORTB &= ~(1 << PIN_DS);	// Сброс ножки данных
	PORTB |= (1 << PIN_ST);	// Закрепление данных
	PORTB &= ~(1 << PIN_ST);
	DDRB &= ~(1 << PIN_ST);
}

#ifndef MK_WITHOUT_LOCK
void lock_door(uint8_t status)
{
	if (status)
	{
		set_bit(sysByte, SYS_LOCK);
		set_bit(statusByte, STATUS_LOCK);
	}
	else
	{
		set_bit(sysByte, SYS_UNLOCK);
		unset_bit(statusByte, STATUS_LOCK);
	}

	writeRegister();
	_delay_ms(DELAY_LOCK);
	unset_bit(sysByte, SYS_LOCK);
	unset_bit(sysByte, SYS_UNLOCK);
	writeRegister();
}
#endif

void firstStart()
{
	set_bit(sysByte, SYS_STATE_0);
	writeRegister();
	_delay_ms(DELAY_TIME);
}

#ifndef MK_NO_BEEP
void signalFail()
{
	unset_bit(sysByte, SYS_ENGINE);
	unset_bit(sysByte, SYS_LED);

	for (uint8_t i = 0; i < COUNT_SIGNAL_FAIL*2; ++i)
	{
		if (bit_seted(sysByte, SYS_BEEP))
		{
			unset_bit(sysByte, SYS_BEEP);
		}
		else
		{
			set_bit(sysByte, SYS_BEEP);
		}

		writeRegister();
		_delay_ms(DELAY_BEEP);
	}

#ifndef MK_UNLIMITE_START
	fail_count++;
#endif
}
#endif

void engineStop()
{
#ifndef MK_UNLIMITE_START
	fail_count = 0;
#endif
	sysByte = 0;
	writeRegister();
	_delay_ms(DELAY_ENGINE);
}

void engineStart()
{
#ifndef MK_UNLIMITE_START
	if (fail_count >= COUNT_ENGINE_FAIL)
	{
		unset_bit(sysByte, SYS_STATE_1);
		set_bit(sysByte, SYS_LED);
#ifndef MK_UNLIMITE_START
		set_bit(sysByte, SYS_BEEP);
#endif
		writeRegister();
		while (bit_seted(PINB, PIN_START));
	}
#endif
	set_bit(sysByte, SYS_STATE_1);
	writeRegister();

#ifndef MK_UNLIMITE_START
	if (fail_count >= COUNT_ENGINE_FAIL)
	{
		while (!bit_seted(PINB, PIN_START));
#ifndef MK_UNLIMITE_START
		unset_bit(sysByte, SYS_BEEP);
#endif
	}
	else
#endif
	{
		for (uint16_t i = 0; i <= DELAY_TIME; ++i)
		{
			if (bit_seted(PINB, PIN_ENGINE))
			{
				unset_bit(sysByte, SYS_STATE_1);
				writeRegister();
				break;
			}
	
			_delay_ms(1);
		}
	}

	unset_bit(sysByte, SYS_STATE_1);
	writeRegister();

	_delay_ms(DELAY_ENGINE);

	if (!bit_seted(PINB, PIN_ENGINE))
	{
#ifdef MK_UNLIMITE_START
		fail_count++;
#else
		signalFail();
#endif
	}
	else
	{
#ifndef MK_UNLIMITE_START
		fail_count = 0;
#endif
		set_bit(sysByte, SYS_ENGINE);
		set_bit(sysByte, SYS_LED);
		writeRegister();
	}
}

int main()
{
	DDRB &= ~((1 << PIN_START) | (1 << PIN_ENGINE) | (1 << PIN_SIGNAL));
	DDRB |= (1 << PIN_DS);
	DDRB |= (1 << PIN_SH);

	while (bit_seted(PINB, PIN_SIGNAL));

	writeRegister();

	while (1)
	{
#ifdef MK_WITHOUT_UNLOCK
#ifndef MK_UNLIMITE_START
		led_delay = (fail_count >= COUNT_ENGINE_FAIL ? DELAY_LED_SHORT : DELAY_LED);	// Мигать чаще если мк готов к постоянному кручению стартера
#endif
#endif

		if ((!bit_seted(sysByte, SYS_ENGINE)) && (time_delay >= led_delay))
		{
			(bit_seted(sysByte, SYS_LED) ? unset_bit(sysByte, SYS_LED) : set_bit(sysByte, SYS_LED));
			time_delay = 0;
			writeRegister();
		}

		if ((bit_seted(sysByte, SYS_ENGINE)) && (!bit_seted(PINB, PIN_ENGINE)))
		{
			unset_bit(sysByte, SYS_ENGINE);
		}

		if (bit_seted(PINB, PIN_SIGNAL))
		{
			while (bit_seted(PINB, PIN_SIGNAL));

#ifndef MK_WITHOUT_UNLOCK
			if ((!bit_seted(statusByte, STATUS_LOCK)) && (!bit_seted(sysByte, SYS_STATE_0)))	// Запуск с брелка
#else
			if (!bit_seted(sysByte, SYS_STATE_0))	// Запуск с брелка
#endif
			{
				firstStart();
				engineStart();
#ifndef MK_WITHOUT_LOCK
				lock_door(1);
#endif
				continue;
			}

#ifndef MK_WITHOUT_UNLOCK
			if ((!bit_seted(statusByte, STATUS_LOCK)) && (bit_seted(PINB, PIN_ENGINE)))
#else
			if (bit_seted(PINB, PIN_ENGINE))
#endif
			{
				engineStop();
			}

#ifndef MK_WITHOUT_UNLOCK
			if (bit_seted(statusByte, STATUS_LOCK))	// Разблокировка с брелка
			{
				lock_door(0);
			}
#endif
			continue;
		}

		if ((!bit_seted(PINB, PIN_START)) && (!bit_seted(PINB, PIN_SIGNAL)))	// Была ли нажата кнопка сарта
		{
			start_delay = 0;

#ifndef MK_UNLIMITE_START
			set_bit(sysByte, SYS_BEEP);
			writeRegister();
			_delay_ms(DELAY_BEEP_BUTTON);
			unset_bit(sysByte, SYS_BEEP);
			writeRegister();
#endif

			while (!bit_seted(PINB, PIN_START))
			{
				_delay_ms(1);
				start_delay++;

#ifndef MK_UNLIMITE_START
				if (start_delay == DELAY_BUTTON)
				{
					set_bit(sysByte, SYS_BEEP);
					writeRegister();
					_delay_ms(DELAY_BEEP_ENGINE);
					unset_bit(sysByte, SYS_BEEP);
					writeRegister();
				}
#endif
			}

			if (((start_delay >= DELAY_BUTTON)) && (!bit_seted(PINB, PIN_SIGNAL)) && (bit_seted(PINB, PIN_ENGINE)) && (bit_seted(sysByte, SYS_STATE_0)))	// Остановка двигателя
			{
				engineStop();
				continue;
			}

			if ((start_delay >= DELAY_BUTTON) && (!bit_seted(PINB, PIN_SIGNAL)))	// Запуск "на прямую" или заглох двигатель
			{
				if (!bit_seted(sysByte, SYS_STATE_0))
				{
					firstStart();
				}

				if (!bit_seted(PINB, PIN_ENGINE))
				{
					engineStart();
				}

				continue;
			}

			if ((start_delay < DELAY_BUTTON) && (!bit_seted(sysByte, SYS_STATE_0)) && (!bit_seted(PINB, PIN_SIGNAL)))	// Включение приборов
			{
				firstStart();
				continue;
			}

			if ((start_delay < DELAY_BUTTON) && (bit_seted(sysByte, SYS_STATE_0)) && (!bit_seted(PINB, PIN_SIGNAL)))	// Выключение приборов
			{
				engineStop();
			}
		}

		// if (!bit_seted(sysByte, SYS_ENGINE))
		// {
			time_delay++;
			_delay_ms(1);
		// }
	}

	return 0;
}