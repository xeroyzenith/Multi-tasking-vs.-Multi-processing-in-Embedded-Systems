// Copyright by Adam Kinsman, Henry Ko and Nicola Nicolici
// Developed for the Embedded Systems course (COE4DS4)
// Department of Electrical and Computer Engineering
// McMaster University
// Ontario, Canada

#include <stdio.h>
#include <string.h>
#include "sys/alt_alarm.h"
#include "alt_types.h"
#include "sys/alt_irq.h"
#include "system.h"
#include "nios2.h"
#include "io.h"
#include "altera_avalon_mutex.h"
#include "altera_avalon_performance_counter.h"

#define NOTHING 14
#define COMPLETE 13
#define PRINT_VALUES 12
#define RECV_VALUES_CPU2 11
#define RECV_VALUES_CPU1 10
#define CALC_VALUES 9
#define SEND_VALUES_CPU2_2 8
#define SEND_VALUES_CPU2 7
#define SEND_VALUES_CPU1_2 6
#define SEND_VALUES_CPU1 5
#define PB_MESSAGE_CPU2 4
#define PB_MESSAGE_CPU1 3
#define MESSAGE_WAITING_LCD 2
#define MESSAGE_WAITING_UART 1
#define NO_MESSAGE 0

#define LOCK_SUCCESS 0
#define LOCK_FAIL 1

#define MESSAGE_BUFFER_BASE MESSAGE_BUFFER_RAM_BASE

#define MS_DELAY 1000

// Message buffer structure
typedef struct {
	char flag;
	//int array_A[16][32];
	//int array_B[1][32];
	// the buffer can hold up to 4KBytes data
	int buf[1024];
} message_buffer_struct;

int data_array1[32][32];
int data_array2[32][32];
int array_C[32][32];
int state_flag = 0;
int line;
int time[6];

// For performance counter
void *performance_name = CPU1_PERFORMANCE_COUNTER_BASE;

// Global functions
void handle_cpu1_button_interrupts() {
	int i = 0, j = 0, neg = 1;
	int switches;

	switch(IORD(CPU1_PB_BUTTON_I_BASE, 3)) { // PB 2 and 3
		case 1:
			//PERF_RESET(performance_name);

			// Start the performance counter
			//PERF_START_MEASURING(performance_name);

			switches = IORD(CPU1_SWITCH_I_BASE, 0);

			srand(switches);

			printf("CPU1 PB0 pressed\n");
			for (i = 0; i < 32; i++){
				for (j = 0; j < 32; j++){
					/*neg = rand()%2;
					if (neg == 0) neg = -1;
					else neg = 1;*/
					if(rand()%2) neg = -1;
					else neg = 1;
					data_array1[i][j] = (rand() % 10000) * neg;
					data_array2[i][j] = (rand() % 400) * neg;
					//printf("%d\t", data_array1[i][j]);
				}
				//printf("\n");
			}
			state_flag = 1;
			printf("Random Array Generation completed.\n");
			/*for (i = 0; i < 32; i++){
				for (j = 0; j < 32; j++){
					printf("%d\t", data_array2[i][j]);
				}
				printf("\n");
			}*/
			//PERF_STOP_MEASURING(performance_name);

			//time[0] = perf_get_section_time(performance_name,0);
			break;
		case 2:
			//PERF_RESET(performance_name);

			// Start the performance counter
			//PERF_START_MEASURING(performance_name);

			switches = IORD(CPU1_SWITCH_I_BASE, 0);

			srand(~switches);

			printf("CPU1 PB1 pressed\n");
			for (i = 0; i < 32; i++){
				for (j = 0; j < 32; j++){
					/*neg = rand()%2;
					if (neg == 0) neg = -1;
					else neg = 1;*/
					if(rand()%2) neg = -1;
					else neg = 1;
					data_array1[i][j] = (rand() % 10000) * neg;
					data_array2[i][j] = (rand() % 400) * neg;
					//printf("%d\t", data_array1[i][j]);
				}
				//printf("\n");
			}
			state_flag = 1;
			printf("Random Array Generation completed.\n");
			/*for (i = 0; i < 32; i++){
				for (j = 0; j < 32; j++){
					printf("%d\t", data_array2[i][j]);
				}
				printf("\n");
			}*/
			//PERF_STOP_MEASURING(performance_name);

			//time[0] = perf_get_section_time(performance_name,0);
			break;
	}
	IOWR(CPU1_PB_BUTTON_I_BASE, 3, 0x0);
}

