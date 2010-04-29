// Project3Server.cc

#include "system.h"
#include "network.h"
#include "post.h"
#include "interrupt.h"
#include <sstream>
#include <string>
//#include <stdio.h>
//#include <stdlib.h>
#include "ServerLock.cc"
#include "ServerCond.cc"

enum RequestType { UNKNOWN, CREATE_LOCK, ACQUIRE, RELEASE, DESTROY_LOCK, CREATE_CONDITION, WAIT, SIGNAL, BROADCAST, REGISTER, CREATE_MV, GET, SET };

struct ServerMV {
  int value;
  string name;
};

struct ServerToken {
  string name;
};

int MAX_SERVER_MVs = 1000; //the max number of server MVs
int MAX_SERVER_TOKENS = 1000;

RequestType getRequestType(char* req);

ServerLock *serverLockTable[1000];
ServerCond *serverCondTable[1000];
ServerMV serverMVTable[1000];
ServerToken serverTokenTable[1000];


int numServerLocks;
int numServerConds;
int numServerMVs;
int numServerTokens;
int numMembers;
int maxNumMembers;

struct Member {
  int machineNum;
  int mailboxNum;
};
Member members[1000];

int TestCreateLock();
void TestAcquireLock(int theLockID);
void TestReleaseLock(int theLockID);
void TestDestroyLock(int theLockID);
int TestCreateCondition();
void TestWait(int theCondID, int theLockID);
void TestSignal(int theCondID, int theLockID);
void TestBroadcast(int theCondID, int theLockID);
void TestRegister();
int TestCreateMV();
void TestGet(int theMVID);
void TestSet(int theMVID, int theValue);

