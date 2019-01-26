#include "serial.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

#define F_CPU 10000000UL
#define UBRR 9600
#define BAUD (F_CPU/16/(UBRR+1))
#define NULL 0

static volatile uint8_t _rx_buffer[SERIAL_BUFFER_SIZE];
static volatile uint8_t _rx_buffer_head = 0;
static volatile uint8_t _rx_buffer_tail = 0;
	
static volatile uint8_t _tx_buffer[SERIAL_BUFFER_SIZE];
static volatile uint8_t _tx_buffer_head = 0;
static volatile uint8_t _tx_buffer_tail = 0;

ISR(USART0_UDRE_vect)
{
	uint8_t c = _tx_buffer[_tx_buffer_head];
	_tx_buffer_head = (_tx_buffer_head + 1) % SERIAL_BUFFER_SIZE;
	
	UDR0 = c;
	
	// 보낼꺼 모두 보내고 UDRE 인터럽트 끄기
	if (_tx_buffer_head == _tx_buffer_tail)
	{
		UCSR0B &= ~(1 << UDRIE0);
	}
}

ISR(USART0_RX_vect)
{
	uint8_t c = UDR0;
	uint8_t next_rx_tail = (_rx_buffer_tail + 1) % SERIAL_BUFFER_SIZE;
	
	// 다음 tail이 현재 head와 같다면 버퍼가 가득 찼으니 현재꺼는 버린다.
	if (next_rx_tail != _rx_buffer_head)
	{
		_rx_buffer[_rx_buffer_tail] = c;
		_rx_buffer_tail = next_rx_tail;
	}
}

void serial_init()
{
	UCSR0B = (1 << RXCIE0) | (1 << RXEN0) | (1 << TXEN0);
	UCSR0C = 0x06; // 8bit, no parity
	UBRR0H = (uint8_t) (BAUD >> 8);
	UBRR0L = (uint8_t) BAUD;
	
	sei();
}

serial_size_t serial_write(uint8_t c)
{
	if ( _tx_buffer_head == _tx_buffer_tail && (UCSR0A & (1 << UDRE0)) )
	{
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			UDR0 = c;
		}
		return 1;
	}
	
	uint8_t next_tx_tail = (_tx_buffer_tail + 1) % SERIAL_BUFFER_SIZE;
	// buffer가 꽉 찼을 경우 대기
	while (next_tx_tail == _tx_buffer_head);
	
	_tx_buffer[_tx_buffer_tail] = c;
	
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		_tx_buffer_tail = next_tx_tail;
		UCSR0B |= (1 << UDRIE0);
	}
	
	return 1;
}

serial_size_t serial_write_str(const char *str)
{
	serial_size_t s = 0;
	if (str == NULL) return s;
	while (*str != '\0')
	{
		serial_write((uint8_t) *str++);
		s++;
	}
	return s;
}

serial_size_t serial_write_int(long n)
{
	serial_size_t s = 0;
	char buf[20]; // 2^64승도 19자리에 불과, +1은 \0
	char *str = buf + 19;
	*str = '\0';
	
	if (n < 0)
	{
		serial_write('-');
		n = -n;
		s++;
	}
	
	do
	{
		char r = n % 10;
		n /= 10;
		
		*--str = '0' + r;
	} while (n);
	
	return s + serial_write_str(str);
}

serial_size_t serial_write_float(double n, uint8_t precision)
{
	serial_size_t s = 0;
	
	if (n < 0.0)
	{
		serial_write('-');
		n = -n;
		s++;
	}
	
	// (3.14159, 2) => 3.14, 0.005
	double rounding = 0.5;
	for (uint8_t i=0; i<precision; i++) rounding /= 10;
	
	int i = (int) n;
	double r = n - i;
	
	s += serial_write_int(i);
	if (precision > 0)
	{
		s += serial_write('.');
		for (uint8_t i=0; i<precision; i++) r *= 10;
		s += serial_write_int((int) r);
	}
	
	return s;
}

serial_size_t serial_available(void)
{
	if (_rx_buffer_tail < _rx_buffer_head)
	{
		return 256 - _rx_buffer_head + _rx_buffer_tail;
	}
	
	return _rx_buffer_tail - _rx_buffer_head;
}

int16_t serial_read(void)
{
	if (_rx_buffer_head == _rx_buffer_tail) return -1;
	
	uint8_t c = _rx_buffer[_rx_buffer_head];
	_rx_buffer_head = (_rx_buffer_head + 1) % SERIAL_BUFFER_SIZE;
	return c;
}