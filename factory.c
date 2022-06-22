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

#include "shmem.h"
#include "wrappers.h"
#include "message.h"
int min(int x, int y)
{
    if (x <= y) 
    {
        return x;
    }
    return y;
}
int main( int argc , char *argv[] )
{
    int        shmid  ;
    key_t      shmkey ;
    int        shmflg ;
    shData    *p ;
	
	//---------------------------- Set up shared memory and initialize its objects -------------------------------------
	shmkey = ftok("shmem.h" , 0 ) ;
    shmflg = S_IRUSR | S_IWUSR;
	
	shmid  = Shmget( shmkey , SHMEM_SIZE , shmflg ) ;

    p      = (shData *) Shmat( shmid , NULL , 0 ); // Attach for Read/Write 

    // Access Named Semaphore
    sem_t *factory_Mutex = Sem_open2("/factory_Mutex", 0);

    int factory_ID = atoi(argv[1]);
    int capacity = atoi(argv[2]);
    int duration = atoi(argv[3]);

    // Find the message queue
    msgBuf msg;
    int msgStatus;
    int queueID;
    key_t supervisorKey = ftok("supervisor.c", 1);
	queueID = Msgget(supervisorKey, 0600);

    printf("Factory #%3d: STARTED. My Capacity = %3d, in %4d milliSeconds\n", factory_ID, capacity, duration);
    fflush(stdout);

    int local_remain;

    Sem_wait(factory_Mutex);
    local_remain = p -> remain;
    Sem_post(factory_Mutex);
    int total_parts = 0;
    int total_iterations = 0;
    while (local_remain > 0)
    {
        Sem_wait(factory_Mutex); // Start of Critical Section
        local_remain = p -> remain;
        if (local_remain == 0)
        {
            Sem_post(factory_Mutex);
            break;
        }
        int amountToMake = min(local_remain, capacity);
        printf("Factory #%3d: Going to make%6d parts in%5d milliSecs\n", factory_ID, amountToMake, duration);

        p -> remain -= amountToMake;
        local_remain = p -> remain;

        msg.purpose = 1; // This is a production message
        msg.facID = factory_ID; // This displays which factory the message is being sent from
        msg.capacity = capacity; // This is how many parts can be made by the factory
        msg.partsMade = amountToMake; // This is how many parts were made by the factory
        msg.duration = duration; // This is how long the production took

        msgStatus = msgsnd(queueID, &msg, MSG_INFO_SIZE, 0);
        if (msgStatus < 0) 
        {
            //printf("Factory #%d: Failed to send on queueID %d. Error code=%d\n",factory_ID, queueID, errno);
            //perror("Reason");
            exit(-2);
        } else 
        {
            //printf("\n Factory #%d: sent this message to Supervisor on queueID %d\n", factory_ID, queueID);
        }
        fflush(stdout);

        Sem_post(factory_Mutex); // End of Critical Section

        Usleep(duration);
        total_parts += amountToMake;
        total_iterations++;
    }

    Sem_wait(factory_Mutex);
    printf(">>> Factory #%4d: Terminating after making total of%6d parts in%5d iterations\n", factory_ID, total_parts, total_iterations);
    msg.purpose = 2; // This is a TERMINATION message
    msg.facID = factory_ID; // This displays which factory the message is being sent from
    msg.capacity = capacity; // This is how many parts can be made by the factory
    msg.partsMade = total_parts; // This is how many parts were made by the factory
    msg.duration = total_iterations; // This is how long the production took
    msgStatus = msgsnd(queueID, &msg, MSG_INFO_SIZE, 0);
    if (msgStatus < 0) 
    {
    //printf("Factory #%d: Failed to send on queueID %d. Error code=%d\n",factory_ID, queueID, errno);
    //perror("Reason");
    exit(-2);
    } else 
    {
        //printf("\n Factory #%d: sent this message to Supervisor\n", factory_ID);
    }
    fflush(stdout);
    Sem_post(factory_Mutex);
}