void StartProject3Server(int numberOfMembers) {

  maxNumMembers = numberOfMembers;
  

  stringstream ss;

  PacketHeader outPktHdr, inPktHdr;
  MailHeader outMailHdr, inMailHdr;
  char buffer[MaxMailSize];

  char* param = new char;
  char* param2 = new char; //second param, if needed
  char* request = new char;
  char* response = new char;

  

  Lock *ServerLockTableLock = new Lock("Server Lock Table Lock");
  //ServerLock serverLockTable[];
  numServerLocks = 0;
  Lock *ServerCondTableLock = new Lock("Server Cond Table Lock");
  numServerConds = 0;
  
  Lock *ServerTokenTableLock = new Lock("Server Token Table Lock");
  numServerTokens = 0;

  Lock *ServerMVTableLock = new Lock("Server MV Table Lock");
  numServerMVs = 0;

  Lock *MemberTableLock = new Lock("Member Table Lock");

  printf("Starting Server. Waiting for %d members.\n", maxNumMembers);

  while(true) {
    //printf("Starting Server\n");

    postOffice->Receive(0, &inPktHdr, &inMailHdr, buffer);
    printf("Server got \"%s\" from %d, box %d\n",buffer,inPktHdr.from,inMailHdr.from);
    fflush(stdout);

    ss.clear();
    ss.str(buffer);

    outPktHdr.to = inPktHdr.from;
    outMailHdr.to = inMailHdr.from;

    outMailHdr.from = 0;

    LockOwner newOwner;
    newOwner.machineID = inPktHdr.from;
    newOwner.mailboxNum = inMailHdr.from;

    CondOwner newCondOwner;
    newCondOwner.machineID = inPktHdr.from;
    newCondOwner.mailboxNum = inMailHdr.from;
    
    ss >> request;
    ss >> param;

    printf("req: %s\n", request);
    printf("param: %s\n", param);

    bool sendResponse = false;
    bool error = false;
    bool mvExists = false;
    bool tokenExists = false;

    string mvName = "";
    string tokenName = "";

    RequestType r = getRequestType(request);

    KernelLock curLock;
    int lockID = -1;
    int condID = -1;
    int mvID = -2;
    int mvValue = -1;

    switch(r) {

    case CREATE_LOCK:
      printf("Server received Create Token request. Token name = %s.\n", param);
      tokenName = param;
      ss.clear();
      //create the lock and return the lock ID
      
      ServerTokenTableLock->Acquire();

      if(numServerTokens >= MAX_SERVER_TOKENS) {
	//server lock table full
	ServerTokenTableLock->Release();
	DEBUG('q',"Server Lock Table Full\n");
	error = true;
	break;
      }

      for(int i = 0; i < numServerTokens; i++) {
	if(serverTokenTable[i].name == tokenName) {
	  //token already exists w/ same name
	  //printf("A token with the name \"%s\" already exists with id=%d\n",tokenName,i);
	  lockID = i;
	  tokenExists = true;
	  break;
	}
      }

      if(!tokenExists) {

	serverTokenTable[numServerTokens].name = tokenName;

	lockID = numServerTokens;
	numServerTokens++;

	sprintf(response, "%d", lockID); //send the client the lock ID
      } else {
	//the token already exists
	sprintf(response, "%d dup", lockID); //send the client the lock ID and tell it that the token already exists
      }

      ServerTokenTableLock->Release();
      
      //sprintf(response, "%d", lockID); //send the client the lock ID
      sendResponse = true;

      /*  PROJECT 3 CREATE LOCK HERE
      printf("Server received Create Lock request. Lock name = %s.\n", param);
      ss.clear();
      //create the lock and return the lock ID
      
      ServerLockTableLock->Acquire();

      if(numServerLocks >= MAX_SERVER_LOCKS) {
	//server lock table full
	ServerLockTableLock->Release();
	DEBUG('q',"Server Lock Table Full\n");
	error = true;
	break;
      }

      ServerLock *newLock = new ServerLock(numServerLocks);
      serverLockTable[numServerLocks] = newLock;

      lockID = numServerLocks;
      numServerLocks++;
      ServerLockTableLock->Release();
      
      sprintf(response, "%d", lockID); //send the client the lock ID
      sendResponse = true;

      */

      /*
      KernelLockTableLock->Acquire();

      if(nextLockIndex >= MAX_LOCKS) {
	KernelLockTableLock->Release();
	DEBUG('q', "LOCK TABLE FULL\n");
	//return -1
      }
      
      osLocks[nextLockIndex].lock = new Lock(param);
      //osLocks]nextLockIndex].as = currentThread->space;
      osLocks[nextLockIndex].usageCounter = 0;
      osLocks[nextLockIndex].toBeDestroyed = FALSE;
      nextLockIndex++;
      KernelLockTableLock->Release();
      lockID = nextLockIndex-1;

      //itoa(lockID, response, 10);
      sprintf(response, "%d", lockID);

      //response = "ok";  //TODO - put the lock ID here
      //outMailHdr.length = sizeof(response) + 1;
      sendResponse = true;
      */
      break;

    case ACQUIRE:
      printf("Server received Acquire request. Lock ID = %s.\n",param);
      ss.clear();
      ss.str(param);
      ss>>lockID;
      //printf("lock id: %d\n",lockID);

      ServerLockTableLock->Acquire();
      if(lockID < 0 || lockID >= numServerLocks) {
	//this is a bad value
	DEBUG('q',"BAD VALUE\n");
	error = true;
	break;
      }
      ServerLockTableLock->Release();
      

      serverLockTable[lockID]->Acquire(newOwner);

      


      /*
      
      KernelLockTableLock->Acquire();
      DEBUG('a',"ACQUIRING LOCK\n");

      if(lockID < 0 || lockID >= nextLockIndex) {
	//this is a bad value
	DEBUG('q', "BAD VALUE\n");
	//return
	error = true;
	break;
      }

      curLock = osLocks[lockID];
      if(curLock.lock == NULL) {
	//the lock has been destroyed
	DEBUG('q',"LOCK HAS BEEN DESTROYED\n");
	//return;
	error = true;
	break;
      }

      curLock.usageCounter++;
      DEBUG('q',"ACQUIRING LOCK\n");
      KernelLockTableLock->Release();
      curLock.lock->Acquire();

      response = "ok";
      sendResponse = true;
      */
      break;

    case RELEASE:
      printf("Server received Release request. Lock ID = %s.\n",param);
      ss.clear();
      ss.str(param);
      ss>>lockID;
      //printf("lock id: %d\n",lockID);

      ServerLockTableLock->Acquire();
      if(lockID < 0 || lockID >= numServerLocks) {
	//this is a bad value
	DEBUG('q',"BAD VALUE\n");
	//error = true;
	break;
      }
      ServerLockTableLock->Release();

      serverLockTable[lockID]->Release(newOwner);

      sprintf(response, "%d", lockID); //send the client the released lock ID (just to confirm that there was no error)
      
      sendResponse = true;

      /*
      KernelLockTableLock->Acquire();
      if(lockID < 0 || lockID >= nextLockIndex) {
	//this is a bad value
	DEBUG('q',"BAD VALUE\n");
	//return;
	error = true;
	break;
      }

      curLock = osLocks[lockID];
      if(curLock.lock == NULL) {
	// the lock has been destroyed
	DEBUG('q',"LOCK HAS BEEN DESTROYED\n");
	//return;
	error = true;
	break;
      }

      DEBUG('q',"RELEASING LOCK\n");
      
      KernelLockTableLock->Release();
      curLock.lock->Release();
      curLock.usageCounter--;

      response = "ok";
      sendResponse = true;
      */

      break;

    case DESTROY_LOCK:
      printf("Server received Destroy Lock request. Lock ID = %s.\n",param);
      ss.clear();
      ss.str(param);
      ss>>lockID;
      //printf("lock id: %d\n",lockID);

      ServerLockTableLock->Acquire();
      if(lockID < 0 || lockID >= numServerLocks) {
	//this is a bad value
	DEBUG('q',"BAD VALUE\n");
	//return;
	//error = true;
	break;
      }

      ServerLockTableLock->Release();
      
      serverLockTable[lockID]->Destroy();

      /*
      curLock = osLocks[lockID];

      //if lock is in use, mark it for destruction
      if(curLock.usageCounter > 0) {
	DEBUG('q',"CANNOT DESTROY LOCK IN USE\n");
	curLock.toBeDestroyed = TRUE;
	KernelLockTableLock->Release();
	//return;
	response = "ok"; //change to something else?
	sendResponse = true;
	break;
      }

      //if the lock is already destined to be destroyed, return
      if(curLock.usageCounter == 0 || curLock.toBeDestroyed == TRUE) {
	//destroy lock
	curLock.toBeDestroyed = TRUE;
	//effectively destroy lock
	curLock.lock = NULL;
	DEBUG('q',"DESTROYING LOCK\n");
	KernelLockTableLock->Release();
	response = "ok";
	sendResponse = true;
	break;
      }
      
      //should never get to this point
      error = true;
      */

      break;

    case CREATE_CONDITION:
      printf("Server received Create Condition request. Condition Name = %s.\n",param);

      ServerCondTableLock->Acquire();

      if(numServerLocks >= MAX_SERVER_CONDS) {
	//server cond table full
	ServerCondTableLock->Release();
	DEBUG('q',"Server Condition Table Full\n");
	error = true;
	break;
      }

      ServerCond *newCond = new ServerCond(numServerConds);
      serverCondTable[numServerConds] = newCond;

      condID = numServerConds;
      numServerConds++;
      ServerCondTableLock->Release();
      
      sprintf(response, "%d", condID); //send the client the cond ID
      sendResponse = true;

      /*

      KernelCondTableLock->Acquire();

      int myCondIndex = nextCondIndex;
      
      //make sure the table is not full
      if(nextCondIndex >= MAX_CONDS) {
	//the table is full
	KernelCondTableLock->Release();
	DEBUG('q',"COND TABLE FULL\n");
	//return -1;
	error = true;
	break;
      }

      osConds[nextCondIndex].condition = new Condition("");
      //osConds[nextCondIndex].as = currentThread->space;
      osConds[nextCondIndex].usageCounter = 0;
      osConds[nextCondIndex].toBeDestroyed = FALSE;
      
      nextCondIndex++;
      KernelCondTableLock->Release();
      
      sprintf(response, "%d", myCondIndex);
      sendResponse = true;

      */
      break;

    case WAIT:
      ss>>param2; //get 2nd parameter (lock ID)
      printf("Server received Wait request. Condition ID = %s. Lock ID = %s\n",param, param2);

      ss.clear();
      ss.str(param);
      ss>>condID;  //1st parameter is the condition ID

      ss.clear();
      ss.str(param2);
      ss>>lockID; //2nd parameter is the lock ID
      
      //printf("cond id: %d\n",condID);
      //printf("lock id: %d\n",lockID);

      //validate the condition ID
      ServerCondTableLock->Acquire();
      if(condID < 0 || condID >= numServerConds) {
	//bad cond ID
	DEBUG('q',"BAD VALUE\n");
	error = true;
	break;
      }
      ServerCondTableLock->Release();
      
      //validate the lock ID
      ServerLockTableLock->Acquire();
      if(lockID < 0 || lockID >= numServerLocks) {
	//bad lock ID
	DEBUG('q',"BAD VALUE\n");
	error = true;
	break;
      }
      ServerLockTableLock->Release();

      
      

      //make sure the client owns the given lock ID - ?

      //wait
      newCondOwner.lockID = lockID;

      printf("server: before wait: machine: %d, box: %d\n",newCondOwner.machineID, newCondOwner.mailboxNum);
      serverCondTable[condID]->Wait(newCondOwner);

      //release the lock while waiting -- need to check for ownership before releasing??
      serverLockTable[lockID]->Release(newOwner);

      break;

    case SIGNAL:
      
      ss>>param2; //get 2nd parameter (lock ID)
      printf("Server received Signal request. Condition ID = %s. Lock ID = %s\n",param, param2);

      ss.clear();
      ss.str(param);
      ss>>condID;  //1st parameter is the condition ID

      ss.clear();
      ss.str(param2);
      ss>>lockID; //2nd parameter is the lock ID
      
      //printf("cond id: %d\n",condID);
      //printf("lock id: %d\n",lockID);

      //validate the condition ID
      ServerCondTableLock->Acquire();
      if(condID < 0 || condID >= numServerConds) {
	//bad cond ID
	DEBUG('q',"BAD VALUE\n");
	//error = true;
	break;
      }
      ServerCondTableLock->Release();
      
      //validate the lock ID
      ServerLockTableLock->Acquire();
      if(lockID < 0 || lockID >= numServerLocks) {
	//bad lock ID
	DEBUG('q',"BAD VALUE\n");
	//error = true;
	break;
      }

      ServerLockTableLock->Release();
      
      //get the client to wake up
      ClientAddr clientToWake = serverCondTable[condID]->Signal(newCondOwner);

      printf("client to wake --> machineID: %d, mailboxNum: %d\n",clientToWake.machineID, clientToWake.mailboxNum);
      
      if(clientToWake.machineID != -1 && clientToWake.mailboxNum != -1) {
	//send a message to the client to wake
	//acquire the lock (the Acquire will send the message to the client)

	LockOwner clientToWake_L;
	clientToWake_L.machineID = clientToWake.machineID;
	clientToWake_L.mailboxNum = clientToWake.mailboxNum;
	
	printf("client to wake (from LockOwner)--> machineID: %d, mailboxNum: %d\n",clientToWake_L.machineID, clientToWake_L.mailboxNum);
	
	serverLockTable[lockID]->Acquire(clientToWake_L);
	
      } else {
	//no one was waiting on the CV
	//do nothing
      }

      break;

    case BROADCAST:
      
      ss>>param2; //get 2nd parameter (lock ID)
      printf("Server received Broadcast request. Condition ID = %s. Lock ID = %s\n",param, param2);

      ss.clear();
      ss.str(param);
      ss>>condID;  //1st parameter is the condition ID

      ss.clear();
      ss.str(param2);
      ss>>lockID; //2nd parameter is the lock ID
      
      //printf("cond id: %d\n",condID);
      //printf("lock id: %d\n",lockID);

      //validate the condition ID
      ServerCondTableLock->Acquire();
      if(condID < 0 || condID >= numServerConds) {
	//bad cond ID
	DEBUG('q',"BAD VALUE\n");
	//error = true;
	break;
      }
      ServerCondTableLock->Release();
      
      //validate the lock ID
      ServerLockTableLock->Acquire();
      if(lockID < 0 || lockID >= numServerLocks) {
	//bad lock ID
	DEBUG('q',"BAD VALUE\n");
	error = true;
	break;
      }

      ServerLockTableLock->Release();

      ClientAddr* clientsToWake = serverCondTable[condID]->Broadcast();

      for(int i=0; i < sizeof(clientsToWake); i++) {
      
	//get the client to wake up
	ClientAddr clientToWake = clientsToWake[i];

	printf("client to wake --> machineID: %d, mailboxNum: %d\n",clientToWake.machineID, clientToWake.mailboxNum);
      
	if(clientToWake.machineID != -1 && clientToWake.mailboxNum != -1) {
	  //send a message to the client to wake
	  //acquire the lock (the Acquire will send the message to the client)

	  LockOwner clientToWake_L;
	  clientToWake_L.machineID = clientToWake.machineID;
	  clientToWake_L.mailboxNum = clientToWake.mailboxNum;
	
	  printf("client to wake (from LockOwner)--> machineID: %d, mailboxNum: %d\n",clientToWake_L.machineID, clientToWake_L.mailboxNum);
	
	  serverLockTable[lockID]->Acquire(clientToWake_L);
	
	} else {
	  //no one was waiting on the CV
	  //do nothing
	}

      } //for
      
      break;

    case REGISTER:
      printf("Server received a registration message (join) request from machine %d, box %d.\n", inPktHdr.from, inMailHdr.from);
      
      //TODO - add the member's info to a list or something
      
      MemberTableLock->Acquire();

      members[numMembers].machineNum = inPktHdr.from;
      members[numMembers].mailboxNum = inMailHdr.from;

      numMembers++;
      if(numMembers == maxNumMembers) {
	printf("Reached the desired number of members (%d).\n", maxNumMembers);
	//Send message to each member containing a list of the other members.
	string memberListStr = "X";
	ss.clear();
	//ss << ".";
	printf("Member List Generated:\n");
	for(int i = 0; i < numMembers; i++) {
	  memberListStr.append(" ");
	  char tempBuf[10];
	  sprintf(tempBuf,"%d",members[i].machineNum);
	  memberListStr.append(tempBuf);
	  memberListStr.append(",");
	  sprintf(tempBuf,"%d",members[i].mailboxNum);
	  memberListStr.append(tempBuf);
	  //memberListStr.append(" ");
	  printf("   Machine: %d, Mailbox: %d\n", members[i].machineNum, members[i].mailboxNum);
	  //ss << members[i].machineNum << "," << members[i].mailboxNum << " ";
	}
	//ss>>memberListStr;
	//memberListStr.append(">");
	//cout<<"member list: "<<memberListStr<<endl;

	char* memberListToSend = new char[memberListStr.size() + 1];
	strcpy(memberListToSend, memberListStr.c_str());

	//cout<<"member list to send: "<<memberListToSend<<endl;
	//cout<<"size of member list: "<<sizeof(memberListToSend)<<endl;
	//cout<<"max mail size: "<<MaxMailSize<<endl;
	

	outMailHdr.length = strlen(memberListToSend) + 1;

	for(int i = 0; i < numMembers; i++) {
	  outPktHdr.to = members[i].machineNum;
	  outMailHdr.to = members[i].mailboxNum;

	  bool success = postOffice->Send(outPktHdr, outMailHdr, memberListToSend);
	  if(!success) {
	    //sending response to client failed
	    printf("The postOffice Send failed. Terminating Nachos.\n");
	    interrupt->Halt();
	  }
	}
      }
      MemberTableLock->Release();

      break;

    case CREATE_MV:
      printf("Server received Create MV request. MV name = %s.From %d,%d\n", param, inPktHdr.from, inMailHdr.from);
      mvName = param;
      ss.clear();
      //create the MV and return the lock ID
      
      ServerMVTableLock->Acquire();

      printf("Num Server MVs: %d\n",numServerMVs);

      if(numServerMVs >= MAX_SERVER_MVs) {
	printf("**   NumServerMVs: %d, MAX_SERVER_MVs: %d\n",numServerMVs, MAX_SERVER_MVs);
	//server MV table full
	ServerMVTableLock->Release();
	DEBUG('q',"Server MV Table Full\n");
	error = true;
	break;
      }

      for(int i=0; i < numServerMVs; i++) {
	if(serverMVTable[i].name == mvName) {
	  //a MV with the same name already exists
	  //printf("MV name \n%s\" already exists. Sending existing MV ID=%d\n",mvName, i);
	  mvExists = true;
	  mvID = i;
	  break;
	}
      }

      if(!mvExists) {
	serverMVTable[numServerMVs].value = 0;
	serverMVTable[numServerMVs].name = mvName;

	mvID = numServerMVs;
	numServerMVs++;
      } else {
	//MV already exists
      }
      ServerMVTableLock->Release();
      
      sprintf(response, "%d", mvID); //send the client the MV ID
      sendResponse = true;
      break;

    case GET:
      printf("Server received Get request. MV ID = %s.\n",param);
      ss.clear();
      ss.str(param);
      ss>>mvID;
      //printf("MV id: %d\n",lockID);

      ServerMVTableLock->Acquire();
      if(mvID < 0 || mvID >= numServerMVs) {
	//this is a bad value
	DEBUG('q',"BAD VALUE\n");
	//error = true;
	break;
      }

      mvValue = serverMVTable[mvID].value;

      ServerMVTableLock->Release();

      //send the value to the client
      sprintf(response, "%d", mvValue);
      sendResponse = true;
     
      break;

    case SET:
      ss>>param2; //get 2nd parameter (MV value)
      printf("Server received Set request. MV ID = %s, Value = %s.\n",param,param2);
      ss.clear();
      ss.str(param);
      ss>>mvID;

      //int mvValue;
      ss.clear();
      ss.str(param2);
      ss>>mvValue;
      //printf("MV id: %d\n",lockID);

      ServerMVTableLock->Acquire();
      if(mvID < 0 || mvID >= numServerMVs) {
	//this is a bad value
	DEBUG('q',"BAD VALUE\n");
	// error = true;
	break;
      }

      serverMVTable[mvID].value = mvValue;

      ServerMVTableLock->Release();


      sendResponse = false;
      break;
      
    case UNKNOWN:
      printf("Server received an Unknown request.\n");
      break;

    }

    if(sendResponse) {
      //Send a message back to the client
      outMailHdr.length = sizeof(response) + 1;
      bool success = postOffice->Send(outPktHdr, outMailHdr, response);
      if(!success) {
	//sending response to client failed
	printf("The postOffice Send failed. Terminating Nachos.\n");
	interrupt->Halt();
      } else {
	printf("Server sent response to client machine=%d, box=%d, response=%s.\n",outPktHdr.to,outMailHdr.to,response);
      }
    } 
       
    else if(error) {
      //error doing something, send a message to client
      
      //todo- send error msg to client
      sprintf(response, "%d", -3);
      outMailHdr.length = sizeof(response) + 1;
      bool success = postOffice->Send(outPktHdr, outMailHdr, response);
      if(!success) {
	//sending response to client failed
	printf("The postOffice Send failed. Terminating Nachos.\n");
	interrupt->Halt();
      } else {
	printf("Sent response to client.\n");
      }
    }
    

  }

}

