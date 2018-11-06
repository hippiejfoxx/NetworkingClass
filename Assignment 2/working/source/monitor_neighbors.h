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
#include "utilities.h"

extern int globalMyID;
//last time you heard from each node. TODO: you will want to monitor this
//in order to realize when a neighbor has gotten cut off from you.
extern struct timeval globalLastHeartbeat[256];

extern int connected[256];

//our all-purpose UDP socket, to be bound to 10.1.1.globalMyID, port 7777
extern int globalSocketUDP;
//pre-filled for sending to 10.1.1.0 - 255, port 7777
extern struct sockaddr_in globalNodeAddrs[256];

RouteInfo_vector * distances[256];

void openCostFile(char * fileName)
{
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
		if(target >= 0)
		{
			RouteInfo newInfo;
			newInfo.nodeID = target;
			newInfo.cost = distance;
			newInfo.path = *newIntVector();

			distances[target] = newRouteInfoVector();

			RouteInfo_vector * vec = distances[target];
			addRouteInfoToVector(vec, newInfo);
		}
	}
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

void sendCostUpdateMsg(int target, long newDist, int numHops, int * hops)
{
	int pad = 0;
	int size = (4 * sizeof(char)) + (2 * sizeof(int)) + sizeof(long) + (numHops * sizeof(int));
	char * msg = (char *)malloc(size);

	memcpy(msg, "CUPD", (4 * sizeof(char)));
	pad = pad + (4 * sizeof(char));

	memcpy(msg+pad, &target, sizeof(int));
	pad = pad + sizeof(int);

	memcpy(msg+pad, &newDist, sizeof(long));
	pad = pad + sizeof(long);

	memcpy(msg+pad, &numHops, sizeof(int));
	pad = pad + sizeof(int);

	for(int i = 0; i < numHops; ++i)
	{
		memcpy(msg+pad, &hops[i], sizeof(int));
		pad = pad + sizeof(int);
	}

	hackyBroadcast(msg, size);
}

void sendInfoMsg(int target, long newDist, int numHops, int * hops)
{
	int pad = 0;
	int size = (4 * sizeof(char)) + (2 * sizeof(int)) + sizeof(long) + (numHops * sizeof(int));
	char * msg = (char *)malloc(size);

	memcpy(msg, "INFO", (4 * sizeof(char)));
	pad = pad + (4 * sizeof(char));
	
	memcpy(msg+pad, &target, sizeof(int));
	pad = pad + sizeof(int);

	memcpy(msg+pad, &newDist, sizeof(long));
	pad = pad + sizeof(long);

	memcpy(msg+pad, &numHops, sizeof(int));
	pad = pad + sizeof(int);

	for(int i = 0; i < numHops; ++i)
	{
		memcpy(msg+pad, &hops[i], sizeof(int));
		pad = pad + sizeof(int);
	}

	hackyBroadcast(msg, size);
}

void sendWithdrawMsg(int target, long newDist, int numHops, int * hops)
{
	int pad = 0;
	int size = (4 * sizeof(char)) + (2 * sizeof(int)) + sizeof(long) + (numHops * sizeof(int));
	char * msg = (char *)malloc(size);

	memcpy(msg, "WITH", (4 * sizeof(char)));
	pad = pad + (4 * sizeof(char));
	
	memcpy(msg+pad, &target, sizeof(int));
	pad = pad + sizeof(int);

	memcpy(msg+pad, &newDist, sizeof(long));
	pad = pad + sizeof(long);

	memcpy(msg+pad, &numHops, sizeof(int));
	pad = pad + sizeof(int);

	for(int i = 0; i < numHops; ++i)
	{
		memcpy(msg+pad, &hops[i], sizeof(int));
		pad = pad + sizeof(int);
	}

	hackyBroadcast(msg, size);
}

