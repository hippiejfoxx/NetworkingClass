void handleNeighborMsg(int nodeID)
{
	RouteInfo info;
	int hop[1];
	hop[0] = globalMyID;

	if(distances[nodeID] == NULL)
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

void handleCostMessage(unsigned char * recvBuf)
{
	int target = 0;
	long newDist = 0;

	memcpy(&target, recvBuf + 4, 2);
	memcpy(&newDist, recvBuf + 6, 4);
	target = IntEndianConversion(target);
	newDist = LongEndianConversion(newDist);
	// distances[target] = newDist;

	sendInfoMsg(target, newDist, 0);
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
	memcpy(&cost, recvBuf + 4 + sizeof(int) , sizeof(long));
	memcpy(&numHops, recvBuf + 4 + sizeof(int) + sizeof(long), sizeof(int));
	int * hops = (int *)malloc(sizeof(int) * numHops);

	printf("%s -- New Cost of %d is %d. Hops: %d \n", "INFO", nodeID, cost, hops);
	fflush(stdout);

	//Todo: Add to distance
	sendInfoMsg(nodeID, cost, hops + 1, hops);
}