void TestProject3() {

  TestRegister();

  int newMVID = TestCreateMV();

  TestSet(newMVID, 42);

  TestGet(newMVID);

  //printf("creating lock\n");
  //int createLockID = TestCreateLock();
  //int createLockID2 = TestCreateLock();

  //printf("acquiring lock #%d\n",createLockID);
  //TestAcquireLock(createLockID);
  //printf("trying to acquire same lock again\n");
  //TestAcquireLock(createLockID);

  //TestReleaseLock(createLockID);

  //TestDestroyLock(createLockID);

  //printf("creating condition\n");
  //int createCondID = TestCreateCondition();

  //printf("waiting on CV #%d with Lock #%d\n", createCondID, createLockID);
  //TestWait(createCondID, createLockID);

  //printf("signaling CV #%d with Lock #%d\n", createCondID, createLockID);
  //TestSignal(createCondID, createLockID);

  //printf("broadcasting CV #%d with Lock #%d\n", createCondID, createLockID);
  //TestBroadcast(createCondID, createLockID);

}

void TestSet(int theMVID, int theValue) {

  stringstream ss;

  int myClientNum = 1;

  PacketHeader outPktHdr, inPktHdr;
  MailHeader outMailHdr, inMailHdr;
  char buffer[MaxMailSize];
  //char *data = "L 1234";

  sprintf(buffer, "SMV %d %d", theMVID, theValue);

  //ss<<"A "<<theLockID;

  //ss>>buffer;


  outPktHdr.to = 0; //hard coded to 0 for testing
  outMailHdr.to = 0;
  outMailHdr.from = 1;
  outMailHdr.length = strlen(buffer) + 1;

  bool success = postOffice->Send(outPktHdr, outMailHdr, buffer);

  if(!success) {
    printf("The postOffice Send failed.\n");
    interrupt->Halt();
  }

  



}

