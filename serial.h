#ifndef SERIAL_H_
#define SERIAL_H_

#include <stdint.h>

#define SERIAL_BUFFER_SIZE 64

#if (SERIAL_BUFFER_SIZE > 256)
	typedef uint16_t serial_size_t;
#else
	typedef uint8_t serial_size_t;
#endif

void serial_init();
serial_size_t serial_write(uint8_t c);
serial_size_t serial_write_int(long n);
serial_size_t serial_write_float(double n, uint8_t precision);
serial_size_t serial_write_str(const char *str);
serial_size_t serial_available(void);
int16_t serial_read(void);

#endif