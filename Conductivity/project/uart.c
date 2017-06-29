#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

#define RX_SIZE 256
#define BUF_SIZE 20
#define SYSERR -1
#define OK 	0

int init(void);
int transmit(int,char*,char*);
void* receive(void*);

pthread_mutex_t count_mutex;

int init() 
{
	int uart0_filestream = -1;
	uart0_filestream = open("/dev/ttyS0", O_RDWR | O_NOCTTY | O_NDELAY);
	
	if (uart0_filestream == -1)
	{	
		printf("Error: Unable to open UART\n");
		return SYSERR;
	}
	else
	{	
		printf("Successfully opened UART port\n");

		struct termios options;

		tcgetattr(uart0_filestream, &options);

		options.c_cflag = B9600 | CS8 | CLOCAL | CREAD;
		options.c_iflag = IGNPAR;
		options.c_oflag = 0;
		options.c_lflag = 0;

		tcflush(uart0_filestream, TCIFLUSH);
		tcsetattr(uart0_filestream, TCSANOW, &options);
		return uart0_filestream;
	}
}

int transmit(int serial, char* buffer_base, char* p_buffer)
{
	int count = write(serial, buffer_base, (p_buffer - buffer_base));
			
	if(count < 0)
	{
		printf("Transmit Error\n");
		return SYSERR;
	}

	return OK;	
}

void* receive(void* arg)
{
	int serial = *((int*)arg);
	free(arg);
	
	if(serial != -1)
	{
		unsigned char rx_buffer[RX_SIZE];
		int i;
		
		for(i = 0; i < RX_SIZE; i++)
			rx_buffer[i] = 0;

		int rx_length = 0;

		while(1)
		{
			rx_length = read(serial, (void*) rx_buffer, RX_SIZE - 1);

			if(rx_length > 0)
			{	
				rx_buffer[rx_length] = '\0';
				pthread_mutex_lock(&count_mutex);
					printf("%s", rx_buffer);
				
					if(rx_buffer[rx_length-1] == 0x0D)
						printf("\nCommand:");
			
					fflush(stdout);

				pthread_mutex_unlock(&count_mutex);

				bzero(&rx_buffer, RX_SIZE);
			}	
		}
	}	
}

int main()
{
	int i, serial = init();

	int *arg = malloc(sizeof(*arg));
	*arg = serial;	
	/* Thread Pool */	

	pthread_t recv_t, tran_t;	

	/* Thread setup  */
	
	if(pthread_create(&recv_t, NULL, receive,(void*)arg) != 0)
	{
		printf("Failed to create thread");
		return -1;
	}	

	unsigned char tx_buffer[BUF_SIZE], c;
	unsigned char *p_tx_buffer_cur, *p_tx_buffer_base;
		
	for(i = 0; i < BUF_SIZE; i++)
		tx_buffer[i] = 0;

	p_tx_buffer_base = &tx_buffer[0];
	p_tx_buffer_cur = p_tx_buffer_base;

	printf("Command:");
		
	while((c = getchar()) != 'q')
	{
		if((c != 0x00))
		{	
			if(c != '\n')
				*p_tx_buffer_cur++ = c;
				
			if(c == '\n' && (p_tx_buffer_cur != p_tx_buffer_base))
			{
				*p_tx_buffer_cur++ = 0x0D;		
				
				transmit(serial, p_tx_buffer_base, p_tx_buffer_cur);
				bzero(p_tx_buffer_base, (p_tx_buffer_cur - p_tx_buffer_base));
				p_tx_buffer_cur = &tx_buffer[0];
			}	
		}
	}
	printf("Closing serial...");
	close(serial);
	printf("closed\n");

	return OK;
}
