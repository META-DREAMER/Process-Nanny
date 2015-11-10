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
    int isRunning;
} ConfigProc;

typedef struct ChildProc {
    pid_t pID;
    int isFree;
    int monitorTime;
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
ConfigProc *createConfigProc(char *name, int runTime, int isRunning);
void destroyConfigProc(ConfigProc *proc);
ChildProc *createChildProc(pid_t pID, int isFree, int monitorTime);
void destroyChildProc(ChildProc *proc);
static void	sig_handler(int);




ConfigProc *createConfigProc(char *name, int runTime, int isRunning) {
    ConfigProc *proc = malloc(sizeof(ConfigProc));
    assert(proc != NULL);

    proc->name = strdup(name);
    proc->runTime = runTime;
    proc->isRunning = isRunning;

    return proc;
}

void destroyConfigProc(ConfigProc *proc) {
    assert(proc != NULL);

    free(proc->name);
    free(proc);
    printf("Dstr Config\n");
}

ChildProc *createChildProc(pid_t pID, int isFree, int monitorTime) {
    ChildProc *proc = malloc(sizeof(ChildProc));
    assert(proc != NULL);

    proc->pID = pID;
    proc->isFree = isFree;
	proc->monitorTime = monitorTime;

    return proc;
}

void destroyChildProc(ChildProc *proc) {
    assert(proc != NULL);
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

int parseConfig(ConfigProc *pm[MAXPROC], char *file) {
	int i = 0;
	FILE *config = fopen(file, "r");

	char name[buf];
	int runTime;


	if (config == NULL) {
		printf("failed to open file\n");
		exit(1);
	}
	while (1) {
		int ret = fscanf(config, "%s %d", name, &runTime);
        if(ret == 2) {
			printf("\nRead Name,Time: %s, %d", name, runTime);
			ConfigProc *proc = createConfigProc(name, runTime, 0);
			pm[i] = proc;
			i++;
        }
		else {
			break;
		}
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



int checkRunning (int pidArray[MAXPROC], char pNames[MAXPROC][buf], char pRunning[MAXPROC][buf], int numToMonitor) {
	char	pid[100];
	FILE	*fpin;
	int i;
	int k = 0;
	for (i = 1; i <= numToMonitor; i++)
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

void cleanExit (ConfigProc *procsToMonitor[], int numConfig,  ChildProc *children[], int numChild) {
	int i = 0;
	for(i = 0; i < numConfig; i++){
		destroyConfigProc(procsToMonitor[i]);
	}
	for(i = 0; i < numChild; i++){
		kill(children[i]->pID, SIGKILL);
		destroyChildProc(children[i]);
	}
	free(procsToMonitor);
	free(children);
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
		int runTime;
		int i;

		int numToMonitor = 0;
		int numChildren = 0;
		ConfigProc *procsToMonitor[MAXPROC];
		ChildProc *children[MAXPROC];

		ConfigProc *(*pm)[] = &procsToMonitor; //inialize pointer to array of pointers
		ChildProc *(*c)[] = &children; //inialize pointer to array of pointers

		// ConfigProc *procsToMonitor = calloc(MAXPROC, sizeof(ConfigProc));
		// ConfigProc **pm = &procsToMonitor;
		// ChildProc *children = calloc(MAXPROC, sizeof(ChildProc));
		// ChildProc **c = &children;

		int numKilled = 0;
		char configLines[128][255] = {{0}};
		char pRunning[128][255] = {{0}};
		int pidArray[MAXPROC];
		memset(pidArray, 0, MAXPROC);

		numToMonitor = parseConfig((*pm), argv[1]);

		printf("\nNum to monitor: %d\n", numToMonitor);


		runTime = atoi(configLines[0]);

		char runTimeString[15];
		sprintf(runTimeString, "%d",runTime);
		
		int numRunning = checkRunning(pidArray, configLines, pRunning, numToMonitor);

		for(i = 0; i < numRunning; i++) {
			pid_t pID = fork();

			if (pID == 0){
				//Child
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

				ChildProc *proc =  createChildProc(pID, 0, runTime);
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

		for(i = 0; i < numToMonitor; i++){
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