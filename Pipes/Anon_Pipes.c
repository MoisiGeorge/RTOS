#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <process.h>
#include <inttypes.h>
#include <sys/neutrino.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/syspage.h>


typedef enum{temp, presure, speed, acceleration, weight, distance} dataTypes;

typedef enum{busy = 1, ready, done} commStates;

typedef struct univDataUnit{
	int sensID;
	float value;
	int type; 
	uint64_t time; // time = clock(); in clock ticks / CLOCKS_PER_SEC => secunde
}uData;			 //	Nu functioneaza din cauza ca Read() & Write() sunt blocante in lipsa datelor in pipe. 
					//  ClockCycles( )  functioneaza totusi, ele fiind numarul de tick-uri al sistemului de operare.

float genData(dataTypes type){
	switch(type){
	case temp:
		return (((rand() % 3000) / 30.1) * (-1 + 2*(rand() % 2))); // garde C
		break;
	case presure:
		return ((rand() % 300) / 30.1); // bari
		break;
	case speed:
		return (((rand() % 1500) / 30.1) * (-1 + 2*(rand() % 2))); // m/s
		break;
	case acceleration:
		return (((rand() % 900) / 30.1) * (-1 + 2*(rand() % 2))); // m/s^2
		break;
	case weight:
		return ((rand() % 6000) / 30.1); // kg
		break;
	case distance:
		return ((rand() % 60000) / 30.1); // m
		break;
	}
}

void getType(dataTypes type, char* buff){
	
	switch(type){
	case temp:
		strcpy(buff, "Temperature");
		break;
	case presure:
		strcpy(buff, "Presure");
		break;
	case speed:
		strcpy(buff, "Speed");
		break;
	case acceleration:
		strcpy(buff, "Acceleration");
		break;
	case weight:
		strcpy(buff, "Weight");
		break;
	case distance:
		strcpy(buff, "Distance");
		break;
	}
}

int procData(int ID, int commPipes[][2], int commNum, int* dataPipe){
	
	int state;
	uData data[2];
	dataTypes type;
	char typeName[20];
	uint64_t cps = SYSPAGE_ENTRY(qtime)->cycles_per_sec;
	int i;
	
	if(ID >= commNum){
		state = done;
		for(i = 0; i < commNum; i++){
			write(commPipes[i][1], &state, sizeof(commStates));
		}
		perror( "CommPipe number out of range\n" );
		return 0;
	}
	state = ready;
	for(i = 0; i < 2; i++){
		write(commPipes[ID][1], &state, sizeof(commStates) );
		read(dataPipe[0], &data[i], sizeof(uData) );
		printf("Acquired Data instance %d: \n", i + 1);
		printf("Sensor ID: %d\n", data[i].sensID);
		getType(data[i].type, typeName);
		printf("%s value: %f\n", typeName, data[i].value);
	
	}
	printf("Calculated results: \n");
	printf("%s difference: %f\n",typeName ,data[0].value - data[1].value);
	printf("Time between measurements: %f sec\n", (float) (data[1].time - data[0].time) / cps );
	printf("Time derivative: %f \n", (float) (data[0].value - data[1].value) / ( (float) (data[1].time - data[0].time) / cps ) );
	printf("Calculation End\n");
	return 1;
}

