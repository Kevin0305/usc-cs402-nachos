// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "synch.h"
#include <stdio.h>
#include <iostream>

using namespace std;

int copyin(unsigned int vaddr, int len, char *buf) {
    // Copy len bytes from the current thread's virtual address vaddr.
    // Return the number of bytes so read, or -1 if an error occors.
    // Errors can generally mean a bad virtual address was passed in.
    bool result;
    int n=0;			// The number of bytes copied in
    int *paddr = new int;

    while ( n >= 0 && n < len) {
      result = machine->ReadMem( vaddr, 1, paddr );
      while(!result) // FALL 09 CHANGES
	{
	  result = machine->ReadMem( vaddr, 1, paddr ); // FALL 09 CHANGES: TO HANDLE PAGE FAULT IN THE ReadMem SYS CALL
	}	
      
      buf[n++] = *paddr;
      
      if ( !result ) {
	//translation failed
	return -1;
      }

      vaddr++;
    }

    delete paddr;
    return len;
}

int copyout(unsigned int vaddr, int len, char *buf) {
    // Copy len bytes to the current thread's virtual address vaddr.
    // Return the number of bytes so written, or -1 if an error
    // occors.  Errors can generally mean a bad virtual address was
    // passed in.
    bool result;
    int n=0;			// The number of bytes copied in

    while ( n >= 0 && n < len) {
      // Note that we check every byte's address
      result = machine->WriteMem( vaddr, 1, (int)(buf[n++]) );

      if ( !result ) {
	//translation failed
	return -1;
      }

      vaddr++;
    }

    return n;
}

void Create_Syscall(unsigned int vaddr, int len) {
    // Create the file with the name in the user buffer pointed to by
    // vaddr.  The file name is at most MAXFILENAME chars long.  No
    // way to return errors, though...
    char *buf = new char[len+1];	// Kernel buffer to put the name in

    if (!buf) return;

    if( copyin(vaddr,len,buf) == -1 ) {
	printf("%s","Bad pointer passed to Create\n");
	delete buf;
	return;
    }

    buf[len]='\0';

    fileSystem->Create(buf,0);
    delete[] buf;
    return;
}

int Open_Syscall(unsigned int vaddr, int len) {
    // Open the file with the name in the user buffer pointed to by
    // vaddr.  The file name is at most MAXFILENAME chars long.  If
    // the file is opened successfully, it is put in the address
    // space's file table and an id returned that can find the file
    // later.  If there are any errors, -1 is returned.
    char *buf = new char[len+1];	// Kernel buffer to put the name in
    OpenFile *f;			// The new open file
    int id;				// The openfile id

    if (!buf) {
	printf("%s","Can't allocate kernel buffer in Open\n");
	return -1;
    }

    if( copyin(vaddr,len,buf) == -1 ) {
	printf("%s","Bad pointer passed to Open\n");
	delete[] buf;
	return -1;
    }

    buf[len]='\0';

    f = fileSystem->Open(buf);
    delete[] buf;

    if ( f ) {
	if ((id = currentThread->space->fileTable.Put(f)) == -1 )
	    delete f;
	return id;
    }
    else
	return -1;
}

void Write_Syscall(unsigned int vaddr, int len, int id) {
    // Write the buffer to the given disk file.  If ConsoleOutput is
    // the fileID, data goes to the synchronized console instead.  If
    // a Write arrives for the synchronized Console, and no such
    // console exists, create one. For disk files, the file is looked
    // up in the current address space's open file table and used as
    // the target of the write.
    
    char *buf;		// Kernel buffer for output
    OpenFile *f;	// Open file for output

    if ( id == ConsoleInput) return;
    
    if ( !(buf = new char[len]) ) {
	printf("%s","Error allocating kernel buffer for write!\n");
	return;
    } else {
        if ( copyin(vaddr,len,buf) == -1 ) {
	    printf("%s","Bad pointer passed to to write: data not written\n");
	    delete[] buf;
	    return;
	}
    }

    if ( id == ConsoleOutput) {
      for (int ii=0; ii<len; ii++) {
	printf("%c",buf[ii]);
      }

    } else {
	if ( (f = (OpenFile *) currentThread->space->fileTable.Get(id)) ) {
	    f->Write(buf, len);
	} else {
	    printf("%s","Bad OpenFileId passed to Write\n");
	    len = -1;
	}
    }

    delete[] buf;
}