void* monitorConnections(void* unusedParam)
{
	struct timespec sleepFor;
	while(1)
	{
		fflush(stdout);
		struct timeval now;
		gettimeofday(&now, 0);
		sleepFor.tv_sec = 0;
		sleepFor.tv_nsec = 300 * 1000 * 1000 * 2; //600 ms
		for(int i = 0; i < 256; i++)
		{
			if(connected[i])
			{
				struct timeval res; 
				timersub(&now, &globalLastHeartbeat[i], &res);
				if(res.tv_sec > 1)
				{
					printf("Disconnected: %d \n", i);
					connected[i] = 0;

					RouteInfo_vector * vec = distances[i];
					RouteInfo * info;
					int nodes[1];
					nodes[0] = globalMyID;

					if(vec != NULL && findByHops(*vec, 0, &info))
					{
						sendWithdrawMsg(info->nodeID, info->cost, 1, nodes);
					}
				}
			}
		}
		nanosleep(&sleepFor, 0);
	}
}

void handleNeighborMsg(int nodeID)
{
	RouteInfo_vector* vec= distances[nodeID];
	RouteInfo  * info;
	int hop[1];
	hop[0] = globalMyID;

	if(vec == NULL || !findByHops(*vec, 0, &info))
	{
		info = (RouteInfo *)malloc(sizeof(RouteInfo));
		info->nodeID = nodeID;
		info->cost = 1;
		info->isActive = 1;
		info->path = *newIntVector();

		distances[nodeID] = newRouteInfoVector();

		RouteInfo_vector * vec = distances[nodeID];
		addRouteInfoToVector(vec, *info);
	}

	if(connected[nodeID] == 0)
	{
		sendInfoMsg(nodeID, info->cost, 1, hop);
		for(int i = 0; i < 256; i++)
		{
			RouteInfo_vector * routes = distances[i];
			if(routes != NULL)
			{
				for(int j = 0; j < routes->numValues; j++)
				{
					RouteInfo info = routes->routes[j];
					int_vector nodes = info.path;
					addValueToIntVector(&nodes, globalMyID);
					sendInfoMsg(info.nodeID, info.cost, nodes.numValues, nodes.values);
				}
			}
		}
		connected[nodeID] = 1;
	}
}

void updateNeighborInfo(int nodeID, int newCost)
{
	RouteInfo_vector * vec= distances[nodeID];
	int hop[1];
	hop[0] = globalMyID;

	if(vec == NULL)
	{
		RouteInfo info;
		info.nodeID = nodeID;
		info.cost = newCost;
		info.path = *newIntVector();

		distances[nodeID] = newRouteInfoVector();

		RouteInfo_vector * vec = distances[nodeID];
		addRouteInfoToVector(vec, info);
	} else 
	{
		RouteInfo * existing;
		int found = findByHops(*vec, 0, &existing);
		if(found)
		{
			existing->cost = newCost;
		}
	}

	int pad = 0;
	int size = (4 * sizeof(char)) + (2 * sizeof(int)) + sizeof(long);
	char * msg = (char *)malloc(size);
	memset(msg, 0, size);

	memcpy(msg, "CUPD", (4 * sizeof(char)));
	pad = pad + (4 * sizeof(char));

	memcpy(msg+pad, &globalMyID, sizeof(int));
	pad = pad + sizeof(int);

	memcpy(msg+pad, &newCost, sizeof(long));
	pad = pad + sizeof(long);

	sendto(globalSocketUDP, msg, size, 0, (struct sockaddr*)&globalNodeAddrs[nodeID], sizeof(globalNodeAddrs[nodeID]));
}

void handleCostMessage(unsigned char * recvBuf)
{
	int target = 0;
	long newDist = 0;

	memcpy(&target, recvBuf + 4, 2);
	memcpy(&newDist, recvBuf + 6, 4);
	target = IntEndianConversion(target);
	newDist = LongEndianConversion(newDist);

	updateNeighborInfo(target, newDist);
}



