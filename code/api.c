#include "api.h"
#include "io.h"
#include "kernel.h"	// Fixes implicit declarations (good imho)
#include "kernel2.h"
#include "InfoFIFO.h"

/**
	Workflow for initializing a fifo according to test1.c
	initFifoInfo(shmaddr)
	createFifo(shmaddr, 16*4, 1, 1);
	-- Task1
		openFifo(shmaddr,1,0);
		writeFifo(ififo,data,1,0);
	-- Task2
		openFifo(shmaddr,1,0);
		readFifo(ififo,data,1,0);

	
*/

volatile struct Info_FIFO *info;
volatile int Entering[NUM_FIFOS][NUMBER_OF_PROC];
volatile int Number[NUM_FIFOS][NUMBER_OF_PROC] ;


int createFifo(void* shmaddr, uint32_t tokensize, uint32_t fifosize, int id)
{	
	int result = -1; //Assume failure
	if(0 <= id && id < NUM_FIFOS )
	{
		
	if(id == 0)
	{
		info[id].fifoAddress = (void *)FIFO_BASE_ADDRESS; 
	}
	else
	{	
		info[id].fifoAddress = (void *)info[id-1].fifoAddress+ info[id-1].token_size * info[id-1].fifo_size ;
	}
	
  	info[id].id = id;
  	info[id].token_size = tokensize;
  	info[id].fifo_size = fifosize;
	
  	info[id].writec = 0;
	info[id].readc  = 0;
 	info[id].direction = 0;
	info[id].con_ID = 0;
	info[id].active = 1;
	info[id].tdma = 0;
	info[id].remote = 0;

	result = 0;
	}

	return result;
}



//Initialize the fifo adminstration.
void initFifoInfo(void* shmaddr)
{
	int i = 0;
	// Sets the base address for the administration of all the fifos
	info = (struct InfoFifo *) shmaddr;
	for(i = 0; i<NUM_FIFOS; i++)
	{
		// All Fifos have the same memory address for the administration.
		info[i].fifoAdminAddress = shmaddr;
	}

	initSemaphores(NUMBER_OF_LOCKS);
}

//Enables the user to access a fifo on his own.
volatile struct Info_FIFO* openFifo(void* shmaddr, int id, int mode)
{
	int i =0;
	struct Info_FIFO* result = NULL;

	for(i=0; i<NUM_FIFOS; i++)
	{
		if(info[i].id == id)
		{
			result = (struct Info_FIFO *) &info[i];
		}
	}	

	return result;
}

void* acquire_w(volatile struct Info_FIFO* ptrFifo)
{
		
	lock(ptrFifo->id, GetCurrentTaskID(), NUMBER_OF_PROC);
	
	
	return (void *) ptrFifo;
}


void release_w(volatile struct Info_FIFO* ptrFifo)
{
	unlock(ptrFifo->id, GetCurrentTaskID(), 0);
	
}

void *acquire_r(volatile struct Info_FIFO* ptrFifo)
{
	lock(ptrFifo->id, GetCurrentTaskID() ,NUMBER_OF_PROC);
	return (void *) ptrFifo;
}

void release_r(volatile struct Info_FIFO* ptrFifo)
{
	unlock(ptrFifo->id, GetCurrentTaskID(), 0);
}


int writeFifo(volatile struct Info_FIFO* ptrFifo, void* data, int bytes)
{	
	int writable = 0;
	int result = 0;
	int size_in_bytes = ptrFifo->fifo_size*ptrFifo->token_size;

	//Check to see if we have space to write
	if(ptrFifo->readc <= ptrFifo->writec && ptrFifo->direction == 0)
	{
		writable = size_in_bytes + ptrFifo->readc - ptrFifo->writec;
	}
	else
	{
		writable = ptrFifo->readc - ptrFifo->writec;
	}

	acquire_w(ptrFifo);
	
	// Write as much as possible and return number of bytes written
	if(writable > bytes)
	{
		memcpy(data, ptrFifo->data, bytes);
		ptrFifo->writec = ( ptrFifo->writec + bytes ) % size_in_bytes;
		result = bytes;
	}
	else
	{
		memcpy(data, ptrFifo->data, writable);
		ptrFifo->writec = ( ptrFifo->writec + writable ) % size_in_bytes;
		result = writable;
	}


	if(ptrFifo->readc == ptrFifo->writec)
	{	
		// We have written to the read pointer, buffer is full;
		// We use direction for lack of a better alternative
		
		ptrFifo->direction = 1;
	}
	
		
	release_w(ptrFifo);


	return result;
}


int readFifo(volatile struct Info_FIFO* ptrFifo, int* data, int bytes)
{	
	int result = 0;
	int readable = 0;
	int size_in_bytes = ptrFifo->fifo_size*ptrFifo->token_size;

	//Check if there is anything to read
	if(ptrFifo->readc <= ptrFifo->writec && ptrFifo->direction == 0 )
	{
		readable = ptrFifo->writec - ptrFifo->readc;
	}
	else
	{
		readable = size_in_bytes + ptrFifo->writec - ptrFifo->readc;	
	}

	acquire_r(ptrFifo);
	
	//Read as much as is allowed.
	if(readable > bytes)
	{
		memcpy(data, ptrFifo->data, bytes);
		ptrFifo->readc = ( ptrFifo->readc + bytes ) % size_in_bytes;
		result = bytes;
	}
	else
	{
		memcpy(data, ptrFifo->data, readable);
		ptrFifo->readc = ( ptrFifo->readc + readable ) % size_in_bytes;
		result = readable;
	}	

	if(ptrFifo->readc == ptrFifo->writec)
	{	
		// We have read to the write pointer, buffer is empty;
		// We use direction for lack of a better alternative
		ptrFifo->direction = 0;
	}

	release_r(ptrFifo);
	return result;
}

int maximum( int *ptr, int num )
{
  /* According to Gadi Taubenfeld -
     Synchronization Algorithms and Concurrent Programming
     This implementation of max is correct for Bakery algorithm */
  int max = 0, i, current;
  for ( i = 0; i < num; i++ )
  {
    current = ptr[i];
    if ( max < current )
      max = current;
  }

  return max;
}

/***************************************
* lock (based on Bakery)
* inputs:
*       * lockid: to identify between different locks
*       * id: to identify between competing tasks for a lock
*       * num: maximum number of competing tasks for this lock
*
* CS is entered after the function returns.
***************************************/
void lock(int lockid, int id, int num)
{
	//printf("lock called \n");
	int i,x =0;	
	Entering[lockid][id] = 1;
	Number[lockid][id] = 1 + maximum(Number[lockid], num);
	Entering[lockid][id] = 0;
	for(i=0; i < num; i++)
	{
	
		while(Entering[lockid][i]);
		
		while ((Number[lockid][i] != 0) 
			&& (Number[lockid][i] < Number[lockid][id]));	
	}
}

/***************************************
* unlock
* inputs:
*       * lockid: to identify between different locks
*       * id: to identify between competing tasks for a lock
*       * num: maximum number of competing tasks for this lock
*
* Another CS(i) can be entered after the function returns.
***************************************/
void unlock(int lockid, int id, int num)
{
	Number[lockid][id] = 0;
}


void initSemaphores(int numberoflocks)
{
	int i, j = 0;
	for(i=0; i<NUM_FIFOS; i++)
	{
		for(j=0;j<NUMBER_OF_PROC; j++)
		{
			Number[i][j] = 0;
			Entering[i][j] = 0;
		}
	}
}