void TestGet(int theMVID) {

  stringstream ss;

  int myClientNum = 1;

  PacketHeader outPktHdr, inPktHdr;
  MailHeader outMailHdr, inMailHdr;
  char buffer[MaxMailSize];
  //char *data = "L 1234";

  sprintf(buffer, "GMV %d", theMVID);

  //ss<<"A "<<theLockID;

  //ss>>buffer;


  outPktHdr.to = 0; //hard coded to 0 for testing
  outMailHdr.to = 0;
  outMailHdr.from = 1;
  outMailHdr.length = strlen(buffer) + 1;

  bool success = postOffice->Send(outPktHdr, outMailHdr, buffer);

  if(!success) {
    printf("The postOffice Send failed.\n");
    interrupt->Halt();
  }

  
  postOffice->Receive(myClientNum, &inPktHdr, &inMailHdr, buffer);
  printf("Got \"%s\" from %d, box %d\n",buffer,inPktHdr.from,inMailHdr.from);
  fflush(stdout);

  ss.str(buffer);
  int mvVal_rec;
  ss>>mvVal_rec;

  if(mvVal_rec < 0) {
    printf("Error Getting the MV value. Server Response=%d.\n",mvVal_rec);
  } else {
    printf("Successfully Got the MV value. Server Response=%d.\n",mvVal_rec);
  }


}

