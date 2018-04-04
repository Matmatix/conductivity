#include "ubidots.h"


int main() {
	UbidotsClient *client = ubidots_init("3d08eb13f058278570b22e031547f9d03134a814");

	while(1){
		double value = 4.00;
		ubidots_save_value(client, "593ec0e876254251732ac87d", value, TIMESTAMP_NOW);
	}
	
	ubidots_cleanup(client);

	return 0;
		
}
