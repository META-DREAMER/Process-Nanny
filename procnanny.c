#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "memwatch.h"
const int buf = 255;
const int MAXPROC = 256;
const int maxlines = 4096;
const char pathVar[] = "PROCNANNYLOGS";
const char log_start[]= "[%s] Info: Parent process is PID %s."; //logType 0
const char log_notFound[]= "[%s] Info: No '%s' processes found."; //logType 1
const char log_initMon[]= "[%s] Info: Initializing monitoring of process '%s' (PID %s)."; //logType 2
const char log_killedProc[]= "[%s] Action: PID %s (%s) killed after exceeding %s seconds."; //logType 3
const char log_exit[]= "[%s] Info: Exiting. %s process(es) killed."; //logType 4
const char log_killedOld[]= "[%s] Action: Killed previous instance of Procnanny (PID %s)."; //logType 5

time_t curtime;
int logExists = 0;
time_t time(time_t *t);
char *ctime(const time_t *timer);
pid_t getpid(void);
pid_t fork(void);
unsigned int sleep(unsigned int seconds);
pid_t waitpid(pid_t pid, int *status, int options); 


void logMessage(int logType, char *argv[]) {
	//get path set in environment variable
	char *path;
	if((path = getenv(pathVar)) == NULL){
		printf("PROCNANNYLOGS environment var not set\n");
		exit(1);
	}

	FILE *output;
	output = fopen(path, "a"); // open file in append mode

	if (!logExists) {
		output = fopen(path, "w");
		logExists = 1;
	}
	else		
		output = fopen(path, "a");
		
	if(output == NULL) {
		if (logExists)
			logExists = 0;
		return;
	}

	char message[1024] = {0};

	time(&curtime);
	char *pos;
	char *date = ctime(&curtime);
	if ((pos=strchr(date, '\n')) != NULL)
	    *pos = '\0';

	switch (logType) {
		case 0:
			if(argv[0] == NULL){ return;} //exit if not enough args
			sprintf(message, log_start, date, argv[0]);
			break;
		case 1:
			//process not found
			if(argv[0] == NULL){ return;} //exit if not enough args
			sprintf(message, log_notFound,date,argv[0]);
			break;
		case 2:
			//initialize monitor
			if(argv[1] == NULL){ return;} //exit if not enough args
			sprintf(message, log_initMon,date,argv[0], argv[1]);
			break;
		case 3:
			//killed process
			if(argv[2] == NULL){ return;} //exit if not enough args
			sprintf(message, log_killedProc,date,argv[0],argv[1],argv[2]);
			break;
		case 4:
			//exit
			if(argv[0] == NULL){ return;} //exit if not enough args
			sprintf(message, log_exit,date,argv[0]);
			break;
		case 5:
			//exit
			if(argv[0] == NULL){ return;} //exit if not enough args
			sprintf(message, log_killedOld,date,argv[0]);
			break;
		default:
			//invalid log type
			return;
	}

	//write message to log and close file
	fputs(message, output);
	putc('\n', output);
	if(output)
		fclose(output);
	return ;
}

int readFile(char array[][buf], char *file) {
	int i = 0;
	char * line = NULL;
	size_t len = 0;
	ssize_t read;
	FILE *config = fopen(file, "r");

	if (config == NULL) {
		printf("failed to open file\n");
		exit(1);
	}
	while ((read = getline(&line, &len, config)) != -1) {
		strcpy(array[i], line);
		if(array[i][read-1] == '\n') {
			array[i][read-1] = '\0';
		}
		else {
			array[i][read] = '\0';
		}
		i = i + 1;
	}	
	int numLines = i;

	fclose(config);

	return numLines;

}

