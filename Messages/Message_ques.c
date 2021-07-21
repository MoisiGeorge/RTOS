#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <process.h>
#include <inttypes.h>
#include <fcntl.h>
#include <mqueue.h>
#include <sys/neutrino.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/syspage.h>

#define sensNum 3
#define DATA_QUEUE_NAME   "/data-queue"
#define QUEUE_PERMISSIONS 0660
#define MAX_MESSAGES 10
#define MAX_MSG_SIZE 256
#define MSG_BUFFER_SIZE MAX_MSG_SIZE + 10
pid_t pids[sensNum]= {[0 ... sensNum - 1] = 1};


typedef enum{temp, presure, speed, acceleration, weight, distance} dataTypes;

typedef enum{busy = 1, ready, done} commStates;

int sensState = busy;

typedef struct sensMsg{
	dataTypes type;
	mqd_t queueId;
}idMsg;

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

int procData(dataTypes sensType, idMsg* sensList, mqd_t dataQueue){
	
	mqd_t sensQueue;
	uData data[2];
	dataTypes type;
	char typeName[20];
	uint64_t cps = SYSPAGE_ENTRY(qtime)->cycles_per_sec;
	int i;
	
	for(i = 0; i < sensNum; i++){
		if(sensList[i].type == sensType){
			sensQueue = sensList[i].queueId;
		}
	}
	
	for(i = 0; i < 2; i++){
		sensState = ready;
		mq_send(sensQueue, (char*) &sensState, sizeof(commStates),NULL);
		mq_receive(dataQueue, (char*) &data[i], sizeof(uData), NULL);
		printf("Acquired Data instance %d: \n", i + 1);
		printf("Sensor ID: %d\n", data[i].sensID + 1);
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

void stopParent(idMsg* sensList){
	int i;
	for(i = 0; i < sensNum; i++){
		sensState = done;
		mq_send(sensList[i].queueId, (char*) &sensState, sizeof(commStates), NULL);
	}	
	while(waitpid(-1, NULL, 0) > 0);
    printf("From: %d -- All children Sensors have exited\n", getpid());
    exit( EXIT_SUCCESS );

}


int main(int argc, char* argv[]){
	int i;
	int tmpComm;
	mqd_t dataQueue;
	struct mq_attr attr;
	uint64_t cps = SYSPAGE_ENTRY(qtime)->cycles_per_sec;
	
	attr.mq_flags = 0;
    attr.mq_maxmsg = MAX_MESSAGES;
    attr.mq_msgsize = MAX_MSG_SIZE;
    attr.mq_curmsgs = 0;
	
	printf( "This system has %lld cycles/sec.\n",cps );

	
	if ((dataQueue = mq_open (DATA_QUEUE_NAME, O_RDONLY | O_CREAT, QUEUE_PERMISSIONS, &attr)) == -1) {
        perror ("Parent: mq_open (client)");
        exit (1);
    }
	printf("MainDataQueue: DQ_ID: %d \n", dataQueue);
	
	if(dataQueue < 0){
		printf("Eroare creare coada mesaje!\n");
		exit( EXIT_FAILURE );
	}
	
	for(i = 0; i < sensNum; i++){
		
		if( (pids[i] = fork()) == -1){
		 	perror( "Coud not open child process\n");
		 	exit( EXIT_FAILURE );
		}
		
		if(pids[i] != 0) printf("Pid val: %d\n", pids[i]);
		
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
    		mqd_t sensQueue;
    		idMsg sensId;
    		uint64_t liveTime = ClockCycles();
			
			if ((dataQueue = mq_open (DATA_QUEUE_NAME, O_WRONLY | O_CREAT, QUEUE_PERMISSIONS, &attr)) == -1) {
				perror ("Parent: mq_open (client)");
				exit (1);
			}
			
    		printf("DataQueue: DQ_ID: %d \n", dataQueue);
    		
			switch(i){
			case 0:								
				type = temp;
				sensId.type = type;
				sensQueue = mq_open("Coada_Senzor_1", O_CREAT|O_RDWR, 0x777, NULL);
				sensId.queueId = sensQueue;
				
				if(sensQueue < 0){
					printf("Eroare creare coada mesaje!\n");
					exit( EXIT_FAILURE );
				}
				mq_send(dataQueue, (char*) &sensId, sizeof(idMsg), NULL);
				printf("Sensor-1: MQ_ID: %d  SensType: %d \n", sensId.queueId, sensId.type);
      			while(1){
      
    					switch(sensState){
    					case busy:
    						mq_receive(sensQueue, (char*) &sensState, sizeof(commStates), NULL);
    						//printf("SensState: %d \n", sensState);
    						break;
    					case ready:
    						data.sensID = ID;
    						data.type = type;
    						data.value = genData(type);
    						data.time = ClockCycles();
    						mq_send(dataQueue, (char*) &data, sizeof(uData), NULL);
    						sensState = busy;
    						break;
    					case done:
    						sleep(1);
    						mq_close(sensQueue);
    						mq_unlink("Coada_Senzor_1");
    						printf("From: %d -- Exiting Sensor %d -- LiveTime: %f\n", getpid(), ID +1, ( (float)( ClockCycles() - liveTime ) / cps ) );
    						exit( EXIT_SUCCESS );
    					}
    				}
				break;
			
			case 1:
				type = distance;
				sensId.type = type;
				sensQueue = mq_open("Coada_Senzor_2", O_CREAT|O_RDWR, 0x777, NULL);
				sensId.queueId = sensQueue;
				
				if(sensQueue < 0){
					printf("Eroare creare coada mesaje!\n");
					exit( EXIT_FAILURE );
				}
				mq_send(dataQueue, (char*) &sensId, sizeof(idMsg), NULL);
    			printf("Sensor-2: MQ_ID: %d  SensType: %d \n", sensId.queueId, sensId.type);
    			while(1){
    			
    					switch(sensState){
    					case busy:
    						mq_receive(sensQueue, (char*) &sensState, sizeof(commStates), NULL);
    						break;
    					case ready:
    						data.sensID = ID;
    						data.type = type;
    						data.value = genData(type);
    						data.time = ClockCycles();
    						mq_send(dataQueue, (char*) &data, sizeof(uData), NULL);
    						sensState = busy;
    						break;
    					case done:
    						sleep(1);
    						mq_close(sensQueue);
    						mq_unlink("Coada_Senzor_2");
    						printf("From: %d -- Exiting Sensor %d -- LiveTime: %f\n", getpid(), ID +1, ( (float)( ClockCycles() - liveTime ) / cps ) );
    						exit( EXIT_SUCCESS );
    					}
    				}
				break;
				
			case 2:
    			type = speed;
    			sensId.type = type;
				sensQueue = mq_open("Coada_Senzor_3", O_CREAT|O_RDWR, 0x777, NULL);
				sensId.queueId = sensQueue;
				printf("Sensor-3: MQ_ID: %d  SensType: %d \n", sensId.queueId, sensId.type);
				if(sensQueue < 0){
					printf("Eroare creare coada mesaje!\n");
					exit( EXIT_FAILURE );
				}
				mq_send(dataQueue, (char*) &sensId, sizeof(idMsg), NULL);
    			
    			while(1){
    			
    					switch(sensState){
    					case busy:
    						mq_receive(sensQueue, (char*) &sensState, sizeof(commStates), NULL);
    						break;
    					case ready:
    						data.sensID = ID;
    						data.type = type;
    						data.value = genData(type);
    						data.time = ClockCycles();
    						mq_send(dataQueue, (char*) &data, sizeof(uData), NULL);
    						sensState = busy;
    						break;
    					case done:
    						sleep(1);
    						mq_close(sensQueue);
    						mq_unlink("Coada_Senzor_3");
    						printf("From: %d -- Exiting Sensor %d -- LiveTime: %f\n", getpid(), ID +1, ( (float)( ClockCycles() - liveTime ) / cps ) );
    						exit( EXIT_SUCCESS );
    					}
    				}
				break;
			}
		}
	}

	if(1){								//Continuare process parinte
    
    	char option;
    	int resId;
    	idMsg sensList[sensNum];
    	sleep(4);
    	for(i = 0; i < sensNum; i++){
    		mq_receive(dataQueue, (char*) &sensList[i], sizeof(idMsg), NULL);
    		printf("MQ_ID: %d  SensType: %d \n", sensList[i].queueId, sensList[i].type);
    	}
    	sleep(1);
    	printf("\n\nDone creating sensor forks\nPlease follown instructions\n\n");
    	printf("Interface Options:\n Press: 1  for Temperature Sensor \n Press: 2  Distance Sensor\n Press: 3  for Speed Sensor");
    	printf("\n Press: 0  to Exit\n");
    	
    	while(1){
    		printf(" => ");
    		scanf(" %c", &option);
    		printf("Optiunea: %c\n\n", option);
    		switch(option){
    			case '0':
    				mq_close(dataQueue);
    				mq_unlink("Coada_Principala");
    				stopParent(sensList);
    				break;
    			case '1':
    				procData(temp, sensList, dataQueue);
    				sleep(1);
    				break;
    			case '2':
    				procData(distance, sensList, dataQueue);
    				break;
    			case '3':
    				procData(speed, sensList, dataQueue);
    				break;
    			default:
    				mq_close(dataQueue);
    				mq_unlink("Coada_Principala");
    				stopParent(sensList);
    				break;
    		}
    		
    	}
    }
	return 0;
}