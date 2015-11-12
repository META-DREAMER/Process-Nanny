#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h> 
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include "memwatch.h"

#define P_READ     0
#define P_WRITE    1
#define C_READ     2
#define C_WRITE    3
#define P_NUM	   2

const int buf = 255;
const int MAXPROC = 128;
const int maxlines = 4096;
const char pathVar[] = "PROCNANNYLOGS";
const char parentToChild[] = "PID:%d TIME:%d PIPE:%d";
const char childToParent[] = "PID:%d KILLED:%d PIPE:%d";
const char log_start[]= "[%s] Info: Parent process is PID %s."; //logType 0
const char log_notFound[]= "[%s] Info: No '%s' processes found."; //logType 1
const char log_initMon[]= "[%s] Info: Initializing monitoring of process '%s' (PID %s)."; //logType 2
const char log_killedProc[]= "[%s] Action: PID %s (%s) killed after exceeding %s seconds."; //logType 3
const char log_exit[]= "[%s] Info: Exiting. %s process(es) killed."; //logType 4
const char log_killedOld[]= "[%s] Action: Killed previous instance of Procnanny (PID %s)."; //logType 5
const char log_SIGHUP[]= "[%s] Info: Caught SIGHUP. Configuration file '%s' re-read."; //logType 6
const char log_SIGINT[]= "[%s] Info: Caught SIGINT. Exiting cleanly.  %s process(es) killed."; //logType 7


typedef struct ConfigProc {
	char *name;
	int runTime;
	int *pids;
	int numPIDs;
} ConfigProc;

typedef struct ChildProc {
	pid_t pID;
	int isFree;
	time_t started;
	int watchTime;
	pid_t watchPID;
	char *watchName;
} ChildProc;

time_t curtime;

//FLAGS
int logExists = 0;
int keepRunning = 1;
int readConfig = 0;
int closedPipes = 0;
int canWriteToParent = 0;
int canReadFromParent = 0;

//FuncDef
time_t time(time_t *t);
char *ctime(const time_t *timer);
pid_t getpid(void);
pid_t fork(void);
unsigned int sleep(unsigned int seconds);
pid_t waitpid(pid_t pid, int *status, int options); 
ConfigProc *createConfigProc(char *name, int runTime, int pids[MAXPROC], int numPIDs);
void destroyConfigProc(ConfigProc *proc);
ChildProc *createChildProc(pid_t pID, int isFree, int watchTime, pid_t watchPID, char *watchName);
void destroyChildProc(ChildProc *proc);
int isRunning(char* procName);
static void	sig_handler(int);


ConfigProc *createConfigProc(char *name, int runTime, int pids[MAXPROC], int numPIDs) {
	ConfigProc *proc = malloc(sizeof(ConfigProc));
	assert(proc != NULL);

	proc->name = strdup(name);
	proc->runTime = runTime;
	proc->pids = malloc(MAXPROC * sizeof(int));
	memcpy(proc->pids, pids, MAXPROC * sizeof(int));
	proc->numPIDs = numPIDs;

	return proc;
}

void destroyConfigProc(ConfigProc *proc) {
	assert(proc != NULL);

	free(proc->name);
	free(proc->pids);
	free(proc);
}

ChildProc *createChildProc(pid_t pID, int isFree, int watchTime, pid_t watchPID, char *watchName) {
	ChildProc *proc = malloc(sizeof(ChildProc));
	assert(proc != NULL);

	time_t started;
	time(&started);

	proc->pID = pID;
	proc->isFree = isFree;
	proc->started = started;
	proc->watchTime = watchTime;
	proc->watchPID = watchPID;
	proc->watchName = strdup(watchName);

	return proc;
}

void destroyChildProc(ChildProc *proc) {
	assert(proc != NULL);
	free(proc->watchName);
	free(proc);
}

