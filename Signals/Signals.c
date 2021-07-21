#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <process.h>
#include <inttypes.h>
#include <sys/neutrino.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/syspage.h>

#define sensNum 3
pid_t pids[sensNum]= {[0 ... sensNum - 1] = 1};


typedef enum{temp, presure, speed, acceleration, weight, distance} dataTypes;

typedef enum{busy = SIGUSR1, ready = SIGUSR2, done=SIGQUIT} commStates;

int sensState = busy;

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

int procData(int ID, int* dataPipe){
	
	int state;
	uData data[2];
	dataTypes type;
	char typeName[20];
	uint64_t cps = SYSPAGE_ENTRY(qtime)->cycles_per_sec;
	int i;
	
	if(ID >= sensNum){
		perror("Wrong ID!");
		raise(done);
	}
	
	for(i = 0; i < 2; i++){
		kill(pids[ID], ready);
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

void stopParent(){
	int i;
	for(i = 0; i < sensNum; i++){
		kill(pids[i], done);
	}	
	
	while(waitpid(-1, NULL, 0) > 0);
    printf("From: %d -- All children Sensors have exited\n", getpid());
    exit( EXIT_SUCCESS );

}

void setState(int sigNum){
	sensState = sigNum;
}


int main(int argc, char* argv[]){
	int i;
	int dataPipe[2];
	uint64_t cps = SYSPAGE_ENTRY(qtime)->cycles_per_sec;
	
	printf( "This system has %lld cycles/sec.\n",cps );
	
	if(pipe(dataPipe) != 0){
		 perror( "Coud not open dataPipe\n" );
		 exit( EXIT_FAILURE );
	}
	
	for(i = 0; i < sensNum; i++){
		
		if( (pids[i] = fork()) == -1){
		 	perror( "Coud not open child process\n");
		 	exit( EXIT_FAILURE );
		}
		printf("Pid val: %d\n", pids[i]);
		
		if(pids[i] == 0){
			break;
		}
	}
	sleep(1);
	
	for(i = 0; i < sensNum; i++){
		
		if(pids[i] == 0){
    		int size_read;
    		int ID = i;
    		uData data;
    		int type;
    		uint64_t liveTime = ClockCycles();
    		
    		signal(busy, setState);
    		signal(ready, setState);
    		signal(done, setState);
			switch(i){
			
			case 0:								
				type = temp;
      			while(1){
      				pause();
      				
    					switch(sensState){
    					case busy:
    						pause();
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
				break;
			
			case 1:
				type = distance;
    			while(1){
    				pause();
    					switch(sensState){
    					case busy:
    						pause();
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
				break;
				
			case 2:
    			type = speed;
    			while(1){
    				pause();
    					switch(sensState){
    					case busy:
    						pause();
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
				break;
			}
		}
	}


    if(1){ 										//Continuare process parinte
    	int state;
    	char option;
    	
    	signal(done, stopParent);
    	
    	printf("\n\nDone creating sensor forks\nPlease follown instructions\n\n");
    	printf("Interface Options:\n Press: 1  for Temperature Sensor \n Press: 2  Distance Sensor\n Press: 3  for Speed Sensor");
    	printf("\n Press: 0  to Exit\n");
    	
    	while(1){
    		printf(" => ");
    		scanf(" %c", &option);
    		printf("Optiunea: %c\n\n", option);
    		switch(option){
    			case '0':
    				raise(done);
    				break;
    			case '1':
    				procData(0, dataPipe);
    				sleep(1);
    				break;
    			case '2':
    				procData(1, dataPipe);
    				break;
    			case '3':
    				procData(2, dataPipe);
    				break;
    			default:
    				raise(done);
    				break;
    		}
    		
    	}
	}
	return 0;
}