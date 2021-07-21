#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <process.h>
#include <inttypes.h>
#include <fcntl.h>
#include <sys/neutrino.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/syspage.h>

#define sensNum 3
pid_t pids[sensNum]= {[0 ... sensNum - 1] = 1};


typedef enum{temp, presure, speed, acceleration, weight, distance} dataTypes;

typedef enum{busy = 1, ready, done} commStates;

int sensState = busy;

typedef struct sensMsg{
	dataTypes type;
	pid_t pid;
	int chId;
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

int procData(dataTypes sensType, idMsg* sensList){
	
	int chId;
	pid_t pid;
	int coId;
	uData data[2];
	dataTypes type;
	char typeName[20];
	uint64_t cps = SYSPAGE_ENTRY(qtime)->cycles_per_sec;
	int i;
	
	for(i = 0; i < sensNum; i++){
		if(sensList[i].type == sensType){
			chId = sensList[i].chId;
			pid = sensList[i].pid;
		}
	}
	coId = ConnectAttach(0, pid, chId, 0|_NTO_SIDE_CHANNEL,_NTO_COF_CLOEXEC);
	
	for(i = 0; i < 2; i++){
		sensState = ready;
		MsgSend(coId, &sensState, sizeof(commStates), &data[i], sizeof(uData));
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
	ConnectDetach(coId);
	return 1;
}

void stopParent(idMsg* sensList){
	int i;
	int coId;
	for(i = 0; i < sensNum; i++){
		coId = ConnectAttach(0, sensList[i].pid, sensList[i].chId, 0|_NTO_SIDE_CHANNEL,_NTO_COF_CLOEXEC);
		sensState = done;
		MsgSend(coId, &sensState, sizeof(commStates), NULL, NULL);
		ConnectDetach(coId);
	}	
	while(waitpid(-1, NULL, 0) > 0);
    printf("From: %d -- All children Sensors have exited\n", getpid());
    exit( EXIT_SUCCESS );

}


int main(int argc, char* argv[]){
	int i;
	int tmpComm;
	uint64_t cps = SYSPAGE_ENTRY(qtime)->cycles_per_sec;
	
	printf( "This system has %lld cycles/sec.\n",cps );
	
	tmpComm = ChannelCreate(_NTO_CHF_UNBLOCK);
	
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
    		int tmpCoId;
    		int chId;
    		int resId;
    		idMsg sensId;
    		uint64_t liveTime = ClockCycles();
    		
    		tmpCoId = ConnectAttach(0, getppid(), tmpComm, 0|_NTO_SIDE_CHANNEL,_NTO_COF_CLOEXEC);
    		chId = ChannelCreate(_NTO_CHF_UNBLOCK);
    		
    		sensId.chId = chId;
    		sensId.pid = getpid();
			
			switch(i){
			case 0:								
				type = temp;
				sensId.type = type;
				MsgSend(tmpCoId, &sensId, sizeof(sensId), &sensState, sizeof(commStates));
				sleep(1);
				ConnectDetach(tmpCoId);
      			while(1){
      
    					switch(sensState){
    					case busy:
    						resId = MsgReceive(chId, &sensState, sizeof(commStates), NULL);
    						break;
    					case ready:
    						data.sensID = ID;
    						data.type = type;
    						data.value = genData(type);
    						data.time = ClockCycles();
    						MsgReply(resId,0, &data, sizeof(uData));
    						sensState = busy;
    						break;
    					case done:
    						MsgReply(resId,0, NULL, NULL);
    						sleep(1);
    						ChannelDestroy(chId);
    						printf("From: %d -- Exiting Sensor %d -- LiveTime: %f\n", getpid(), ID +1, ( (float)( ClockCycles() - liveTime ) / cps ) );
    						exit( EXIT_SUCCESS );
    					}
    				}
				break;
			
			case 1:
				type = distance;
				sensId.type = type;
				MsgSend(tmpCoId, &sensId, sizeof(sensId), &sensState, sizeof(commStates));
				sleep(1);
				ConnectDetach(tmpCoId);
    			while(1){
    			
    					switch(sensState){
    					case busy:
    						resId = MsgReceive(chId, &sensState, sizeof(commStates), NULL);
    						break;
    					case ready:
    						data.sensID = ID;
    						data.type = type;
    						data.value = genData(type);
    						data.time = ClockCycles();
    						MsgReply(resId,0, &data, sizeof(uData));
    						sensState = busy;
    						break;
    					case done:
    						MsgReply(resId,0, NULL, NULL);
    						sleep(1);
    						ChannelDestroy(chId);
    						printf("From: %d -- Exiting Sensor %d -- LiveTime: %f\n", getpid(), ID +1, ( (float)( ClockCycles() - liveTime ) / cps ) );
    						exit( EXIT_SUCCESS );
    					}
    				}
				break;
				
			case 2:
    			type = speed;
    			sensId.type = type;
				MsgSend(tmpCoId, &sensId, sizeof(sensId), &sensState, sizeof(commStates));
				sleep(1);
				ConnectDetach(tmpCoId);
    			while(1){
    			
    					switch(sensState){
    					case busy:
    						resId = MsgReceive(chId, &sensState, sizeof(commStates), NULL);
    						break;
    					case ready:
    						data.sensID = ID;
    						data.type = type;
    						data.value = genData(type);
    						data.time = ClockCycles();
    						MsgReply(resId,0, &data, sizeof(uData));
    						sensState = busy;
    						break;
    					case done:
    						MsgReply(resId,0, NULL, NULL);
    						sleep(1);
    						ChannelDestroy(chId);
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
    	for(i = 0; i < sensNum; i++){
    		resId = MsgReceive(tmpComm, &sensList[i], sizeof(idMsg), NULL);
    		sensState = busy;
    		MsgReply(resId, 0, &sensState, sizeof(commStates));
    	}
    	printf("\n\nDone creating sensor forks\nPlease follown instructions\n\n");
    	printf("Interface Options:\n Press: 1  for Temperature Sensor \n Press: 2  Distance Sensor\n Press: 3  for Speed Sensor");
    	printf("\n Press: 0  to Exit\n");
    	
    	while(1){
    		printf(" => ");
    		scanf(" %c", &option);
    		printf("Optiunea: %c\n\n", option);
    		switch(option){
    			case '0':
    				stopParent(sensList);
    				break;
    			case '1':
    				procData(temp, sensList);
    				sleep(1);
    				break;
    			case '2':
    				procData(distance, sensList);
    				break;
    			case '3':
    				procData(speed, sensList);
    				break;
    			default:
    				stopParent(sensList);
    				break;
    		}
    		
    	}
    }
	return 0;
}