void printChild(ChildProc *proc) {
    printf("Child PID:%d, isFree:%d, watchTime:%d, watchPID:%d,watchName:%s\n", 
    		proc->pID,proc->isFree,proc->watchTime,proc->watchPID,proc->watchName);
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
			//Killed Old
			if(argv[0] == NULL){ return;} //exit if not enough args
			sprintf(message, log_killedOld,date,argv[0]);
			break;
		case 6:
			//Caught SigHUP
			if(argv[0] == NULL){ return;} //exit if not enough args
			sprintf(message, log_SIGHUP,date,argv[0]);
			printf("\n%s\n",message);
			fflush(stdout);
			break;
		case 7:
			//Caught SigINT
			if(argv[0] == NULL){ return;} //exit if not enough args
			sprintf(message, log_SIGINT,date,argv[0]);
			printf("\n%s\n",message);
			fflush(stdout);
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
	int		tempPIDs[MAXPROC];
	int k = 0;
	int i;
	int numFound = 0;


	for (i = 0; i < MAXPROC; i++) {
		(*pids)[i] = 0;
		tempPIDs[i] = 0;
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
		tempPIDs[k] = atoi(pid);
		k++;
	}

	if (pclose(fpin) == -1) {
		printf("pclose error\n");
		exit(0);
	}

	for(i=0; i<k; i++){
		char userCMD[512];
		char* user;
		if ((user = getenv("USER")) == NULL){
			printf("Cant Get User ENV\n");
			exit(0);
		}
		sprintf(userCMD, "ps -au %s | grep '\\b%d\\b'",user,tempPIDs[i]);

		if ((fpin = popen(userCMD, "r")) == NULL) {
			printf("popen error\n");
			exit(0);
		}

		if(fgets(pid, maxlines, fpin) != NULL) {
			(*pids)[numFound] = tempPIDs[i];
			numFound++;
		}

		if (pclose(fpin) == -1) {
			printf("pclose error\n");
			exit(0);
		}
	}

	return numFound;
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

//updates ConfigProc array with whats in the config file
//returns number of procs in config file
int parseConfig(ConfigProc *pm[MAXPROC], int numConfig, char *file) {
	int i = 0, j;
	FILE *config = fopen(file, "r");
	char name[buf];
	int runTime;

	if (config == NULL) {
		printf("failed to open file\n");
		exit(1);
	}

	for(j=0;j<numConfig;j++){
		destroyConfigProc(pm[j]);
	}
	while ((fscanf(config, "%s %d", name, &runTime)) == 2) {
		assert(name != NULL);
		// printf("\nRead Name,Time: %s, %d", name, runTime);
		int running = 0;
		running = isRunning(name);

		if(running == 0) { //No processes found
			char *logDataNotFound[] = {name};
			logMessage(1, logDataNotFound);
		}
		int pids[128] = {0};
		pm[i] = createConfigProc(name, runTime, pids, 0);

		i++;
	}
	fclose(config);
	printf("Parsed %d procs in config\n", i);
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
	int i;
	for(i=0; i<numC; i++){
		if((c[i]->watchPID == pid) && (c[i]->isFree != 1))
			return 1;
	}
	return 0;
}

void refreshPIDs (ConfigProc *pm[MAXPROC],int numPM) {
	int i,j;

	for (i = 0; i < numPM; i++)
	{
		char *name = pm[i]->name;
		// int runTime = pm[i]->runTime;
		int pidList[MAXPROC];
		for (j = 0; j < MAXPROC; j++) {
			pidList[i] = 0;
		}
		int numPIDs = getPIDs(name,  &pidList);
		for(j=0;j<numPIDs;j++){
			pm[i]->pids[j] = pidList[j];
		}
		pm[i]->numPIDs = numPIDs;
	}

}

static void parentSigHandler(int signo) {
	if (signo == SIGINT)
		keepRunning = 0;
	else if (signo == SIGHUP)
		readConfig = 1;
}

static void sig_handler(int signo) {
	if (signo == SIGUSR1)
		canReadFromParent = 1;
	else if (signo == SIGUSR2)
		canWriteToParent = 1;
}

int updateChildren(ChildProc *c[MAXPROC], int numC, int (*pipes)[MAXPROC][2]){
	int i;
	time_t now;
	time(&now);
	char info[buf];
	int numKilled = 0;

	for(i=0;i<numC;i++) {
		ChildProc *child = c[i];
		memset(info, 0, sizeof(info));
		time_t started = child->started;
		int diff = now - started;

		if(child->isFree == 1){
			continue;
		}

		printf("Checking Child#%d with wTime:%d, diff:%d\n", i+1,child->watchTime, diff);

		if(diff >= (child->watchTime)) {
			kill(child->pID, SIGUSR2);
			int numRead;
			numRead = read((*pipes)[0][0], info, buf);
			if (numRead < 0) {
				perror("parent failed to read");
				continue;
			}
			else if (numRead == 0) {
				perror("parent read EOF");
				continue;
			}
			printf("Parent received from Child#%d :%s\n", i+1,info);

			pid_t watchPID;
			int watchPipe;
			int killed = 0;

			sscanf(info, childToParent, &watchPID,&killed,&watchPipe);
			assert(watchPipe == (i+1));
			assert(watchPID == (child->watchPID));
			child->isFree = 1;

			if(killed){
				char pidString[15], timeString[15], nameString[15];
				memset(pidString, 0, sizeof(pidString));
				memset(timeString, 0, sizeof(timeString));
				memset(nameString, 0, sizeof(nameString));
				sprintf(pidString, "%d",child->watchPID);
				sprintf(timeString, "%d",child->watchTime);
				sprintf(nameString, "%s",child->watchName);
				char *logKilled[] = {pidString, nameString, timeString};
				logMessage(3, logKilled);
				numKilled++;
			}
		}
	}
	return numKilled;
}

//taken: http://www.dreamincode.net/forums/topic/317682-trying-to-close-an-array-of-pipes-in-c/
void closeReadPipes(int (*pipes)[MAXPROC][2], int childNum) {
	int i;
	for(i = 0; i < MAXPROC; i++) {
		if(i != childNum){
			// printf("Closing pipes[%d][0] for process %d\n", i, childNum);
			if(close((*pipes)[i][0]) < 0){
				perror("child Failed to close read end of pipe");
			}

		}
	}
}
//close all write pipes but parents
void closeWritePipes(int (*pipes)[MAXPROC][2], int childNum) {
	int i;
	for(i = 1; i < MAXPROC; i++) {
		if(close((*pipes)[i][1]) < 0)
			perror("Failed to close write end of pipe");
	}
}

void startMonitor(int (*pipes)[MAXPROC][2], int childPipe) {
	closeReadPipes(pipes, childPipe);
	closeWritePipes(pipes, childPipe);
	printf("Child#%d PID:%d Start Monitor.\n", childPipe, getpid());

	while(1) {
		pid_t watchPID;
		int watchTime, watchPipe;
		int killed = 0, numRead;
		char info[buf];
		memset(info, 0, sizeof(info));
		if(!canReadFromParent){
			printf("Child#%d cant read, sleeping\n", childPipe);
			pause();
			printf("Child#%d woke up 1\n", childPipe);
			continue;
		}
		else {
			canReadFromParent = 0;
			numRead = read((*pipes)[childPipe][0], info, buf);
			if (numRead < 0) {
				perror("Child failed to read");
				continue;
			}
			else if (numRead == 0) {
				perror("Child read EOF");
				continue;
			};
		}
		
		sscanf(info, parentToChild, &watchPID,&watchTime,&watchPipe);
		assert(watchPipe == childPipe);
		assert(watchPID > 0);
		assert(watchTime > 0);
		sleep(watchTime);
		if(kill(watchPID, SIGKILL) == 0)
			killed = 1;

		memset(info, 0, sizeof(info));
		sprintf(info, childToParent, watchPID,killed,watchPipe);
		printf("Child#%d done monitoring. Info: %s\n", childPipe,info);
		while(1) {
			if(!canWriteToParent){
				printf("Child#%d cant write, sleeping\n", childPipe);
				pause();
				continue;
			}
			else{
				canWriteToParent = 0;
				if (write((*pipes)[0][1], info, buf) != buf)
					perror("Child failed to write to parent");
				else
					printf("Child#%d wrote back: %s\n", childPipe,info);
				break;
			}
		}
	}
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

		//Create pipes for all children
		int pipes[MAXPROC][2];

		for (i=0; i<MAXPROC; i++) {
			if((pipe(pipes[i])) < 0) {
				printf("Failed to open pipe");
			}
		}

		int numConfig = 0;
		int numChildren = 0;
		int numKilled = 0;
		ConfigProc *procsToMonitor[MAXPROC];
		ChildProc *children[MAXPROC];
		ConfigProc *(*pm)[] = &procsToMonitor; //inialize pointer to array of pointers
		ChildProc *(*c)[] = &children; //inialize pointer to array of pointers

		numConfig = parseConfig((*pm), numConfig, argv[1]);
		refreshPIDs((*pm), numConfig);

		if (signal(SIGUSR1, sig_handler) == SIG_ERR)
			printf("\ncan't catch SIGUSR1\n");
		if (signal(SIGUSR2, sig_handler) == SIG_ERR)
			printf("\ncan't catch SIGUSR2\n");
		if (signal(SIGINT, parentSigHandler) == SIG_ERR)
			printf("\ncan't catch SIGINT\n");
		if (signal(SIGHUP, parentSigHandler) == SIG_ERR)
			printf("\ncan't catch SIGHUP\n");

		while(1) {

			if(keepRunning == 0) {
				char numKilledString[5];
				sprintf(numKilledString, "%d",numKilled);
				char *logNumKilled[] = {numKilledString};
				logMessage(7, logNumKilled);
				break;
			}
			if(readConfig == 1) {
				printf("Read Config\n");
				char *logFileName[] = {argv[1]};
				logMessage(6, logFileName);
				numConfig = parseConfig((*pm), numConfig, argv[1]);
				readConfig = 0;
			}

			numKilled += updateChildren((*c), numChildren, &pipes);
			refreshPIDs((*pm), numConfig);

			for(i=0;i<numConfig;i++) {
				int *pidList = (*pm)[i]->pids;
				int numPIDs = (*pm)[i]->numPIDs;
				int j = 0;
				pid_t curPID = -1;

				for(j=0;j<numPIDs;j++) {
					curPID = pidList[j];
					if(curPID == 0)
						break;

					if(isBeingMonitored(curPID, (*c), numChildren)){
						//TODO: Update ones that are done monitoring
						printf("PID %d already mon.\n", curPID);
						continue;
						// printf("BM PID: %d\n",curPID);
					}

					int k;
					int assigned = 0;
					pid_t childPID;
					int childPipe;
					for(k=0;k<numChildren;k++) { 
						if((*c)[k]->isFree){ //check if old children are free
							free((*c)[k]->watchName);
							// (*c)[k]->watchName = malloc(sizeof((*pm)[i]->name));
							(*c)[k]->watchName = strdup((*pm)[i]->name);
							(*c)[k]->watchPID = curPID;
							(*c)[k]->watchTime = (*pm)[i]->runTime;
							(*c)[k]->isFree = 0;
							assigned = 1;
							childPID = (*c)[k]->pID;
							childPipe = k+1;
							printf("assigned pid %d to old child#%d\n", curPID, childPipe);
							break;
						}
					}
					if(!assigned) { //create new child if unable to reuse old child
						childPipe = numChildren+1; //pipe0 is parent
						childPID = fork();
						if(childPID == 0) {
							//Child
							startMonitor(&pipes, childPipe);
							/*never reached*/
							exit(1);
						}
						else if (childPID < 0){
							printf("Fork Failed\n");
							exit(1);
						}
						else {
							(*c)[numChildren] = createChildProc(childPID, 0, (*pm)[i]->runTime, curPID, (*pm)[i]->name);
							numChildren++;
						}
					}
					if(childPID > 0) {
						ChildProc *assigned = (*c)[childPipe-1];
						time_t startMonitor;
						char info[buf];
						memset(info, 0, sizeof(info));
						sprintf(info, parentToChild, assigned->watchPID,assigned->watchTime,childPipe);
						kill(assigned->pID, SIGUSR1);
						printf("Send Start Signal to Child#%d PID:%d\n", childPipe, assigned->pID);
						if (write(pipes[childPipe][1], info, buf) != buf){
							perror("Parent failed to write to child");
						}
						time(&startMonitor);
						assigned->started = startMonitor;
						char pidString[15];
						memset(pidString, 0, sizeof(pidString));
						sprintf(pidString, "%d",assigned->watchPID);
						char *logStartMonitor[] = {assigned->watchName, pidString};
						logMessage(2, logStartMonitor);
					}
				}
			}
			sleep(5);
			printf("Woke Up\n");
		}

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