int TestCreateMV() {

  stringstream ss;

  int myClientNum = 1;

  PacketHeader outPktHdr, inPktHdr;
  MailHeader outMailHdr, inMailHdr;
  char buffer[MaxMailSize];
  char *data = "CMV name";



  outPktHdr.to = 0; //hard coded to 0 for testing
  outMailHdr.to = 0;
  outMailHdr.from = 1;
  outMailHdr.length = strlen(data) + 1;

  bool success = postOffice->Send(outPktHdr, outMailHdr, data);

  if(!success) {
    printf("The postOffice Send failed.\n");
    interrupt->Halt();
  }

  
  postOffice->Receive(myClientNum, &inPktHdr, &inMailHdr, buffer);
  printf("Got \"%s\" from %d, box %d\n",buffer,inPktHdr.from,inMailHdr.from);
  fflush(stdout);

  ss.str(buffer);
  int mvID_rec;
  ss>>mvID_rec;

  return mvID_rec;

}

void TestRegister() {

  stringstream ss;

  int myClientNum = 1;

  PacketHeader outPktHdr, inPktHdr;
  MailHeader outMailHdr, inMailHdr;
  char buffer[MaxMailSize];
  char *data = "J";



  outPktHdr.to = 0; //hard coded to 0 for testing
  outMailHdr.to = 0;
  outMailHdr.from = 1;
  outMailHdr.length = strlen(data) + 1;

  bool success = postOffice->Send(outPktHdr, outMailHdr, data);

  if(!success) {
    printf("The postOffice Send failed.\n");
    interrupt->Halt();
  }

  //cout<<"buffer: "<<buffer<<endl;
  
  postOffice->Receive(myClientNum, &inPktHdr, &inMailHdr, buffer);
  printf("Got \"%s\" from %d, box %d\n",buffer,inPktHdr.from,inMailHdr.from);
  //cout<<buffer<<endl;
  fflush(stdout);
  
  ss.clear();
  ss.str(buffer);
  /*
  int lockID_rec;
  ss>>lockID_rec;

  return lockID_rec;
  */
  string currentClient;
  int numClients = 0;
  Member clients[1000];
  printf("Member List Received:\n");
  while( getline(ss, currentClient, ' ')) {
    
    int firstCommaPos = currentClient.find_first_of(",");
    //cout<<"firstCommaPos:"<<firstCommaPos<<endl;
    //int lastPeriodPos = currentClient.find_last_of(".");
    string machineNumStr = currentClient.substr(0,firstCommaPos);
    string mailboxNumStr = currentClient.substr(firstCommaPos+1);
    //cout<<"machine num: "<<machineNumStr<<" mailbox num: "<<mailboxNumStr<<endl;
    int machineNum = atoi(machineNumStr.c_str());
    int mailboxNum = atoi(mailboxNumStr.c_str());
    clients[numClients].machineNum = machineNum;
    clients[numClients].mailboxNum = mailboxNum;
    numClients++;
    printf("   Machine: %d, Mailbox: %d\n", machineNum, mailboxNum);
    //cout<<"machine num: "<<machineNum<<" mailbox num: "<<mailboxNum<<endl;
  }
  printf("There are %d clients (including myself)\n",numClients);

}

