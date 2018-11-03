#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "routeInfo.h"

typedef struct dyn_routeinfo_obj 
{
	int size;
	int numValues;
	RouteInfo * values;
} RouteInfo_vector;

RouteInfo_vector * newRouteInfoVector()
{
	RouteInfo_vector * newVec = (RouteInfo_vector *)malloc(sizeof(RouteInfo_vector));
	newVec->size = 10;
	newVec->numValues = 0;
	newVec->values = (RouteInfo *)malloc(sizeof(RouteInfo) * 10);
	return newVec;
}

void * addRouteInfoToVector(RouteInfo_vector * vec, RouteInfo info)
{
	if(vec->numValues >= vec->size)
	{
		RouteInfo * newArr = (RouteInfo *)malloc(sizeof(RouteInfo) * (vec->size * 2));
		memcpy(newArr, vec->values, vec->size);
		vec->size = vec->size * 2;
	}
	vec->values[vec->numValues] = info;
	vec->numValues = vec->numValues + 1;
}

int findNeighbor(RouteInfo_vector vec, int findHops, RouteInfo ** info)
{
	int found = 0;

	printf("Searching...\n");

	for(int i = 0; i < vec.numValues; i++)
	{
		if(vec.values[i].values.numValues == 0)
		{
			*info =  &vec.values[i];
			return 1;
		}
	}
	return 0;
}

void printRoutingInfo(RouteInfo_vector * vec)
{
	for(int i = 0; i < vec->numValues; i++)
	{
		RouteInfo info = vec->values[i];
		printf("Node %d Cost: %d \n", info.nodeID, info.cost);
		printf("Route: ");
		for(int j = 0; j < info.values.numValues; j++)
		{
			printf("%d ", info.values.values[j]);
		}
		printf("\n");
	}
}