int getNextHop(RouteInfo_vector vec)
{
	printf("Avaliable routes: %d\n", vec.numValues);
	printRoutingInfo(&vec);
	// int lowestCost = findLowestActiveCost(vec);
	// printf("Lowest cost: %d\n", lowestCost);
	// RouteInfo_vector * res = newRouteInfoVector();
	// int found = findActiveRoutesWithCost(vec, lowestCost, &res);
	// if(found > 0)
	// {
	// 	RouteInfo_vector routeVec = *res;
	// 	printf("Found %d routes\n", found);
	// 	if(found == 1)
	// 	{
	// 		if(routeVec.routes[0].path.numValues < 1 && connected[routeVec.routes[0].nodeID])
	// 		{
	// 			return routeVec.routes[0].nodeID;
	// 		} else
	// 		{
	// 			return routeVec.routes[0].path.values[routeVec.routes[0].path.numValues - 1];
	// 		}
	// 	} else
	// 	{
	// 		if(routeVec.routes[0].path.values[routeVec.routes[0].path.numValues - 1] < routeVec.routes[1].path.values[routeVec.routes[1].path.numValues - 1])
	// 		{
	// 			return routeVec.routes[0].path.values[routeVec.routes[0].path.numValues - 1];
	// 		} else
	// 		{
	// 			return routeVec.routes[1].path.values[routeVec.routes[1].path.numValues - 1];
	// 		}
	// 	}
	// } 
	return 1;
}

void handleSendMessage(unsigned char * recvBuf)
{
	char msgBuff[100];
	int target = 0;
	memcpy(&target, recvBuf + 4, 2);
	memcpy(msgBuff, recvBuf + 6, 100);
	target = IntEndianConversion(target);

	if(target == globalMyID)
	{
		printf("%s", msgBuff);
	} else 
	{
		if(distances[target] != NULL)
		{
			int nextHop = getNextHop(*distances[target]);
			sendto(globalSocketUDP, recvBuf, 100, 0, (struct sockaddr*)&globalNodeAddrs[nextHop], sizeof(globalNodeAddrs[nextHop]));
		}
	}
	
	fflush(stdout);
	
}

void handleWithdrawMessage(char * recvBuf)
{
	int pad = sizeof(char) * 4; 
	int nodeID;
	long cost;
	int numHops;

	memcpy(&nodeID, recvBuf + pad, sizeof(int));
	pad = pad + sizeof(int);

	memcpy(&cost, recvBuf + 4 + sizeof(int) , sizeof(long));
	pad = pad + (sizeof(long));

	memcpy(&numHops, recvBuf + 4 + sizeof(int) + sizeof(long), sizeof(int));
	pad = pad + sizeof(int);

	RouteInfo * info = (RouteInfo *)malloc(sizeof(RouteInfo));
	info->nodeID = nodeID;
	info->cost = cost;
	int_vector * vec = newIntVector();

	for(int i = 0; i < numHops; i++)
	{
		addValueToIntVector(vec, *(recvBuf+pad));
		pad = pad + sizeof(int);
	}

	info->path = *vec;

	if(nodeID != globalMyID && !contains(*vec, globalMyID))
	{
		RouteInfo * found;
		if(findMatchingRoute(*distances[nodeID], *info, &found))
		{
			printf("withdraw processed\n");
			found->isActive = 0;
		}
		addValueToIntVector(vec, globalMyID);
		sendWithdrawMsg(nodeID, cost, numHops + 1, vec->values);
		fflush(stdout);
	}
}

void handleCostUpdateMessage(unsigned char * recvBuf)
{
	int pad = sizeof(char) * 4; 
	int nodeID;
	long cost;
	int numHops;

	memcpy(&nodeID, recvBuf + pad, sizeof(int));
	pad = pad + sizeof(int);

	memcpy(&cost, recvBuf + 4 + sizeof(int) , sizeof(long));
	pad = pad + (sizeof(long));

	memcpy(&numHops, recvBuf + 4 + sizeof(int) + sizeof(long), sizeof(int));
	pad = pad + sizeof(int);

	RouteInfo * info = (RouteInfo *)malloc(sizeof(RouteInfo));
	info->nodeID = nodeID;
	info->cost = cost;
	info->isActive = 1;
	int_vector * vec = newIntVector();

	for(int i = 0; i < numHops; i++)
	{
		addValueToIntVector(vec, *(recvBuf+pad));
		pad = pad + sizeof(int);
	}

	info->path = *vec;

	if(nodeID != globalMyID && !contains(*vec, globalMyID)) //Target is not current node and route doesn't contain current node
	{
		RouteInfo_vector * existingRoutes = distances[nodeID];

		if(existingRoutes == NULL)
		{
			distances[nodeID] = newRouteInfoVector();
		}

		int sender = info->path.values[info->path.numValues - 1];

		RouteInfo * oldRoute;
		RouteInfo * neighborInfo;

		if(findByHops(*(distances[sender]), 0, &neighborInfo))
		{
			info->cost = info->cost + neighborInfo->cost;
			int found = findMatchingRoute(*distances[nodeID], *info, &oldRoute);
			if(!found)
			{
				addRouteInfoToVector(distances[nodeID], *info);
			} 
			else
			{
				if(oldRoute->cost != info->cost)
				{
					oldRoute->cost = info->cost;
				}
			}
			addValueToIntVector(vec, globalMyID);
			sendInfoMsg(nodeID, info->cost, numHops + 1, vec->values);
		}
		fflush(stdout);
	}
}