int Read_Syscall(unsigned int vaddr, int len, int id) {
    // Write the buffer to the given disk file.  If ConsoleOutput is
    // the fileID, data goes to the synchronized console instead.  If
    // a Write arrives for the synchronized Console, and no such
    // console exists, create one.    We reuse len as the number of bytes
    // read, which is an unnessecary savings of space.
    char *buf;		// Kernel buffer for input
    OpenFile *f;	// Open file for output

    if ( id == ConsoleOutput) return -1;
    
    if ( !(buf = new char[len]) ) {
	printf("%s","Error allocating kernel buffer in Read\n");
	return -1;
    }

    if ( id == ConsoleInput) {
      //Reading from the keyboard
      scanf("%s", buf);

      if ( copyout(vaddr, len, buf) == -1 ) {
	printf("%s","Bad pointer passed to Read: data not copied\n");
      }
    } else {
	if ( (f = (OpenFile *) currentThread->space->fileTable.Get(id)) ) {
	    len = f->Read(buf, len);
	    if ( len > 0 ) {
	        //Read something from the file. Put into user's address space
  	        if ( copyout(vaddr, len, buf) == -1 ) {
		    printf("%s","Bad pointer passed to Read: data not copied\n");
		}
	    }
	} else {
	    printf("%s","Bad OpenFileId passed to Read\n");
	    len = -1;
	}
    }

    delete[] buf;
    return len;
}

void Close_Syscall(int fd) {
    // Close the file associated with id fd.  No error reporting.
    OpenFile *f = (OpenFile *) currentThread->space->fileTable.Remove(fd);

    if ( f ) {
      delete f;
    } else {
      printf("%s","Tried to close an unopen file\n");
    }
}

/* New Syscalls
 * Acquire, Release, Wait, Signal, Broadcast
 *
 */

int CreateLock_Syscall(int vaddr) {
 //IS OK???
  // Return position in kernel structure array
  int size = 16;
  int max_chars = 20;

  //limit size of lock name
  if((size < 1) || (size > max_chars)) {
    DEBUG('q',"TOO MANY CHARS\n");
    return -1;
  }
  
  int addressSpaceSize = currentThread->space->NumPages() * PageSize;
  //in the clear to create the lock
  char *lockName = new char[size+1];
  lockName[size] = '\0';

  //make sure we aren't creating any part of the lock outside the alloted space
  if(vaddr < 0 || (vaddr+size) >= addressSpaceSize) {
    DEBUG('q',"OUT OF BOUNDS\n");
    return -1;
  }

  KernelLockTableLock->Acquire();

  // Make sure the table is not full 
  if(nextLockIndex >= MAX_LOCKS) {
    //The table is full of locks 
    KernelLockTableLock->Release();
    DEBUG('q',"LOCK TABLE FULL\n");
    return -1;
  }

  copyin(vaddr,size+1,lockName);

  //initialize important vars
  osLocks[nextLockIndex].lock = new Lock(lockName);
  osLocks[nextLockIndex].as = currentThread->space;
  //initialize lock to not in use
  osLocks[nextLockIndex].usageCounter = 0;
  //no one wants to destroy the lock right now
  osLocks[nextLockIndex].toBeDestroyed = FALSE;
  //report number of locks to user first...
  //increment number of locks
  nextLockIndex++;
  KernelLockTableLock->Release();
  return nextLockIndex-1;//-1 to get current lock index?
  // this may prove to have some problems if
  // we are context switched out before nextLockIndex is
  // returned
}

void DestroyLock_Syscall(int value) {
  // Delete from kernel structure array the lock object at position index
  int index = value;
  // Delete from kernel structure array the lock object at position index

  //First, acquire kernallocktablelock
  KernelLockTableLock->Acquire();
  
  //if the lock is in use, mark for destruction
  if(osLocks[index].usageCounter > 0){
    DEBUG('q',"CANNOT DESTROY LOCK IN USE\n");
    osLocks[index].toBeDestroyed = TRUE;
    KernelLockTableLock->Release();
    return;
  }
  
  //if the lock is already destined to be destroyed, return
  if((osLocks[index].usageCounter == 0) || (osLocks[index].toBeDestroyed == TRUE)){
    //destroy lock
    osLocks[index].toBeDestroyed = TRUE;
    //effectively destroy lock
    osLocks[index].lock = NULL;
    DEBUG('q',"DESTROYING LOCK\n");
    KernelLockTableLock->Release();
    return;
  }

}