// The main function
int main() {
	// Pointer to our mutex device
	alt_mutex_dev* mutex = NULL;

	// Local variables
	unsigned int id;
	unsigned int value;
	unsigned int count = 0;
	unsigned int ticks_at_last_message;
	int switches;

	int i = 0, j = 0, k = 0;
	char *token;
	int ind = 0;
	int temp = 0;

	message_buffer_struct *message;

	// For performance counter
	PERF_RESET(CPU1_PERFORMANCE_COUNTER_BASE);

	// Reading switches 15-8 for CPU1
	

	// Enable all button interrupts
	IOWR(CPU1_PB_BUTTON_I_BASE, 2, 0x3);
	IOWR(CPU1_PB_BUTTON_I_BASE, 3, 0x0);
	alt_irq_register(CPU1_PB_BUTTON_I_IRQ, NULL, (void*)handle_cpu1_button_interrupts );
  
	// Get our processor ID (add 1 so it matches the cpu name in SOPC Builder)
	NIOS2_READ_CPUID(id);

	printf("COE4DS4 Winter15\n");
	printf("Lab7     exp.  3\n\n");

	// Value can be any non-zero value
	value = 1;

	// Initialize the message buffer location
	message = (message_buffer_struct*)MESSAGE_BUFFER_BASE;

	// Okay, now we'll open the real mutex
	// It's not actually a mutex to share the jtag_uart, but to share a message
	// buffer which CPU1 is responsible for reading and printing to the jtag_uart.
	mutex = altera_avalon_mutex_open(MESSAGE_BUFFER_MUTEX_NAME);

	// We'll use the system clock to keep track of our delay time.
	// Here we initialize delay tracking variable.
	ticks_at_last_message = alt_nticks();

	if (mutex) {
		message->flag = NO_MESSAGE;
		//if (state_flag == 1) message->flag = SEND_VALUES;
		
		while(1) {

			if (state_flag == 1){
				state_flag = 2;
				message->flag = PB_MESSAGE_CPU1;
			}

			switches = IORD(CPU1_SWITCH_I_BASE, 0);

			//srand(switches);

			// See if it's time to send a message yet
			if (alt_nticks() >= (ticks_at_last_message + ((alt_ticks_per_second() * (MS_DELAY)) / 1000))) {
				ticks_at_last_message = alt_nticks();

				// Try and aquire the mutex (non-blocking).
				if(altera_avalon_mutex_trylock(mutex, value) == LOCK_SUCCESS) {
					// Check if the message buffer is empty
					if(message->flag == NO_MESSAGE) {
						/*count++;

						// Send the message to the buffer						
						sprintf(message->buf, "Mesg from CPU %d.Number sent: %d  \n", id, count);

						// Set the flag that a message has been put in the buffer
						// And this message should be displayed on LCD connected to CPU2
						if(switches){
							message->flag = MESSAGE_WAITING_UART;
						}else{
							message->flag = MESSAGE_WAITING_LCD;
						}*/

					}else if(message->flag == PB_MESSAGE_CPU1) {
						//printf("%s", message->buf);
						/*if ((strcmp(message->buf, "CPU2 Requesting Data.\n") == 0) && (state_flag == 2)){
							printf("%s", message->buf);
							printf("CPU1 Sending Data to CPU2.\n");
							sprintf(message->buf, "CPU1 Sending Data to CPU2.\n");
							//state_flag = 0;
							//PERF_RESET(performance_name);
							// Start the performance counter
							//PERF_START_MEASURING(performance_name);
							message->flag = PB_MESSAGE_CPU2;
						}*/
						if ((message->buf[0] == -999999) && (state_flag == 2)){
							printf("%s", message->buf);
							printf("CPU1 Sending Data to CPU2.\n");
							//sprintf(message->buf, "CPU1 Sending Data to CPU2.\n");
							message->buf[0] = -999998;
							//state_flag = 0;
							//PERF_RESET(performance_name);
							// Start the performance counter
							//PERF_START_MEASURING(performance_name);
							message->flag = PB_MESSAGE_CPU2;
						}else{
							if (state_flag == 2){
								//printf("CPU1 Sending Data to CPU2.\n");
								//sprintf(message->buf, "CPU1 Sending Data to CPU2.\n");
								//state_flag = 0;
								//printf("Got in here");
								//sprintf(message->buf, "\n");
								message->flag = PB_MESSAGE_CPU2;
							}else{
								printf("%s", message->buf);
								//sprintf(message->buf, "Access Denied.\n");
								message->flag = PB_MESSAGE_CPU2;
							}
						}
						/*if ((strcmp(message->buf, "CPU2 Requesting Data.\n") == 0) && (state_flag == 1)){
							printf("CPU1 Sending Data to CPU2.\n");
							state_flag = 0;
							sprintf(message->buf, "CPU2 Approved Data request.\n");
							//message->flag = SEND_VALUES;
						}else{
							printf("Random arrays need to be generated before calculations can commence.\n");
						}
						message->flag = PB_MESSAGE_CPU2;*/
					}else if(message->flag == SEND_VALUES_CPU1){
						//if (strcmp(message->buf, "CPU2 Requesting Array B.\n") == 0){
						//	message->flag = SEND_VALUES_2;
						//}else{
						printf("Sending rows 0 to 16 of Array A to CPU2.\n");
						ind = 0;
						for (i = 0; i < 16; i++){
							for(j=0;j<32;j++){
								message->buf[ind] = data_array1[i][j];
								ind++;
							}
						}
						printf("Sent rows 0 to 16 of Array A to CPU2.\n");
						//}
						message->flag = SEND_VALUES_CPU2;
					}else if (message->flag == SEND_VALUES_CPU1_2){
						printf("Sending rows 0 to 32 of Array B to CPU2.\n");
						ind = 0;
						for (i = 0; i < 32; i++){
							for(j=0;j<32;j++){
								message->buf[ind] = data_array2[i][j];
								ind++;
							}
						}
						//PERF_STOP_MEASURING(performance_name);
						//time[1] = perf_get_section_time(performance_name,0);
						printf("Sent rows 0 to 32 of Array B to CPU2.\n");
						message->flag = SEND_VALUES_CPU2_2;
					}else if(message->flag == CALC_VALUES){
						printf("Calculating CPU1 Half!\n");
						//PERF_RESET(performance_name);
						// Start the performance counter
						//PERF_START_MEASURING(performance_name);
						for (i = 16; i < 32; i++){
							for (j = 0; j < 32; j++){
								temp = 0;
								for (k = 0; k < 32; k++){
									 temp += data_array1[i][k]*data_array2[k][j];
								}
								array_C[i][j] = temp;
							}
						}
						//PERF_STOP_MEASURING(performance_name);
						//time[2] = perf_get_section_time(performance_name,0);
						printf("CPU1 Half Calculation Complete!\n");
						//message->flag = RECV_VALUES_CPU2;
						/*for (i = 0; i < 16; i++){
							for (j = 0; j < 32; j++){
								array_C[i][j] = data_array1[i][j]*data_array2[i][j];
							}
						}
						i = 16;
						j = 0;
						message->flag = RECV_VALUES;*/
					}else if(message->flag == RECV_VALUES_CPU1){
						printf("CPU1 Receiving other half from CPU2.\n");
						//PERF_RESET(performance_name);
						// Start the performance counter
						//PERF_START_MEASURING(performance_name);
						ind = 0;
						for (i = 0; i < 16; i++){
							for(j = 0;j < 32; j++){
								array_C[i][j] = message->buf[ind];
								ind++;
							}
						}
						printf("CPU1 Received other half from CPU2.\n");
						message->flag = PRINT_VALUES;
						//alt_up_character_lcd_set_cursor_pos(lcd_0, 0, 0);
						//alt_up_character_lcd_string(lcd_0, message->buf);
						/*if(j==32){
							i++;
							j=0;
						}
						printf("Recieving %s from CPU2 at Indexes %d %d.\n", message->buf, i, j);
						token = strtok(message->buf, " ");
						data_array1[i][j] = token;
						token = strtok(message->buf, " ");
						data_array2[i][j] = token;
						j++;
						if((j==32)&&(i==31)){
							message->flag = COMPLETE;
						}*/
					}else if(message->flag == PRINT_VALUES){
						//PERF_STOP_MEASURING(performance_name);
						//time[3] = perf_get_section_time(performance_name,0);

						printf("\nPrinting resulting array:\n\n");
						for (i = 0; i < 32; i++){
							for (j = 0; j < 32; j++){
								printf("%d\t", array_C[i][j]);
							}
							printf("\n");
						}

						//printf("Total Array Calculation time: %d \n", perf_get_section_time(performance_name,0));
						printf("\nTotal Array Calculation time could not be calculated (Issues with performance counter, not our fault)");
						//printf("Total Array Calculation time: %d \n", time[0]+time[1]+time[2]+time[3]);

						message->flag = COMPLETE;
					}else if(message->flag == COMPLETE){
						//printf("CPU's Have Completed Matrix Calculations!\n");
					}else if(message->flag == NOTHING){
						printf("Please Generate Arrays First.\n");
					}else{

					}
					// Release the mutex
					altera_avalon_mutex_unlock(mutex);
				}
			}

			// Check the message buffer
			// and if there's a message waiting with the correct flag, print it to stdout.
			if(message->flag == MESSAGE_WAITING_UART) {
				printf("%s", message->buf);

				// Clear the flag
				message->flag = NO_MESSAGE;

				if (strcmp(message->buf, "CPU2 Requesting Data.\n") == 0){
					printf("CPU1 Sending Data to CPU2.\n");
					i = 16;
					message->flag = SEND_VALUES_CPU1;
				}
			}
		}
	}
	
	return(0);
}
