
#include "Comdef.h"
#include "OSAL.h"
#include <stdarg.h>
#include <stdio.h>

#include "ther_uart.h"
#include "ther_uart_comm.h"

#define MODULE "[UART COMM] "

static unsigned char log_level = LOG_DBG;

#define PRINT_BUF_LEN 200
static char print_buf[PRINT_BUF_LEN];


#define UART_PORT 0

#define PRINT_PORT UART_PORT

static void msg_dispatch(unsigned char port, unsigned char *buf, unsigned char len)
{
	uart_send(port, buf, len);

	/*
	 * add '\r\n' to send the data to uart immediately
	 */
//	uart_send(port, "\r\n", 2);

	return;
}

int print(unsigned char level, char *fmt, ...)
{
	va_list args;
	int n;

	if (level < log_level)
		return 0;

	va_start(args, fmt);
	n = vsprintf(print_buf, fmt, args);
//	n = vsnprintf(print_buf, PRINT_BUF_LEN, fmt, args);
	if (n > 100)
		n = 100;
	uart_send(PRINT_PORT, (unsigned char *)print_buf, n);
	va_end(args);

	return n;
}

void uart_comm_init(void)
{
	uart_init(UART_PORT, UART_BAUD_RATE_115200, msg_dispatch);

	print(LOG_INFO, MODULE "uart init ok\r\n");

	return;
}