void Acquire_Syscall(int index) {
  int value = index; // this is the value read by machine->readregister(4)
  KernelLockTableLock->Acquire();
  DEBUG('a',"ACQUIRING LOCK\n\n\n\n\n");
  // perform series of checks on the lock
  // to make sure user program is not doing
  // anything crazy

  //make sure the lock exists
  if(value < 0 || value >= nextLockIndex) {
    // This is a bad value
    DEBUG('q',"BAD VALUE\n");
    return;
  }

  KernelLock curLock = osLocks[value];
  if(curLock.lock == NULL) {
    // The lock has been destroyed 
    DEBUG('q',"LOCK HAS BEEN DESTROYED\n");
    return;
  }
  // The lock has not been destroyed
  
  if(curLock.as != currentThread->space) {
    // this lock belongs to a different process
    // since the address space of the lock does not match the 
    // current thread's address space
    DEBUG('q',"LOCK BELONGS TO DIFFERENT PROCESS");
    return;
  }
  //ensure that lock isn't destroyed while in use
  curLock.usageCounter++; 

  //has to go above acquire to avoid deadlock
  KernelLockTableLock->Release();
    // FINALLY...Acquire the lock
  curLock.lock->Acquire();

}

void Release_Syscall(int index) {
  //NOTE: I just made this the same checks as Acquire for now....
  int value = index;
  KernelLockTableLock->Acquire();
  //make sure the lock exists
  if(value < 0 || value >= nextLockIndex) {
    // This is a bad value
    DEBUG('q',"BAD VALUE\n");
    return;
  }
  KernelLock curLock = osLocks[index];
  if(curLock.lock == NULL) {
    // The lock has been destroyed 
    DEBUG('q',"LOCK HAS BEEN DESTROYED\n");
    return;
  }
  
  if(curLock.as != currentThread->space) {
    // this lock belongs to a different process
    // since the address space of the lock does not match the 
    // current thread's address space    
    DEBUG('q',"LOCK BELONGS TO DIFFERENT PROCESS");
    return;
  }
  //ensure that lock isn't destroyed while in use


  //has to go above acquire to avoid deadlock
  KernelLockTableLock->Release();
    // FINALLY...Release the lock
  curLock.lock->Release();
  curLock.usageCounter--;

}

int CreateCondition_Syscall(int vaddr) {
  //copied from CreateLock, IS OK???
  // Return position in kernel structure array
  int size = 16;
  int max_chars = 20;

  //limit size of condition name
  if((size < 1) || (size > max_chars)) {
    DEBUG('q',"TOO MANY CHARS\n");
    return -1;
  }
  
  int addressSpaceSize = currentThread->space->NumPages() * PageSize;
  //in the clear to create the condition
  char *condName = new char[size+1];
  condName[size] = '\0';

  //NO creating any part of the condition outside the alloted space
  if(vaddr < 0 || (vaddr+size) >= addressSpaceSize) {
    DEBUG('q',"OUT OF BOUNDS\n");
    return -1;
  }

  KernelCondTableLock->Acquire();

  // Make sure the table is not full 
  if(nextCondIndex >= MAX_CONDS) {
    //The table is full of locks 
    KernelCondTableLock->Release();
    DEBUG('q',"COND TABLE FULL\n");
    return -1;
  }

  copyin(vaddr,size+1,condName);

  //initialize important vars
  osConds[nextCondIndex].condition = new Condition(condName);
  osConds[nextCondIndex].as = currentThread->space;
  //initialize condition to not in use
  osConds[nextCondIndex].usageCounter = 0;
  //no one wants to destroy the lock right now
  osConds[nextCondIndex].toBeDestroyed = FALSE;
  //report number of locks to user first...
  //increment number of locks
  nextCondIndex++;
  KernelCondTableLock->Release();
  return nextCondIndex-1;//-1 to get current condition index?
  // this may prove to have some problems if
  // we are context switched out before nextCondIndex is
  // returned 
}

