#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <jansson.h>
#include <time.h>
#include "ubirequest.c"
#include "ubidots.h"

#define RX_SIZE 256
#define BUF_SIZE 20
#define SYSERR -1
#define OK 	0

/* Function declarations */

int init(void);
int transmit(int,char*,char*);
void* receive(void*);

UbidotsCollection* ubidots_collection_init(int);
void ubidots_collection_add(UbidotsCollection*, char*, double);
int ubidots_collection_save(UbidotsClient*, UbidotsCollection*);
void ubidots_collection_cleanup(UbidotsCollection*);
int ubidots_save_value(UbidotsClient*, char*, double, long long);
UbidotsClient* ubidots_init(char*);
UbidotsClient* ubidots_init_with_base_url(char*, char*);
void ubidots_cleanup(UbidotsClient*);

/* Global variables */

pthread_mutex_t count_mutex;
UbidotsClient *client;

/**
 * Instantiate a collection.
 * @arg n  Number of values in this collection.
 * @return A pointer to a collection.
 */
UbidotsCollection* ubidots_collection_init(int n) {
  UbidotsCollection *coll = malloc(sizeof(UbidotsCollection));

  coll->n = n;
  coll->i = 0;
  coll->variable_ids = malloc(sizeof(char*) * n);
  coll->values = malloc(sizeof(float) * n);

  return coll;
}


/**
 * Add a value to a collection.
 * @arg coll         Pointer to the collection made by ubidots_collection_init().
 * @arg variable_id  The ID of the variable this value is associated with.
 * @arg value        The value.
 */
void ubidots_collection_add(UbidotsCollection *coll, char *variable_id, double value) {
  int i = coll->i;

  int len = sizeof(char) * strlen(variable_id);
  coll->variable_ids[i] = malloc(len + 1);
  strcpy(coll->variable_ids[i], variable_id);

  coll->values[i] = value;

  coll->i++;
}


/**
 * Save a collection.
 * @arg coll Collection to save.
 * @reutrn Zero upon success, non-zero upon error.
 */
int ubidots_collection_save(UbidotsClient *client, UbidotsCollection *coll) {
  // Compute URL
  char url[80];

  sprintf(url, "%s/collections/values", client->base_url);

  // Encode JSON Payload
  json_t *j_root = json_array();
  int i, n = coll->n;
  char *json_data;

  for (i = 0; i < n; i++) {
    json_t *j_obj = json_object();
    json_object_set_new(j_obj, "variable", json_string(coll->variable_ids[i]));
    json_object_set_new(j_obj, "value", json_real(coll->values[i]));
    json_array_append_new(j_root, j_obj);
  }

  json_data = json_dumps(j_root, 0);

  // Perform Request
  int rc = ubi_request("POST", url, client->token, json_data, NULL);

  // Cleanup
  json_decref(j_root);
  free(json_data);

  return rc;
}

/**
 * Cleanup a collection when after it is no longer being used.
 * @arg coll Pointer to the collection made by ubidots_collection_init().
 */
void ubidots_collection_cleanup(UbidotsCollection *coll) {
  int i, n = coll->n;

  for (i = 0; i < n; i++) {
    free(coll->variable_ids[i]);
  }

  free(coll->variable_ids);
  free(coll->values);
  free(coll);
}

/**
 * Save a value to Ubidots.
 * @arg client       Pointer to UbidotsClient
 * @arg variable_id  The ID of the variable to save to
 * @arg value        The value to save
 * @arg timestamp    Timestamp (millesconds since epoch). Pass TIMESTAMP_NOW
 *                   to have the timestamp automatically calculated.
 * @return Zero upon success, Non-zero upon error.
 */
int ubidots_save_value(UbidotsClient *client, char *variable_id, double value, long long timestamp) {
  char url[80];
  char json_data[80];

  if (timestamp == TIMESTAMP_NOW)
    timestamp = (long long)time(NULL) * 1000;

  sprintf(url, "%s/variables/%s/values", client->base_url, variable_id);
  sprintf(json_data, "{\"value\": %g, \"timestamp\": %lld}", value, timestamp);

  return ubi_request("POST", url, client->token, json_data, NULL);
}


/**
 * Initialize a Ubidots session. This is most likely the first Ubidots
 * library function you will call.
 * @arg api_key  Your API key for the Ubidots API.
 * @return Upon success, a pointer to a UbidotsClient. Upon error, NULL.
 */
UbidotsClient* ubidots_init(char *api_key) {
  return ubidots_init_with_base_url(api_key, DEFAULT_BASE_URL);
}

UbidotsClient* ubidots_init_with_base_url(char *api_key, char *base_url) {
  // Perform an API request to generate a new token for the given API key.
  char url[80];
  char token_hack[80];
  int rc;
  json_t *j_root, *j_token;

  sprintf(url, "%s/auth/token", base_url);
  sprintf(token_hack, "/%s", api_key);

  rc = ubi_request("POST", url, token_hack, "", &j_root);

  if (rc)
    return NULL;

  j_token = json_object_get(j_root, "token");

  // Allocate and set fields of struct
  UbidotsClient *client = malloc(sizeof(UbidotsClient));

  strncpy(client->base_url, base_url, STRLEN_BASE_URL);
  strncpy(client->api_key, api_key, STRLEN_API_KEY);
  strncpy(client->token, json_string_value(j_token), STRLEN_TOKEN);

  json_decref(j_root);

  return client;
}


/**
 * End a ubidots session. After calling this function with UbidotsClient* client,
 * no more functions may be called with it.
 */
void ubidots_cleanup(UbidotsClient *client) {
  free(client);
}


/* Set up the UART serial port.  Sets it up to be non-blocking at 
a baud rate of 9600 bps */
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

/* Transmit command down serial port using the UART transmission protocol */
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

/* Receives response from device.  If numerical value then it is uploaded to the cloud */
void* receive(void* arg)
{
	int serial = *((int*)arg);
	free(arg);
	
	if(serial != -1)
	{
		unsigned char rx_buffer[RX_SIZE], storage[100];
		int i;
		
		for(i = 0; i < RX_SIZE; i++)
			rx_buffer[i] = 0;

		int sum = 0, rx_length = 0;
		double value = 0;

		while(1)
		{
			rx_length = read(serial, (void*) rx_buffer, RX_SIZE - 1);			

			if(rx_length > 0)
			{
				storage[sum] = '\0';
				sum += rx_length;

				rx_buffer[rx_length] = '\0';
				strcat(storage, rx_buffer);	
				
					if(storage[sum-1] == 0x0D)
					{

						pthread_mutex_lock(&count_mutex);
							storage[sum] = '\0';
							printf("\33[2k\rResponse:%s\nCommand:", storage);
							fflush(stdout);
						
							if(storage[0] != '*')
							{
								value = strtod(storage, NULL);
								printf("\33[2k\rSaving %f to the cloud...", value);
								ubidots_save_value(client, "5942d2ca762542022ae7c5d6", value, TIMESTAMP_NOW);
								printf("done\n");
							}	
							sum = 0;
							bzero(&storage, sum);	
						pthread_mutex_unlock(&count_mutex);
					}	

			}	
		}
	}	
}


/* Main method.  This sets up the Ubidots client, thread pool, and buffers.  */
int main()
{
	int i, serial = init();
	
	/* ----------------------------- */
	printf("Connecting to the cloud...");

	client = ubidots_init("3d08eb13f058278570b22e031547f9d03134a814");
	
	if(client == NULL)
	{
		printf("client = NULL\n");
		return 1;
	}

	printf("connected\n");


	/* ----------------------------- */	

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