int main(int argc, char* argv[]){
	
	pid_t pid1;
	pid_t pid2;
	pid_t pid3;
	int tmp_state = busy;
	int dataPipe[2];
	int commPipes[3][2];
	uint64_t cps = SYSPAGE_ENTRY(qtime)->cycles_per_sec;
	
	printf( "This system has %lld cycles/sec.\n",cps );
	
	if(pipe(dataPipe) != 0){
		 perror( "Coud not open dataPipe\n" );
		 return EXIT_FAILURE;
	}
	if(pipe(commPipes[0]) != 0){
		 perror( "Coud not open commPipe - 1\n" );
		 return EXIT_FAILURE;
	}
	if(pipe(commPipes[1]) != 0){
		 perror( "Coud not open commPipe - 2\n" );
		 return EXIT_FAILURE;
	}
	if(pipe(commPipes[2]) != 0){
		 perror( "Coud not open commPipe - 3\n" );
		 return EXIT_FAILURE;
	}
	printf("Pipe1: %d - %d \nPipe2: %d - %d \nPipe3: %d - %d \n", commPipes[0][0], commPipes[0][1], commPipes[1][0], commPipes[1][1], commPipes[2][0], commPipes[2][1]);
	
	write(commPipes[0][1], &tmp_state, sizeof(commStates));
	write(commPipes[1][1], &tmp_state, sizeof(commStates));
	write(commPipes[2][1], &tmp_state, sizeof(commStates));
	
	 if( ( pid1 = fork() ) == -1 ) {
       perror( "fork-pid1" );
       return EXIT_FAILURE;
    }
    if( pid1 != 0 ){
    	if( ( pid2 = fork() ) == -1) {
       perror( "fork-pid2" );
       return EXIT_FAILURE;
    	}
    }
    if(pid1 != 0 && pid2 != 0){
    	if( ( pid3 = fork() ) == -1) {
       perror( "fork-pid3" );
       return EXIT_FAILURE;
    	}
    }
    
    if( pid1 == 0){ 										//Sensor 1 process
    	int state;
    	int size_read;
    	int ID = 0;
    	uData data;
    	int type = temp;
    	uint64_t liveTime = ClockCycles();
    	
    	while(1){
    		if( (size_read = read(commPipes[ID][0], &state, sizeof(commStates))) !=0 ){
    		
    			printf("Temp Size Read: %d\nStatus Sensor ID-%d: %d\n", size_read, ID, state);
    			switch(state){
    			case busy:
    				sleep(1);
    				break;
    			case ready:
    				data.sensID = ID;
    				data.type = type;
    				data.value = genData(type);
    				data.time = ClockCycles();
    				write(dataPipe[1], &data, sizeof(uData));
    				break;
    			case done:
    				printf("From: %d -- Exiting Sensor %d -- LiveTime: %f\n", getpid(), ID +1, ( (float)( ClockCycles() - liveTime ) / cps ) );
    				exit( EXIT_SUCCESS );
    			}
    		}
    	}

    }else if( pid2 == 0){ 										//Sensor 2 process
    	int state;
    	int size_read;
    	int ID = 1;
    	uData data;
    	int type = distance;
    	uint64_t liveTime = ClockCycles();
    	
    	while(1){
    		if( (size_read = read(commPipes[ID][0], &state, sizeof(commStates))) !=0 ){
    		
    			printf("Distance Size Read: %d\nStatus Sensor ID-%d: %d\n", size_read, ID ,state);
    			switch(state){
    			case busy:
    				sleep(1);
    				break;
    			case ready:
    				data.sensID = ID;
    				data.type = type;
    				data.value = genData(type);
    				data.time = ClockCycles();
    				write(dataPipe[1], &data, sizeof(uData));
    				break;
    			case done:
    				printf("From: %d -- Exiting Sensor %d -- LiveTime: %f\n", getpid(), ID +1, ( (float)( ClockCycles() - liveTime ) / cps ) );
    				exit( EXIT_SUCCESS );
    			}
    		}
    	}
    
    }else if( pid3 == 0){ 										//Sensor 3 process
   	 	int state;
    	int size_read;
    	int ID = 2;
    	uData data;
    	int type = speed;
    	uint64_t liveTime = ClockCycles();
  
    	while(1){
    		if( (size_read = read(commPipes[ID][0], &state, sizeof(commStates))) !=0 ){
    			
    			printf("Speed Size Read: %d\nStatus Sensor ID-%d: %d\n", size_read, ID, state);
    			switch(state){
    			case busy:
    				sleep(1);
    				break;
    			case ready:
    				data.sensID = ID;
    				data.type = type;
    				data.value = genData(type);
    				data.time = ClockCycles();
    				write(dataPipe[1], &data, sizeof(uData));
    				break;
    			case done:
    				printf("From: %d -- Exiting Sensor %d -- LiveTime: %f\n", getpid(), ID +1, ( (float)( ClockCycles() - liveTime ) / cps ) );
    				exit( EXIT_SUCCESS );
    			}
    		}
    	}

    }else{ 															//Continuare process parinte
    	int state;
    	int interf = 1;
    	char option;
    	state = done;
    	sleep(2);
    	
    	printf("\n\nDone creating sensor forks\nPlease follown instructions\n\n");
    	printf("Interface Options:\n Press: 1  for Temperature Sensor \n Press: 2  Distance Sensor\n Press: 3  for Speed Sensor");
    	printf("\n Press: 0  to Exit\n");
    	while(interf){
    		printf(" => ");
    		scanf(" %c", &option);
    		printf("Optiunea: %c\n\n", option);
    		switch(option){
    			case '0':
    				interf = 0;
    				break;
    			case '1':
    				procData(0, commPipes, 3, dataPipe);
    				sleep(1);
    				break;
    			case '2':
    				procData(1, commPipes, 3, dataPipe);
    				break;
    			case '3':
    				procData(2, commPipes, 3, dataPipe);
    				break;
    			default:
    				interf = 0;
    				break;
    		}
    		
    	}
    	
    	sleep(5);
    	write(commPipes[0][1], &state, sizeof(commStates));
		write(commPipes[1][1], &state, sizeof(commStates));
		write(commPipes[2][1], &state, sizeof(commStates));
    	while(waitpid(-1, NULL, 0) > 0);
    	printf("From: %d -- All children Sensors have exited\n", getpid());
    }
	
	return 0;
}