void DestroyCondition_Syscall(int index) {
  // Delete from kernel structure array the condition object at position index 
// Delete from kernel structure array the condition object at position index 
  //First, acquire kernalcondtablelock
  KernelCondTableLock->Acquire();
  
  //if the condition is in use, mark for destruction
  if(osConds[index].usageCounter > 0){
    DEBUG('q',"CANNOT DESTROY CONDITION IN USE\n");
    osLocks[index].toBeDestroyed = TRUE;
    KernelCondTableLock->Release();
    return;
  }
  
  //if the lock is already destined to be destroyed, return
  if((osConds[index].usageCounter == 0) || (osConds[index].toBeDestroyed == TRUE)){
    //destroy condition
    osConds[index].toBeDestroyed = TRUE;
    //effectively destroy lock
    osConds[index].condition = NULL;
    DEBUG('q',"DESTROYING CONDITION");
    KernelCondTableLock->Release();
    return;
  }

}

void Wait_Syscall(int index, int lock_id) {
  KernelCondTableLock->Acquire();
  //VALIDATE CONDITION
  //make sure the condition exists
  if(index < 0 || index >= nextCondIndex) {
    // This is a bad value
    DEBUG('q',"BAD VALUE\n");
    return;
  }
  KernelCond curCond = osConds[index];
  if(curCond.condition == NULL) {
    // The condition has been destroyed 
    DEBUG('q',"COND HAS BEEN DESTROYED\n");
    return;
  }
  // The condition has not been destroyed
  if(curCond.as != currentThread->space) {
    // this condition belongs to a different process
    // since the address space of the condition does not match the 
    // current thread's address space
    DEBUG('q',"CONDITION BELONGS TO DIFFERENT PROCESS\n");
    return;
  }
  //ensure that condition isn't destroyed while in use
  curCond.usageCounter++; 
  KernelCondTableLock->Release();
  KernelLockTableLock->Acquire();
  //VALIDATE LOCK
  //make sure the lock exists
  if(lock_id < 0 || lock_id >= nextLockIndex) {
    // This is a bad value
    DEBUG('q',"BAD VALUE\n");
    return;
  }

  KernelLock curLock = osLocks[lock_id];
  if(curLock.lock == NULL) {
    // The lock has been destroyed 
    DEBUG('q',"LOCK HAS BEEN DESTROYED\n");
    return;
  }
  // The lock has not been destroyed
  
  if(curLock.as != currentThread->space) {
    // this lock belongs to a different process
    // since the address space of the lock does not match the 
    // current thread's address space
    DEBUG('q',"LOCK BELONGS TO DIFFERENT PROCESS\n");
    return;
  }
  //ensure that lock isn't destroyed while in use
  curLock.usageCounter++;
  DEBUG('q',"CONDITION WAITING");
  //has to go above acquire to avoid deadlock
  KernelLockTableLock->Release();
  // FINALLY...use wait on the lock
  curCond.condition->Wait(curLock.lock);
}

void Signal_Syscall(int index, int lock_id) {
  KernelCondTableLock->Acquire();
  //VALIDATE CONDITION
  //make sure the condition exists
  if(index < 0 || index >= nextCondIndex) {
    // This is a bad value
    DEBUG('q',"BAD VALUE\n");
    return;
  }
  KernelCond curCond = osConds[index];
  if(curCond.condition == NULL) {
    // The condition has been destroyed 
    DEBUG('q',"COND HAS BEEN DESTROYED\n");
    return;
  }
  // The condition has not been destroyed
  if(curCond.as != currentThread->space) {
    // this condition belongs to a different process
    // since the address space of the condition does not match the 
    // current thread's address space
    DEBUG('q',"CONDITION BELONGS TO DIFFERENT PROCESS\n");
    return;
  }
  //ensure that condition isn't destroyed while in use
  curCond.usageCounter--; 
  KernelCondTableLock->Release();
  KernelLockTableLock->Acquire();
  //VALIDATE LOCK
  //make sure the lock exists
  if(lock_id < 0 || lock_id >= nextLockIndex) {
    // This is a bad value
    DEBUG('q',"BAD VALUE\n");
    return;
  }

  KernelLock curLock = osLocks[lock_id];
  if(curLock.lock == NULL) {
    // The lock has been destroyed 
    DEBUG('q',"LOCK HAS BEEN DESTROYED\n");
    return;
  }
  // The lock has not been destroyed
  
  if(curLock.as != currentThread->space) {
    // this lock belongs to a different process
    // since the address space of the lock does not match the 
    // current thread's address space
    DEBUG('q',"LOCK BELONGS TO DIFFERENT PROCESS\n");
    return;
  }
  //ensure that lock isn't destroyed while in use
  curLock.usageCounter--;
  DEBUG('q',"CONDITION SIGNAL");
  //has to go above acquire to avoid deadlock
  KernelLockTableLock->Release();
  // FINALLY...use signal on the lock
  curCond.condition->Signal(curLock.lock);

}

