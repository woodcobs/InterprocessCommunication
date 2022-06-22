//---------------------------------------------------------------------
// Assignment : PA-02 Concurrent Processes & IPC
// Date       : 04/02/2022
// Author     : Shane McCarthy and Bradley Woodcock
//----------------------------------------------------------------------

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <time.h>

#include "shmem.h"
#include "wrappers.h"
#include "message.h"

int main( int argc , char *argv[] )
{
    int        shmid  ;
    key_t      shmkey ;
    int        shmflg ;
    shData    *p ;
	srandom(time(NULL)); // seed the RNG
	
	//---------------------------- Set up shared memory and initialize its objects -------------------------------------
	shmkey = ftok("shmem.h" , 0 ) ;
    shmflg = IPC_CREAT | IPC_EXCL  | S_IRUSR | S_IWUSR ; //sales process must run first
	
	shmid  = Shmget( shmkey , SHMEM_SIZE , shmflg ) ;

    p      = (shData *) Shmat( shmid , NULL , 0 ); // Attach for Read/Write 
	
	int numFactories = atoi(argv[1]);

	// Initialize data in the shared memory
	p->order_size = atoi(argv[2]);
	p->made = 0;
	p->remain = atoi(argv[2]);
	printf("Sales: Will Request an Order of Size = %d parts\n", p->order_size); 
	printf("Creating %d Factory(ies)\n", numFactories);
	
	//---------------------------- Set up semaphores and the message queue -------------------------------------
	int mutex_flags = O_CREAT;
	int mutex_permissions = S_IRUSR | S_IWUSR;

	//rendevous semaphores*
	sem_t *rendezvousSem = Sem_open("/rendezvousSem", mutex_flags, mutex_permissions, 0); // this is used in supervisor to signal that the factories are done


	// NOT THE RENDEZVOUS SEMAPHORE
	sem_t *factory_Mutex = Sem_open("/factory_Mutex", mutex_flags, mutex_permissions, 0); // this is used in factory.c to update and receive "remain"

	//----- Message queue initialization -----
	msgBuf msg; // factory to supervisor
	
    msgPurpose_t  purpose ;  /* Purpose of this message to Supervisor */
    int  facID    ,          /* sender's Factory ID */
         capacity ,          /* #of parts made in most recent iteration */
         partsMade,          /* #of parts made in most recent iteration */
         duration ; 

	int queueID;
	key_t supervisorKey = ftok("supervisor.c", 1);
	queueID = Msgget(supervisorKey, IPC_CREAT | IPC_EXCL | 0600);
	
	//---------------------------- Fork / Execute Supervisor process -------------------------------------
	pid_t superID;
	int fd_sup;
	int supStatus;
	
	waitpid(superID, &supStatus, 0);
	//Sem_post(rendezvousSem);
	superID = Fork();
	if (superID == 0)
	{
		int superLogflg = O_WRONLY | O_CREAT | O_TRUNC | S_IRUSR | S_IWUSR;
		fd_sup = open("./supervisor.log", superLogflg, 0600);
		if (fd_sup == -1)
		{
			err_sys("Opening Supervisor's log file failed");
		}
		dup2(fd_sup, 1); //redirect output to log file
		snprintf(argv[1], SHMEM_SIZE, "%d", numFactories);

		if (execlp("./supervisor", "SuperVterm", argv[1], NULL) < 0)
		{
			err_sys("SALES: execlp Supervisor Failed");
		}
		//exit(0);
	}

	//---------------------------- Fork / Execute Factory processes -------------------------------------

	int factoryLogflg = O_WRONLY | O_CREAT | O_APPEND;

	int fd_fac;
	int local_Capacity;
	int local_Duration;
	int i = 1;

	Sem_post(factory_Mutex);
	for (i = 1; i <= numFactories; i++) 
	{
		local_Capacity = (random() % 40) + 11;
		local_Duration = (random() % 700) + 501;


		if (Fork() == 0)
		{
			fd_fac = open("./factory.log", factoryLogflg, 0600);
			if (fd_fac == -1)
			{
				err_sys("Opening Factory's log file failed");
			}
			dup2(fd_fac, 1); //redirect output to log file
			char arg1[20], arg2[20], arg3[20];

			snprintf(arg1, sizeof(arg1), "%d", i);
			snprintf(arg2, sizeof(arg2), "%d", local_Capacity);
			snprintf(arg3, sizeof(arg3), "%d", local_Duration);
			if (execlp("./factory", "factory", arg1, arg2, arg3, NULL) < 0)
			{
				err_sys("SALES: execlp Factory Failed");
			}
			exit(0);
		}
		printf("SALES: Factory #    %d was created, with Capacity=  %d and Duration= %d\n", i, local_Capacity, local_Duration);
	}

	//---------------------------- Wait for supervisor to finish its task of printing the final report -------------------------------------
	Sem_wait(rendezvousSem);
	printf("SALES:   Supervisor says all Factories have completed their mission\n");

	//---------------------------- Clean up after zombie processes (Supervisor + all factories) -------------------------------------
	printf("SALES:   Shutting down the Supervisor all Factory Processes\n");

	//---------------------------- Destroy the shared memory -------------------------------------
	shmdt( p ) ;
	shmctl( shmid , IPC_RMID , NULL );

	//---------------------------- Destroy the semaphores and the message queue -------------------------------------
	Sem_close(rendezvousSem);
	Sem_close(factory_Mutex);
	
	Sem_unlink("/rendezvousSem");
	Sem_unlink("/factory_Mutex");
	
	msgctl(queueID, IPC_RMID, NULL);
}