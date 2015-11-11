#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h> 
#include <assert.h>
#include <unistd.h>
#include "memwatch.h"

const int buf = 255;
const int MAXPROC = 128;
const int maxlines = 4096;
const char pathVar[] = "PROCNANNYLOGS";
const char log_start[]= "[%s] Info: Parent process is PID %s."; //logType 0
const char log_notFound[]= "[%s] Info: No '%s' processes found."; //logType 1
const char log_initMon[]= "[%s] Info: Initializing monitoring of process '%s' (PID %s)."; //logType 2
const char log_killedProc[]= "[%s] Action: PID %s (%s) killed after exceeding %s seconds."; //logType 3
const char log_exit[]= "[%s] Info: Exiting. %s process(es) killed."; //logType 4
const char log_killedOld[]= "[%s] Action: Killed previous instance of Procnanny (PID %s)."; //logType 5


typedef struct ConfigProc {
    char *name;
    int runTime;
    int (*pids)[];
} ConfigProc;

typedef struct ChildProc {
    pid_t pID;
    int isFree;
    int monitorTime;
    pid_t watchPID;
    char *watchName;
} ChildProc;

time_t curtime;
int logExists = 0;
int keepRunning = 0;

//FuncDef
time_t time(time_t *t);
char *ctime(const time_t *timer);
pid_t getpid(void);
pid_t fork(void);
unsigned int sleep(unsigned int seconds);
pid_t waitpid(pid_t pid, int *status, int options); 
ConfigProc *createConfigProc(char *name, int runTime, int (*pids)[MAXPROC]);
void destroyConfigProc(ConfigProc *proc);
ChildProc *createChildProc(pid_t pID, int isFree, int monitorTime, pid_t watchPID, char *watchName);
void destroyChildProc(ChildProc *proc);
int isRunning(char* procName);
static void	sig_handler(int);


ConfigProc *createConfigProc(char *name, int runTime, int (*pids)[MAXPROC]) {
    ConfigProc *proc = malloc(sizeof(ConfigProc));
    assert(proc != NULL);

    // int pidList[MAXPROC];

    proc->name = strdup(name);
    proc->runTime = runTime;
    proc->pids = pids;

    return proc;
}

void destroyConfigProc(ConfigProc *proc) {
    assert(proc != NULL);

    free(proc->name);
    free(proc);
    printf("Dstr Config\n");
}

ChildProc *createChildProc(pid_t pID, int isFree, int monitorTime, pid_t watchPID, char *watchName) {
    ChildProc *proc = malloc(sizeof(ChildProc));
    assert(proc != NULL);

    proc->pID = pID;
    proc->isFree = isFree;
	proc->monitorTime = monitorTime;
	proc->watchName = watchName;

    return proc;
}

void destroyChildProc(ChildProc *proc) {
    assert(proc != NULL);
    free(proc->watchName);
    free(proc);
    printf("Dstr Child\n");

}

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

//Returns nu,ber of PID found for a given procName
//Adds the PIDS to the given array
int getPIDs(char* procName, int (*pids)[MAXPROC]) {
	
	FILE	*fpin;
	char cmd[512];
	char	pid[100];
	int k = 0;
	int i;


	for (i = 0; i < MAXPROC; i++) {
	    (*pids)[i] = 0;
	}

	sprintf(cmd, "pidof -x %s | tr ' ' '\n'",procName);
	if ((fpin = popen(cmd, "r")) == NULL) {
		printf("popen error\n");
		exit(0);
	}

	// printf("found pid: %s\n", pid);
	while(fgets(pid, maxlines, fpin) != NULL) {
		char *pos;
		if ((pos=strchr(pid, '\n')) != NULL)
		    *pos = '\0';
		(*pids)[k] = atoi(pid);
		k++;
	}
	printf("get1\n");

	if (pclose(fpin) == -1) {
		printf("pclose error\n");
		exit(0);
	}

	return k;
}

int isRunning(char* procName) {
	FILE	*fpin;
	char cmd[512];
	char	pid[100];
	int k = 0;


	sprintf(cmd, "pidof -x %s | tr ' ' '\n'",procName);
	if ((fpin = popen(cmd, "r")) == NULL) {
		printf("popen error\n");
		exit(0);
	}

	if(fgets(pid, maxlines, fpin) != NULL) {
		k = 1;
	}

	if (pclose(fpin) == -1) {
		printf("pclose error\n");
		exit(0);
	}
	return k;
}