void Broadcast_Syscall(int index, int lock_id) {
  KernelCondTableLock->Acquire();
  //VALIDATE CONDITION
  //make sure the condition exists
  if(index < 0 || index >= nextCondIndex) {
    // This is a bad value
    DEBUG('q',"BAD VALUE\n");
    return;
  }
  KernelCond curCond = osConds[index];
  if(curCond.condition == NULL) {
    // The condition has been destroyed 
    DEBUG('q',"COND HAS BEEN DESTROYED\n");
    return;
  }
  // The condition has not been destroyed
  if(curCond.as != currentThread->space) {
    // this condition belongs to a different process
    // since the address space of the condition does not match the 
    // current thread's address space
    DEBUG('q',"CONDITION BELONGS TO DIFFERENT PROCESS\n");
    return;
  }
  //ensure that condition isn't destroyed while in use
  int temp = curCond.usageCounter;
  curCond.usageCounter--; 
  KernelCondTableLock->Release();
  KernelLockTableLock->Acquire();
  //VALIDATE LOCK
  //make sure the lock exists
  if(lock_id < 0 || lock_id >= nextLockIndex) {
    // This is a bad value
    DEBUG('q',"BAD VALUE\n");
    return;
  }

  KernelLock curLock = osLocks[lock_id];
  if(curLock.lock == NULL) {
    // The lock has been destroyed 
    DEBUG('q',"LOCK HAS BEEN DESTROYED\n");
    return;
  }
  // The lock has not been destroyed
  
  if(curLock.as != currentThread->space) {
    // this lock belongs to a different process
    // since the address space of the lock does not match the 
    // current thread's address space
    DEBUG('q',"LOCK BELONGS TO DIFFERENT PROCESS\n");
    return;
  }
  //ensure that lock isn't destroyed while in use
  if(curLock.usageCounter - temp >= 0){
    curLock.usageCounter = curLock.usageCounter - temp;
  }else{
    curLock.usageCounter = 0;
  }
  DEBUG('q',"CONDITION BROADCAST");
  //has to go above acquire to avoid deadlock
  KernelLockTableLock->Release();
  // FINALLY...use broadcast on the lock
  curCond.condition->Broadcast(curLock.lock);
}

void Yield_Syscall() {
  currentThread->Yield();
}

void Exit_Syscall(int status) {
  //TODO - finish this
  //interrupt->Halt();

  //cout<<"id of current thread= "<<currentThread->getMyId()<<endl;

  currentThread->Finish();
  
  int i, spaceId_f;
  // get the space id for this new thread
  /*
  for(i=0; i<64; i++) {
    if(processTable[i].as == currentThread->space) {
      spaceId_f = i;
      break;
    } else { 
      // Trying to exit a thread without an existing address space
      DEBUG('a', "Trying to exit a thread without an existing address space\n");
      interrupt->Halt();
    }
  }
  */
  //cout<<"num child processes: "<<processTable[spaceId_f].numChildProcess<<endl;
  if(processTable[spaceId_f].numChildProcess > 0) {
    // do something
  }

}

void Print_Syscall() {

}

void execThread() {
  DEBUG('f',"running the thread 1\n");
  currentThread->space->InitRegisters();
  DEBUG('f',"running the thread 2 \n");
  currentThread->space->RestoreState();
  machine->Run();
  DEBUG('f',"running the thread\n");
}

