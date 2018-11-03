#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include "int_vector.h"
#include "routeInfo_vector.h"

extern int globalMyID;
//last time you heard from each node. TODO: you will want to monitor this
//in order to realize when a neighbor has gotten cut off from you.
extern struct timeval globalLastHeartbeat[256];

//our all-purpose UDP socket, to be bound to 10.1.1.globalMyID, port 7777
extern int globalSocketUDP;
//pre-filled for sending to 10.1.1.0 - 255, port 7777
extern struct sockaddr_in globalNodeAddrs[256];

RouteInfo_vector * distances[256];

void openCostFile(char * fileName)
{
	printf("Opening: %s\n", fileName);
	//TODO: read and parse initial costs file. default to cost 1 if no entry for a node. file may be empty.
	FILE * initFile = fopen(fileName, "r");
	if(initFile == NULL) 
	{
		perror("Cannot open costs file");
		exit(1);
	}

	int target, distance;

	while(!feof (initFile))
	{
		target = -1;
		distance = -1;
		fscanf(initFile, "%d %d", &target, &distance);
		// printf("Node: %d Costs: %d\n", target, distance);
		if(target >= 0)
		{
			RouteInfo newInfo;
			newInfo.nodeID = target;
			newInfo.cost = distance;
			newInfo.values = *newIntVector();

			distances[target] = newRouteInfoVector();

			RouteInfo_vector * vec = distances[target];
			addRouteInfoToVector(vec, newInfo);
			// printRoutingInfo(vec);
		}
	}
}

int IntEndianConversion(int in)
{
	char * val = (char *)&in;
	char * rs = (char *)malloc(sizeof(char)* 2);

	for(int i = 0; i < 2; i++)
	{
		rs[(2 - 1) - i] = val[i];
	}

	int result =  *(int *)rs;
	free(rs);
	return result;
}

long LongEndianConversion(long in)
{
	char * val = (char *)&in;
	char * rs = (char *)malloc(sizeof(char)* 4);

	for(int i = 0; i < 4; i++)
	{
		rs[(4 - 1) - i] = val[i];
	}

	long result =  *(int *)rs;
	free(rs);
	return result;
}

