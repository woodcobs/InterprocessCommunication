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
#include <errno.h>
#include <sys/msg.h>
#include <sys/ipc.h>

#include "shmem.h"
#include "wrappers.h"
#include "message.h"

int main( int argc , char *argv[] )
{

    int activeFactories = atoi(argv[1]);
    int totalFactories = atoi(argv[1]);
    int producedParts[activeFactories + 1];
    int factoryIterations[activeFactories + 1];

    for (int i = 0; i < activeFactories + 1; i++)
    {
        producedParts[i] = 0;
        factoryIterations[i] = 0;
    }

    int        shmid  ;
    key_t      shmkey ;
    int        shmflg ;
    shData    *p ;
	
	//---------------------------- Set up shared memory and initialize its objects -------------------------------------
	shmkey = ftok("shmem.h" , 0 ) ;
    shmflg = S_IRUSR;
	
	shmid  = Shmget( shmkey , SHMEM_SIZE , shmflg ) ;

    p      = (shData *) Shmat( shmid , NULL , 0 ); // Attach for Read only (supervisor does not need to change any values in shared memory)


    printf("SUPERVISOR: Started\n");
    fflush(stdout);

    // Access Rendezvous Semaphore
    sem_t *rendezvousSem = Sem_open2("/rendezvousSem", S_IRUSR | S_IWUSR);
    Sem_post(rendezvousSem);
    Sem_wait(rendezvousSem);
    printf("Supervisor has the semaphore\n");
    fflush(stdout);
     // Find the message queue
    int queueID;
    int msgStatus;
    msgBuf msg;
    key_t supervisorKey = ftok("supervisor.c", 1);
	queueID = Msgget(supervisorKey, IPC_EXCL | 0666);

    while (activeFactories > 0)
    {
        //printf("Inside the while loop. activeFactories = %d and queueID is %d\n", activeFactories, queueID);
        fflush(stdout);
        // Get message from message queue
        msgStatus = msgrcv(queueID, &msg, MSG_INFO_SIZE, 0, 0);
        if (msgStatus < 0)
        {
            printf("Supervisor failed to receive from factory on queueID %d. Error Code=%d\n", queueID, errno);
            perror("Reason");
            exit(-2);
            fflush(stdout);
        } else {
           // printf("Message received!\n");
            //fflush(stdout);
        }
        
        if (msg.purpose == 1)
        {
            printf("SUPERVISOR: Factory #%3d produced%5d parts in%6d milliSecs\n", msg.facID, msg.partsMade, msg.duration);
            producedParts[msg.facID] += msg.partsMade;
            factoryIterations[msg.facID] += 1;
            fflush(stdout);
        } else if(msg.purpose == 2)
        {
            printf("SUPERVISOR: Factory #%3d        COMPLETED its task\n", msg.facID);
            activeFactories--;
        } else
        {
            printf("Invalid purpose\n");
        }
        fflush(stdout);
    }
    //-------- All factories finished, deliver report --------
    int totalMade = 0;
    printf("\n****** SUPERVISOR: Final Report ******\n");
    for (int i = 1; i < totalFactories + 1; i++) 
    {
        printf("Factory #%3d made a total of%5d parts in%6d iterations\n", i, producedParts[i], factoryIterations[i]);
        totalMade += producedParts[i];
    }
    printf("===============================\n");
    printf("Grand total parts made =%6d   vs   order size of%6d", totalMade, p -> order_size);
    printf("\n>>> Supervisor Terminated\n");
    fflush(stdout);
    Sem_post(rendezvousSem);
}