#include <avr/io.h>
#include <util/delay.h>

#define set_bit(m,b) (m |= (1 << b))
#define unset_bit(m,b) (m &= ~(1 << b))
#define bit_seted(m,b) (m & (1 << b))

#define PIN_SH PB0
#define PIN_DS PB2
#define PIN_ST PB3
#define PIN_STOP PB5	// При нажатой уровень низкий
#define PIN_START PB1	// При нажатой уровень низкий
#define PIN_ENGINE PB4	// Призапущенном уровень высокий

#define SYS_SIGNAL 3
#define SYS_UNLOCK 4
#define SYS_LOCK 5
#define SYS_STATE_0 6
#define SYS_STATE_1 7

#define STATUS_LOCK 0

#define DELAY_TIME 200	//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 5000 1000
#define DELAY_LOCK 100

volatile uint8_t	sysByte = 0,
					statusByte = 0;

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

void lockOn()
{
	set_bit(statusByte, STATUS_LOCK);
	set_bit(sysByte, SYS_LOCK);
	writeRegister();
	_delay_ms(DELAY_LOCK);
	unset_bit(sysByte, SYS_LOCK);
	writeRegister();
}

void lockOff()
{
	unset_bit(statusByte, STATUS_LOCK);
	set_bit(sysByte, SYS_UNLOCK);
	writeRegister();
	_delay_ms(DELAY_LOCK);
	unset_bit(sysByte, SYS_UNLOCK);
	writeRegister();
}

void firstStart()
{
	set_bit(sysByte, SYS_STATE_0);
	writeRegister();
	_delay_ms(DELAY_TIME);
	while (!bit_seted(PINB, PIN_ENGINE));
}

void signalFail()	// Доделать!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
{
	set_bit(sysByte, SYS_SIGNAL);
	writeRegister();
}

void engineStop()
{
	unset_bit(sysByte, SYS_STATE_0);
	writeRegister();
}

void engineStart()
{
	set_bit(sysByte, SYS_STATE_1);
	writeRegister();

	for (uint16_t i = 0; i <= DELAY_TIME; ++i)
	{
		if (!bit_seted(PINB, PIN_ENGINE))
		{
			unset_bit(sysByte, SYS_STATE_1);
			writeRegister();
			break;
		}

		_delay_ms(1);
	}

	unset_bit(sysByte, SYS_STATE_1);
	writeRegister();

	if (bit_seted(PINB, PIN_ENGINE))
	{
		signalFail();
	}
}

int main()
{
	DDRB &= ~(1 << PIN_START);
	DDRB &= ~(1 << PIN_STOP);
	DDRB &= ~(1 << PIN_ENGINE);
	DDRB &= ~(1 << PIN_ST);
	DDRB |= (1 << PIN_DS);
	DDRB |= (1 << PIN_SH);

	while (1)
	{
		if (!bit_seted(PINB, PIN_START))	// Была ли нажата кнопка сарта
		{
			if ((!bit_seted(PINB, PIN_STOP)) && (!bit_seted(PINB, PIN_ST)) && (!bit_seted(PINB, PIN_ENGINE)) && (bit_seted(sysByte, SYS_STATE_0)))	// Остановка двигателя
			{
				engineStop();

				while (!bit_seted(PINB, PIN_START));
				continue;
			}

			if ((!bit_seted(PINB, PIN_STOP)) && (!bit_seted(PINB, PIN_ST)))	// Запуск "на прямую" или заглох двигатель
			{
				if (!bit_seted(sysByte, SYS_STATE_0))
				{
					firstStart();
				}

				if (bit_seted(PINB, PIN_ENGINE))
				{
					engineStart();
				}

				while (!bit_seted(PINB, PIN_START));
				continue;
			}

			if ((bit_seted(PINB, PIN_STOP)) && (bit_seted(PINB, PIN_ST)) && (!bit_seted(statusByte, STATUS_LOCK)))	// Запуск с брелка
			{
				while (!bit_seted(PINB, PIN_START));

				lockOn();

				if (!bit_seted(sysByte, SYS_STATE_0))
				{
					firstStart();
				}

				if (bit_seted(PINB, PIN_ENGINE))
				{
					engineStart();
				}

				continue;
			}


			if ((bit_seted(PINB, PIN_ST)) && (bit_seted(statusByte, STATUS_LOCK)))	// Разблокировка с брелка
			{
				lockOff();
				continue;
			}
			
/*			if ((!bit_seted(statusByte, STATUS_LOCK)) && (bit_seted(sysByte, SYS_STATE_0)) && (!bit_seted(PINB, PIN_ST)) && (!bit_seted(PINB, PIN_ENGINE)))	// Блокировка дверей
			{
				lockOn();
				while (!bit_seted(PINB, PIN_START));
				continue;
			}
			
			if ((bit_seted(statusByte, STATUS_LOCK)) && (bit_seted(sysByte, SYS_STATE_0)) && (!bit_seted(PINB, PIN_ST)) && (!bit_seted(PINB, PIN_ENGINE)))	// Разблокировка дверей
			{
				lockOff();
				while (!bit_seted(PINB, PIN_START));
				continue;
			}*/

			if ((bit_seted(PINB, PIN_STOP)) && (!bit_seted(sysByte, SYS_STATE_0)) && (!bit_seted(PINB, PIN_ST)))	// Включение приборов
			{
				firstStart();
				while (!bit_seted(PINB, PIN_START));
				continue;
			}
		}
	}

	return 0;
}