//Yes, this is terrible. It's also terrible that, in Linux, a socket
//can't receive broadcast packets unless it's bound to INADDR_ANY,
//which we can't do in this assignment.
void hackyBroadcast(const char* buf, int length)
{
	int i;
	for(i=0;i<256;i++)
		if(i != globalMyID) //(although with a real broadcast you would also get the packet yourself)
			sendto(globalSocketUDP, buf, length, 0,
				  (struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
}

void* announceToNeighbors(void* unusedParam)
{
	struct timespec sleepFor;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 300 * 1000 * 1000; //300 ms
	while(1)
	{
		hackyBroadcast("HEREIAM", 7);
		nanosleep(&sleepFor, 0);
	}
}

void sendInfoMsg(int target, int newDist, int numHops, int * hops)
{
	int size = 4 + (3 * sizeof(int)) + (numHops * sizeof(int));
	char * msg = (char *)malloc(size);
	memcpy(msg, "INFO", 4);
	memcpy(msg+4, &target, sizeof(int));
	memcpy(msg+4+sizeof(int), &newDist, sizeof(int));
	memcpy(msg+4+sizeof(int)+sizeof(int), &numHops, sizeof(int));

	for(int i = 0; i < numHops; ++i)
	{
		memcpy(msg+4 + (3 * sizeof(int)) + (i * sizeof(int)), &hops[i], sizeof(int));
	}
	
	hackyBroadcast(msg, 4 + (3 * sizeof(int)) + (numHops * sizeof(int)));
}

void handleNeighborMsg(int nodeID)
{
	RouteInfo_vector* vec= distances[nodeID];
	RouteInfo info;
	int hop[1];
	hop[0] = globalMyID;

	if(vec == NULL)
	{
		info.nodeID = nodeID;
		info.cost = 1;
		info.values = *newIntVector();

		distances[nodeID] = newRouteInfoVector();

		RouteInfo_vector * vec = distances[nodeID];
		addRouteInfoToVector(vec, info);
		printf("New neighbor: %d \n", nodeID);
	}

	sendInfoMsg(nodeID, info.cost, 1, hop);
}

void updateNeighborInfo(int nodeID, int newCost)
{
	RouteInfo_vector * vec= distances[nodeID];
	RouteInfo info;
	int hop[1];
	hop[0] = globalMyID;

	if(vec == NULL)
	{
		info.nodeID = nodeID;
		info.cost = newCost;
		info.values = *newIntVector();

		distances[nodeID] = newRouteInfoVector();

		RouteInfo_vector * vec = distances[nodeID];
		addRouteInfoToVector(vec, info);
		printf("New neighbor: %d \n", nodeID);
	} else 
	{
		RouteInfo * info;
		int found = findNeighbor(*vec, 0, &info);
		if(found)
		{
			printf("Found NodeID: %d with Cost: %d \n", info->nodeID, info->cost);
			info->cost = newCost;
		}
	}

	sendInfoMsg(nodeID, info.cost, 1, hop);
}

void handleCostMessage(unsigned char * recvBuf)
{
	int target = 0;
	long newDist = 0;

	memcpy(&target, recvBuf + 4, 2);
	memcpy(&newDist, recvBuf + 6, 4);
	target = IntEndianConversion(target);
	newDist = LongEndianConversion(newDist);
	// distances[target] = newDist;

	updateNeighborInfo(target, newDist);
}

void handleSendMessage(unsigned char * recvBuf)
{
	char msgBuff[100];
	int target = 0;
	memcpy(&target, recvBuf + 4, 2);
	memcpy(msgBuff, recvBuf + 6, 100);

	target = IntEndianConversion(target);
	fflush(stdout);

	sendto(globalSocketUDP, msgBuff, 100, 0, (struct sockaddr*)&globalNodeAddrs[target], sizeof(globalNodeAddrs[target]));
}

void handleInfoMessage(unsigned char * recvBuf)
{
	int nodeID;
	int cost;
	int numHops;
	memcpy(&nodeID, recvBuf + 4 , sizeof(int));
	memcpy(&cost, recvBuf + 4 + sizeof(int) , sizeof(int));
	memcpy(&numHops, recvBuf + 4 + sizeof(int) + sizeof(int), sizeof(int));
	int * hops = (int *)recvBuf + 4 + (sizeof(int) * 3);

	if(nodeID != globalMyID || cost == 0)
	{
		printf("Route info hops: ");
		for(int i = 0; i < numHops; i++)
		{
			printf("%d ", hops[i]);
		}
		printf("\n");
		printf("%s -- New Cost of %d is %d. Hops: %d \n", "INFO", nodeID, cost, numHops);
		fflush(stdout);
		//Todo: Add to distance
		sendInfoMsg(nodeID, cost, numHops + 1, hops);
	}
}

void listenForNeighbors()
{
	char fromAddr[100];
	struct sockaddr_in theirAddr;
	socklen_t theirAddrLen;
	unsigned char recvBuf[1000];

	int bytesRecvd;
	while(1)
	{
		memset(recvBuf, 0, 1000);
		theirAddrLen = sizeof(theirAddr);
		if ((bytesRecvd = recvfrom(globalSocketUDP, recvBuf, 1000 , 0, 
					(struct sockaddr*)&theirAddr, &theirAddrLen)) == -1)
		{
			perror("connectivity listener: recvfrom failed");
			exit(1);
		}
		
		inet_ntop(AF_INET, &theirAddr.sin_addr, fromAddr, 100);
		
		short int heardFrom = -1;
		if(strstr(fromAddr, "10.1.1."))
		{
			heardFrom = atoi(
					strchr(strchr(strchr(fromAddr,'.')+1,'.')+1,'.')+1);
			
			//TODO: this node can consider heardFrom to be directly connected to it; do any such logic now.

			// printf("HEAR :%s\n", recvBuf);
			// fflush(stdout);
			handleNeighborMsg(heardFrom);

			//record that we heard from heardFrom just now.
			gettimeofday(&globalLastHeartbeat[heardFrom], 0);
		}
		
		//Is it a packet from the manager? (see mp2 specification for more details)
		//send format: 'send'<4 ASCII bytes>, destID<net order 2 byte signed>, <some ASCII message>
		if(!strncmp(recvBuf, "send", 4))
		{
			handleSendMessage(recvBuf);
		}
		//'cost'<4 ASCII bytes>, destID<net order 2 byte signed> newCost<net order 4 byte signed>
		else if(!strncmp(recvBuf, "cost", 4))
		{
			handleCostMessage(recvBuf);
		}
		else if(!strncmp(recvBuf, "INFO", 4))
		{
			handleInfoMessage(recvBuf);
		}

	}
	//(should never reach here)
	close(globalSocketUDP);
}