void kernelFunc(int vaddr) {
  int i, spaceId;  
  spaceId = currentThread->space->id;
  // write to register PCReg the virtual address
  machine->WriteRegister(PCReg, vaddr);
  
  // write virtual address + 4 in NextPCReg
  machine->WriteRegister(NextPCReg, (vaddr+4));
  // call RestoreState function
  currentThread->space->RestoreState();
  // write to stack register, the starting position of the stack
  DEBUG('g', "kernel func: space id: %d \n",spaceId);
  DEBUG('h',"current Thread space size %d",currentThread->space->NumPages());
  machine->WriteRegister(StackReg,currentThread->stackLoc);
  //printf("stack location: %d\n", processTable[spaceId].stackLocation);

  machine->Run();
}

void ExceptionHandler(ExceptionType which) {
    int type = machine->ReadRegister(2); // Which syscall?
    int rv = 0;
    if ( which == SyscallException ) {
	switch (type) {
	    default:
		DEBUG('a', "Unknown syscall - shutting down.\n");
	    case SC_Halt:
		DEBUG('a', "Shutdown, initiated by user program.\n");
		interrupt->Halt();
		break;
	    case SC_Create:
		DEBUG('a', "Create syscall.\n");
		Create_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
		break;
	    case SC_Open:
		DEBUG('a', "Open syscall.\n");
		rv = Open_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
		break;
	    case SC_Write:
		DEBUG('a', "Write syscall.\n");
		Write_Syscall(machine->ReadRegister(4),
			      machine->ReadRegister(5),
			      machine->ReadRegister(6));
		break;
	    case SC_Read:
		DEBUG('a', "Read syscall.\n");
		rv = Read_Syscall(machine->ReadRegister(4),
			      machine->ReadRegister(5),
			      machine->ReadRegister(6));
		break;
	    case SC_Close:
		DEBUG('a', "Close syscall.\n");
		Close_Syscall(machine->ReadRegister(4));
		break;
	    case SC_CreateLock:
	        DEBUG('a', "CreateLock syscall.\n");
	        rv = CreateLock_Syscall(machine->ReadRegister(4));
		break;
	    case SC_DestroyLock:
	        DEBUG('a', "DestroyLock syscall.\n");
	        DestroyLock_Syscall(machine->ReadRegister(4));
		break;
	    case SC_Acquire:
		DEBUG('a', "Close syscall.\n");
		Acquire_Syscall(machine->ReadRegister(4));
		break;
	    case SC_Release:
		DEBUG('a', "Release syscall.\n");
		Release_Syscall(machine->ReadRegister(4));
		break;
	    case SC_CreateCondition:
	        DEBUG('a', "CreateCondition syscall.\n");
	        rv = CreateCondition_Syscall(machine->ReadRegister(4));
		break;
	    case SC_DestroyCondition:
	        DEBUG('a', "DestroyCondition syscall.\n");
	        DestroyCondition_Syscall(machine->ReadRegister(4));
		break;
	    case SC_Wait:
		DEBUG('a', "Wait syscall.\n");
		Wait_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
		break;
	    case SC_Signal:
		DEBUG('a', "Signal syscall.\n");
		Signal_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
		break;
	    case SC_Broadcast:
		DEBUG('a', "Broadcast syscall.\n");
		Broadcast_Syscall(machine->ReadRegister(4), machine->ReadRegister(5));
		break;
	    case SC_Yield:
	        DEBUG('a', "Yield syscall. \n");
		Yield_Syscall();
		break;
	    case SC_Exit:
	        DEBUG('a', "Exit syscall.\n");
		Exit_Syscall(machine->ReadRegister(4));
		break;
	    case SC_Fork:
	        DEBUG('a', "Fork syscall.\n");
		// get the virtual address of function being forked
		int virtualAddress;
		virtualAddress = machine->ReadRegister(4);
		int i, spaceId_f;
		// get the space id for this new thread
		spaceId_f = currentThread->space->id;
		
		DEBUG('g',"fork: space id : %d\n", spaceId_f);
		Thread *kernelThread = new Thread("kernelThread");
		// this is the same as the currentThread->space b/c this thread
		// is a child of the currentThread
		//printf("address space num pages %d \n", currentThread->space->NumPages());
		kernelThread->space = processTable[spaceId_f].as;

		// Create a new page table with 8 pages more of stack
		kernelThread->space->NewPageTable();
		kernelThread->stackLoc = (kernelThread->space->NumPages()*PageSize)-16;

		if(processTable[spaceId_f].as == currentThread->space) {
		  //printf("process table address space pointer is equal to current thread\n");
		}
		// Update process table
		// DEBUG('g',"address space address: %d \n",&(processTable[spaceId_f].as));
		processTable[spaceId_f].stackLocation = (kernelThread->space->NumPages()*PageSize)-16;
		//printf("address space num pages %d \n", currentThread->space->NumPages());

		//printf("stack location: %d\n", processTable[spaceId_f].stackLocation);
		processTable[spaceId_f].numChildProcess++;
		kernelThread->Fork(kernelFunc,virtualAddress);
		
		break;
	    case SC_Exec:
	        DEBUG('a',"Exec syscall. \n");
		
		int virtualAddress_e, physicalAddress_e; 
		char* filename;
		// Get the virtual address for the name of the process
		virtualAddress_e = machine->ReadRegister(4);
		char *buf = new char[16+1];	// Kernel buffer to put the name in
		OpenFile *f;			// The new open file
		int id;				// The openfile id
		
		if (!buf) {
		  DEBUG('a',"Can't allocate kernel buffer in Open\n");
		 
		}
		
		if( copyin(virtualAddress_e,16,buf) == -1 ) {
		  DEBUG('a',"Bad pointer passed to Open\n");
		  delete[] buf;
		}
		
		buf[16]='\0';

		// Print out filename
		// printf("%s\n",buf);

		f = fileSystem->Open(buf);
		if(f == NULL) {
		  printf("%s","unable to open file\n");
		  
		} else {

		  AddrSpace *space;
		  DEBUG('a',"Got the file open\n");
		  
		  // create a new address space for this executable file
		  space = new AddrSpace(f);
		  Thread *executionThread = new Thread("executionThread");
		  // Allocate the address space to this thread
		  executionThread->space = space;
		  
		  DEBUG('a',"allocated the address space\n");
		  
		  // Update process table
		  int g, spaceId;
		  for(g = 0; g < 64; g++) {
		    if(!processTable[g].inUse) {
		      spaceId = g;
		      // Set the appropriate address space
		      processTable[spaceId].as = space;
		      processTable[spaceId].stackLocation = (space->NumPages()*PageSize) - 16;
		      processTable[spaceId].inUse = TRUE;
		      break;
		    }
		  }
		  space->id = spaceId; 
		  DEBUG('g',"space id: %d \n",spaceId);
		  
		  DEBUG('a',"Updated the process table with new process\n");
		  
		  // Write the space id to register 2
		  rv = spaceId;
		  // Fork the thread
		  executionThread->Fork((VoidFunctionPtr)execThread,0);
		}
		break;
	case SC_Print:
	  int virtualAddress_p;
	  int p1, p2, p3;
	  char *buf_p = new char[168+1];
	  
	  virtualAddress_p = machine->ReadRegister(4);
	  p1 = machine->ReadRegister(5);
	  p2 = machine->ReadRegister(6);
	  p3 = machine->ReadRegister(7);
	  if(!buf) {
	    DEBUG('a',"Can't allocate kernel buffer in Print\n");
	  }
	  
	  if(copyin(virtualAddress_p, 128, buf_p)==-1) {
	    DEBUG('a',"Bad pointer passed to Print\n");
	    delete[] buf;
	  }
	  buf_p[128] = '\0';
	  printf(buf_p,p1,p2,p3);
	  break;
	}

	// Put in the return value and increment the PC
	machine->WriteRegister(2,rv);
	machine->WriteRegister(PrevPCReg,machine->ReadRegister(PCReg));
	machine->WriteRegister(PCReg,machine->ReadRegister(NextPCReg));
	machine->WriteRegister(NextPCReg,machine->ReadRegister(PCReg)+4);
	return;
    } else {
      cout<<"Unexpected user mode exception - which:"<<which<<"  type:"<< type<<endl;
      interrupt->Halt();
    }
}