int TestCreateLock() {

  stringstream ss;

  int myClientNum = 1;

  PacketHeader outPktHdr, inPktHdr;
  MailHeader outMailHdr, inMailHdr;
  char buffer[MaxMailSize];
  char *data = "L 1234";



  outPktHdr.to = 0; //hard coded to 0 for testing
  outMailHdr.to = 0;
  outMailHdr.from = 1;
  outMailHdr.length = strlen(data) + 1;

  bool success = postOffice->Send(outPktHdr, outMailHdr, data);

  if(!success) {
    printf("The postOffice Send failed.\n");
    interrupt->Halt();
  }

  
  postOffice->Receive(myClientNum, &inPktHdr, &inMailHdr, buffer);
  printf("Got \"%s\" from %d, box %d\n",buffer,inPktHdr.from,inMailHdr.from);
  fflush(stdout);

  ss.str(buffer);
  int lockID_rec;
  ss>>lockID_rec;

  return lockID_rec;

}

void TestAcquireLock(int theLockID) {

  stringstream ss;

  int myClientNum = 1;

  PacketHeader outPktHdr, inPktHdr;
  MailHeader outMailHdr, inMailHdr;
  char buffer[MaxMailSize];
  //char *data = "L 1234";

  sprintf(buffer, "A %d", theLockID);

  //ss<<"A "<<theLockID;

  //ss>>buffer;


  outPktHdr.to = 0; //hard coded to 0 for testing
  outMailHdr.to = 0;
  outMailHdr.from = 1;
  outMailHdr.length = strlen(buffer) + 1;

  bool success = postOffice->Send(outPktHdr, outMailHdr, buffer);

  if(!success) {
    printf("The postOffice Send failed.\n");
    interrupt->Halt();
  }

  
  postOffice->Receive(myClientNum, &inPktHdr, &inMailHdr, buffer);
  printf("Got \"%s\" from %d, box %d\n",buffer,inPktHdr.from,inMailHdr.from);
  fflush(stdout);

  ss.str(buffer);
  int lockID_rec;
  ss>>lockID_rec;

  if(lockID_rec < 0) {
    printf("Error acquiring lock. Server Response=%d.\n",lockID_rec);
  } else {
    printf("Successfully acquired lock. Server Response=%d.\n",lockID_rec);
  }


}