void handleInfoMessage(unsigned char * recvBuf)
{
	int pad = sizeof(char) * 4; 
	int nodeID;
	long cost;
	int numHops;

	memcpy(&nodeID, recvBuf + pad, sizeof(int));
	pad = pad + sizeof(int);

	memcpy(&cost, recvBuf + 4 + sizeof(int) , sizeof(long));
	pad = pad + (sizeof(long));

	memcpy(&numHops, recvBuf + 4 + sizeof(int) + sizeof(long), sizeof(int));
	pad = pad + sizeof(int);

	RouteInfo * info = (RouteInfo *)malloc(sizeof(RouteInfo));
	info->nodeID = nodeID;
	info->cost = cost;
	info->isActive = 1;
	int_vector * vec = newIntVector();

	for(int i = 0; i < numHops; i++)
	{
		addValueToIntVector(vec, *(recvBuf+pad));
		pad = pad + sizeof(int);
	}

	info->path = *vec;

	if(nodeID != globalMyID && !contains(*vec, globalMyID))
	{
		RouteInfo_vector * existingRoutes = distances[nodeID];

		if(existingRoutes == NULL)
		{
			distances[nodeID] = newRouteInfoVector();
		}

		int sender = info->path.values[info->path.numValues - 1];

		RouteInfo * oldRoute;
		RouteInfo * neighborInfo;

		if(findByHops(*(distances[sender]), 0, &neighborInfo))
		{
			info->cost = info->cost + neighborInfo->cost;
			int found = findMatchingRoute(*distances[nodeID], *info, &oldRoute);
			if(!found)
			{
				addRouteInfoToVector(distances[nodeID], *info);
			} 
			else
			{
				if(oldRoute->cost != info->cost)
				{
					oldRoute->cost = info->cost;
					oldRoute->isActive = true;
				}
			}
			addValueToIntVector(vec, globalMyID);
			sendInfoMsg(nodeID, info->cost, numHops + 1, vec->values);
		}
		fflush(stdout);
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
			handleNeighborMsg(heardFrom);

			//record that we heard from heardFrom just now.
			gettimeofday(&globalLastHeartbeat[heardFrom], 0);
			connected[heardFrom] = 1;
		}
		
		//Is it a packet from the manager? (see mp2 specification for more details)
		//send format: 'send'<4 ASCII bytes>, destID<net order 2 byte signed>, <some ASCII message>
		if(!strncmp(recvBuf, "send", 4))
		{
			// heardFrom = atoi(
			// 		strchr(strchr(strchr(fromAddr,'.')+1,'.')+1,'.')+1);
			handleSendMessage(recvBuf);
		}
		//'cost'<4 ASCII bytes>, destID<net order 2 byte signed> newCost<net order 4 byte signed>
		else if(!strncmp(recvBuf, "cost", 4))
		{
		// 	heardFrom = atoi(
		// 			strchr(strchr(strchr(fromAddr,'.')+1,'.')+1,'.')+1);
			handleCostMessage(recvBuf);
		}
		else if(!strncmp(recvBuf, "INFO", 4))
		{
			// heardFrom = atoi(
			// 		strchr(strchr(strchr(fromAddr,'.')+1,'.')+1,'.')+1);
			handleInfoMessage(recvBuf);
		}
		else if(!strncmp(recvBuf, "WITH", 4))
		{
			printf("Received withdraw \n");
			handleWithdrawMessage(recvBuf);
		}
		else if(!strncmp(recvBuf, "CUPD", 4))
		{
			printf("Received cost update \n");
			fflush(stdout);
			handleCostUpdateMessage(recvBuf);
		}

	}
	//(should never reach here)
	close(globalSocketUDP);
}