int parseConfig(ConfigProc *pm[MAXPROC], char *file) {
	int i = 0;
	FILE *config = fopen(file, "r");
	char name[buf];
	int runTime;

	if (config == NULL) {
		printf("failed to open file\n");
		exit(1);
	}
	while ((fscanf(config, "%s %d", name, &runTime)) == 2) {
		assert(name != NULL);
		// printf("\nRead Name,Time: %s, %d", name, runTime);
		int running = 0;
		running = isRunning(name);

		if(running == 0) { //No processes found
			char *logDataNotFound[] = {name};
			logMessage(4, logDataNotFound);
		}
		int temp[128] = {0};
		pm[i] = createConfigProc(name, runTime, &temp);

		i++;
	}
	fclose(config);
	return i;
}

void killOld () {
	int oldProcs[MAXPROC];
	int i;
	int k = 0;

	k = getPIDs("procnanny", &oldProcs);

	for(i = 0; i < k; i++){
		if(oldProcs[i] != getpid()){
			kill(oldProcs[i], SIGKILL);
			char oldPID[15];
			sprintf(oldPID, "%d",oldProcs[i]);
			char *logDataOld[] = {oldPID};
			logMessage(5, logDataOld);
		}
	}

}

int isBeingMonitored(pid_t pid, ChildProc *c[MAXPROC], int numC){
	return 0;
}

void refreshPIDs (ConfigProc *pm[MAXPROC],int numPM) {
	int i;

	for (i = 0; i < numPM; i++)
	{
		char *name = pm[i]->name;
		int pidList[MAXPROC];
		for (i = 0; i < MAXPROC; i++) {
		    pidList[i] = 0;
		}
		getPIDs(name,  &pidList);
		for (i = 0; i < 10; i++) {
		    printf("Got PID: %d\n", pidList[i]);
		}
		pm[i]->pids = &pidList;
	}
	printf("Done\n");
}


static void sig_handler(int signo)
{
	if (signo == SIGINT) {
		printf("received SIGINT\n");
		keepRunning = 0;
	}
	else if (signo == SIGHUP)
		printf("received SIGHUP\n");
	else if (signo == SIGUSR1)
		printf("received SIGUSR1\n");
	else
		printf("received signal %d\n", signo);
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
		int i;

		int numConfig = 0;
		int numChildren = 0;
		int numKilled = 0;
		ConfigProc *procsToMonitor[MAXPROC];
		ChildProc *children[MAXPROC];
		ConfigProc *(*pm)[] = &procsToMonitor; //inialize pointer to array of pointers
		ChildProc *(*c)[] = &children; //inialize pointer to array of pointers


		numConfig = parseConfig((*pm), argv[1]);

		refreshPIDs((*pm), numConfig);

		int numRunning = 0;

		for(i=0; i<numConfig; i++){
			int (*pids)[] = (*pm)[i]->pids;
			char *name = (*pm)[i]->name;
			printf("Name: %s | PID: %d\n",name,(*pids)[0]);
		}


		//TEMP FIX
		int pidArray[128] = {0};
		char *pRunning= "test";
		char *runTimeString = "test";
		int runTime = 5;
		int watchPID = 0;
		char *watchName = "test";
		//END TEMP FIX

		printf("test2\n");

		for(i = 0; i < numRunning; i++) {
			pid_t pID = fork();

			if (pID == 0){
				//Child
				char pidString[15];

				sprintf(pidString, "%d",pidArray[i]);
				char *logData1[] = {pRunning, pidString};
				logMessage(2, logData1);
				sleep(runTime);

				if(kill(pidArray[i], SIGKILL) == 0) {
					char *logData2[] = {pidString, pRunning, runTimeString};
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
				ChildProc *proc =  createChildProc(pID, 0, runTime, watchPID, watchName);
				(*c)[i] = proc;
			}
		}

		while(keepRunning){
			if (signal(SIGINT, sig_handler) == SIG_ERR){
				printf("\ncan't catch SIGINT\n");
			}
			if (signal(SIGHUP, sig_handler) == SIG_ERR)
				printf("\ncan't catch SIGUSR1\n");
			if (signal(SIGUSR1, sig_handler) == SIG_ERR)
				printf("\ncan't catch SIGUSR1\n");

			sleep(5);
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

		for(i = 0; i < numConfig; i++){
			destroyConfigProc((*pm)[i]);
		}

		for(i = 0; i < numChildren; i++){
			kill((*c)[i]->pID, SIGKILL);
			destroyChildProc((*c)[i]);
		}

	}
	else{
		printf("One argument expected.\n");
	}

	fflush(stdout);
	return 0;
}