void TestReleaseLock(int theLockID) {

  stringstream ss;

  int myClientNum = 1;

  PacketHeader outPktHdr, inPktHdr;
  MailHeader outMailHdr, inMailHdr;
  char buffer[MaxMailSize];
  //char *data = "L 1234";

  sprintf(buffer, "R %d", theLockID);

  //ss<<"A "<<theLockID;

  //ss>>buffer;


  outPktHdr.to = 0; //hard coded to 0 for testing
  outMailHdr.to = 0;
  outMailHdr.from = 1;
  outMailHdr.length = strlen(buffer) + 1;

  bool success = postOffice->Send(outPktHdr, outMailHdr, buffer);

  if(!success) {
    printf("The postOffice Send failed.\n");
    interrupt->Halt();
  }

  
  postOffice->Receive(myClientNum, &inPktHdr, &inMailHdr, buffer);
  printf("Got \"%s\" from %d, box %d\n",buffer,inPktHdr.from,inMailHdr.from);
  fflush(stdout);

  ss.str(buffer);
  int lockID_rec;
  ss>>lockID_rec;

  if(lockID_rec < 0) {
    //there was an error releasing the lock
    printf("Received Lock Release ERROR.\n");
  } else {
    //lock released successfully
    printf("Received Lock Released confirmation.\n");
  }


}

void TestDestroyLock(int theLockID) {

  stringstream ss;

  int myClientNum = 1;

  PacketHeader outPktHdr, inPktHdr;
  MailHeader outMailHdr, inMailHdr;
  char buffer[MaxMailSize];
  //char *data = "L 1234";

  sprintf(buffer, "D %d", theLockID);

  //ss<<"A "<<theLockID;

  //ss>>buffer;


  outPktHdr.to = 0; //hard coded to 0 for testing
  outMailHdr.to = 0;
  outMailHdr.from = 1;
  outMailHdr.length = strlen(buffer) + 1;

  bool success = postOffice->Send(outPktHdr, outMailHdr, buffer);

  if(!success) {
    printf("The postOffice Send failed.\n");
    interrupt->Halt();
  }

  
  postOffice->Receive(myClientNum, &inPktHdr, &inMailHdr, buffer);
  printf("Got \"%s\" from %d, box %d\n",buffer,inPktHdr.from,inMailHdr.from);
  fflush(stdout);

  //ss.str(buffer);
  //int lockID_rec;
  //ss>>lockID_rec;


}