void killOld () {
	int pidArray[MAXPROC];
	char	pid[100];
	FILE	*fpin;
	int i;
	int k = 0;

	if ((fpin = popen("pidof -x procnanny | tr ' ' '\n'", "r")) == NULL) {
		printf("popen error\n");
		exit(0);
	}
	int end = 0;
	while(!end) {
		if (fgets(pid, maxlines, fpin) == NULL) {
			break;
		}
		else {
			char *pos;
			if ((pos=strchr(pid, '\n')) != NULL)
			    *pos = '\0';

			pidArray[k] = atoi(pid);
			k++;
		}
	}
	if (pclose(fpin) == -1) {
		printf("pclose error\n");
		exit(0);
	}

	for(i = 0; i < k; i++){
		if(pidArray[i] != getpid()){
			kill(pidArray[i], SIGKILL);
			char oldPID[15];
			sprintf(oldPID, "%d",pidArray[i]);
			char *logDataOld[] = {oldPID};
			logMessage(5, logDataOld);
		}
	}

}

int checkRunning (int pidArray[MAXPROC], char pNames[MAXPROC][buf], char pRunning[MAXPROC][buf], int numPrograms) {
	char	pid[100];
	FILE	*fpin;
	int i;
	int k = 0;
	for (i = 1; i <= numPrograms; i++)
	{

		char grep[512];
		// sprintf(grep, "ps aux | grep '\\b%s\\b' | grep -v grep | awk '{print $2}'",pNames[i]);
		sprintf(grep, "pidof -x %s | tr ' ' '\n'",pNames[i]);

		if ((fpin = popen(grep, "r")) == NULL) {
			printf("popen error\n");
			exit(0);
		}
		int pExists = 0;
		int end = 0;
		while(!end) {
			if (fgets(pid, maxlines, fpin) == NULL) {
				if(!pExists) {
					char *logData[] = {pNames[i]};
					logMessage(1, logData);
				}
				break;
			}
			else {
				time(&curtime);
				char *pos;
				if ((pos=strchr(pid, '\n')) != NULL)
				    *pos = '\0';

				pidArray[k] = atoi(pid);
				strcpy(pRunning[k], pNames[i]);
				k++;
				pExists = 1;
			}
		}
		if (pclose(fpin) == -1) {
			printf("pclose error\n");
			exit(0);
		}

	}
	return k;
}




int main( int argc, char *argv[] )  
{
	if( argc == 2 )
	{	
		char myPid[15];
		sprintf(myPid, "%d",getpid());
		char *logDataStart[] = {myPid};
		logMessage(0, logDataStart);
		killOld();
		int runTime;
		int i;
		int numKilled = 0;
		char pNames[256][255] = {{0}};
		char pRunning[256][255] = {{0}};
		int pidArray[MAXPROC];
		memset(pidArray, 0, MAXPROC);

		int numPrograms = readFile(pNames, argv[1]) - 1;

		runTime = atoi(pNames[0]);

		char runTimeString[15];
		sprintf(runTimeString, "%d",runTime);


		
		int numRunning = checkRunning(pidArray, pNames, pRunning, numPrograms);

		for(i = 0; i < numRunning; i++) {
			pid_t pID = fork();

			if (pID == 0){
				char pidString[15];

				sprintf(pidString, "%d",pidArray[i]);
				char *logData1[] = {pRunning[i], pidString};
				logMessage(2, logData1);
				sleep(runTime);

				if(kill(pidArray[i], SIGKILL) == 0) {
					char *logData2[] = {pidString, pRunning[i], runTimeString};
					logMessage(3, logData2);
					fflush(stdout);
					exit(0);
				}
				else{
					exit(-1);
				}
			}
			else if (pID < 0){
				printf("Fork Failed\n");
				exit(1);
			}
			else{

			}
		}

		for (i = 0; i < numRunning; i++){
			int childStatus;
			waitpid(-1,&childStatus,0);
			if (WEXITSTATUS(childStatus) == 0) {
				numKilled = numKilled + 1;
			}
		}
		

		char numKilledString[5];

		sprintf(numKilledString, "%d",numKilled);
		char *logDataEnd[] = {numKilledString};
		logMessage(4, logDataEnd);
		fflush(stdout);
	}
	else{
		printf("One argument expected.\n");
	}

	fflush(stdout);
	return 0;
}