int TestCreateCondition() {

  stringstream ss;

  int myClientNum = 1;

  PacketHeader outPktHdr, inPktHdr;
  MailHeader outMailHdr, inMailHdr;
  char buffer[MaxMailSize];
  char *data = "C 1234";



  outPktHdr.to = 0; //hard coded to 0 for testing
  outMailHdr.to = 0;
  outMailHdr.from = 1;
  outMailHdr.length = strlen(data) + 1;

  bool success = postOffice->Send(outPktHdr, outMailHdr, data);

  if(!success) {
    printf("The postOffice Send failed.\n");
    interrupt->Halt();
  }

  
  postOffice->Receive(myClientNum, &inPktHdr, &inMailHdr, buffer);
  printf("Got \"%s\" from %d, box %d\n",buffer,inPktHdr.from,inMailHdr.from);
  fflush(stdout);

  ss.str(buffer);
  int condID_rec;
  ss>>condID_rec;

  return condID_rec;

}

void TestWait(int theCondID, int theLockID) {

  stringstream ss;

  int myClientNum = 1;

  PacketHeader outPktHdr, inPktHdr;
  MailHeader outMailHdr, inMailHdr;
  char buffer[MaxMailSize];
  //char *data = "L 1234";

  sprintf(buffer, "W %d %d", theCondID, theLockID);

  //ss<<"A "<<theLockID;

  //ss>>buffer;


  outPktHdr.to = 0; //hard coded to 0 for testing
  outMailHdr.to = 0;
  outMailHdr.from = 1;
  outMailHdr.length = strlen(buffer) + 1;

  bool success = postOffice->Send(outPktHdr, outMailHdr, buffer);

  if(!success) {
    printf("The postOffice Send failed.\n");
    interrupt->Halt();
  }

  //Wait does not expect any response message
  //postOffice->Receive(myClientNum, &inPktHdr, &inMailHdr, buffer);
  //printf("Got \"%s\" from %d, box %d\n",buffer,inPktHdr.from,inMailHdr.from);
  //fflush(stdout);




}

void TestSignal(int theCondID, int theLockID) {

  stringstream ss;

  int myClientNum = 1;

  PacketHeader outPktHdr, inPktHdr;
  MailHeader outMailHdr, inMailHdr;
  char buffer[MaxMailSize];
  //char *data = "L 1234";

  sprintf(buffer, "S %d %d", theCondID, theLockID);

  //ss<<"A "<<theLockID;

  //ss>>buffer;


  outPktHdr.to = 0; //hard coded to 0 for testing
  outMailHdr.to = 0;
  outMailHdr.from = 1;
  outMailHdr.length = strlen(buffer) + 1;

  bool success = postOffice->Send(outPktHdr, outMailHdr, buffer);

  if(!success) {
    printf("The postOffice Send failed.\n");
    interrupt->Halt();
  }

  
  postOffice->Receive(myClientNum, &inPktHdr, &inMailHdr, buffer);
  printf("Got \"%s\" from %d, box %d\n",buffer,inPktHdr.from,inMailHdr.from);
  fflush(stdout);

  //ss.str(buffer);
  //int lockID_rec;
  //ss>>lockID_rec;


}

void TestBroadcast(int theCondID, int theLockID) {

  stringstream ss;

  int myClientNum = 1;

  PacketHeader outPktHdr, inPktHdr;
  MailHeader outMailHdr, inMailHdr;
  char buffer[MaxMailSize];
  //char *data = "L 1234";

  sprintf(buffer, "B %d %d", theCondID, theLockID);

  //ss<<"A "<<theLockID;

  //ss>>buffer;


  outPktHdr.to = 0; //hard coded to 0 for testing
  outMailHdr.to = 0;
  outMailHdr.from = 1;
  outMailHdr.length = strlen(buffer) + 1;

  bool success = postOffice->Send(outPktHdr, outMailHdr, buffer);

  if(!success) {
    printf("The postOffice Send failed.\n");
    interrupt->Halt();
  }

  
  postOffice->Receive(myClientNum, &inPktHdr, &inMailHdr, buffer);
  printf("Got \"%s\" from %d, box %d\n",buffer,inPktHdr.from,inMailHdr.from);
  fflush(stdout);

  //ss.str(buffer);
  //int lockID_rec;
  //ss>>lockID_rec;


}

RequestType getRequestType(char* req) {
  
  if(strcmp(req, "L")==0) {
    return CREATE_LOCK;
  } else if(strcmp(req, "A")==0) {
    return ACQUIRE;
  } else if (strcmp(req, "R")==0) {
    return RELEASE;
  } else if (strcmp(req, "D")==0) {
    return DESTROY_LOCK;
  } else if (strcmp(req, "C")==0) {
    return CREATE_CONDITION;
  } else if (strcmp(req, "W")==0) {
    return WAIT;
  } else if (strcmp(req, "S")==0) {
    return SIGNAL;
  } else if (strcmp(req, "B")==0) {
    return BROADCAST;
  } else if (strcmp(req, "J")==0) {
    return REGISTER;
  } else if (strcmp(req, "CMV")==0) {
    return CREATE_MV;
  } else if (strcmp(req, "GMV")==0) {
    return GET;
  } else if (strcmp(req, "SMV")==0) {
    return SET;
  } else {
    return UNKNOWN;
  }

}
