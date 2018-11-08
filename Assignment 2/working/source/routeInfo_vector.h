#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "routeInfo.h"

typedef struct dyn_routeinfo_obj 
{
	int size;
	int numValues;
	RouteInfo * routes;
} RouteInfo_vector;

RouteInfo_vector * newRouteInfoVector()
{
	RouteInfo_vector * newVec = (RouteInfo_vector *)malloc(sizeof(RouteInfo_vector));
	newVec->size = 10;
	newVec->numValues = 0;
	newVec->routes = (RouteInfo *)malloc(sizeof(RouteInfo) * 10);
	return newVec;
}

void * addRouteInfoToVector(RouteInfo_vector * vec, RouteInfo info)
{
	if(vec->numValues >= vec->size)
	{
		RouteInfo * newArr = (RouteInfo *)malloc(sizeof(RouteInfo) * (vec->size * 2));
		memcpy(newArr, vec->routes, vec->size);
		vec->size = vec->size * 2;
	}
	vec->routes[vec->numValues] = info;
	vec->numValues = vec->numValues + 1;
}

int findByHops(RouteInfo_vector vec, int findHops, RouteInfo ** info)
{
	int found = 0;

	for(int i = 0; i < vec.numValues; i++)
	{
		if(vec.routes[i].path.numValues == 0)
		{
			*info =  &vec.routes[i];
			return 1;
		}
	}
	return 0;
}

int findLowestActiveCost(RouteInfo_vector existingRoutes)
{
	int distance = -1;

	for(int i = 0; i < existingRoutes.numValues; i++)
	{
		RouteInfo cur = existingRoutes.routes[i];
		// printf("Cost: %ld\n", cur.cost);
		fflush(stdout);
		if(cur.isActive && (distance == -1 || (distance >= 0 && distance > cur.cost)))
		{
			distance = cur.cost;
		}
	}
	return distance;
}

int findActiveRoutesWithCost(RouteInfo_vector existingRoutes, int cost, RouteInfo_vector ** results)
{
	for(int i = 0; i < existingRoutes.numValues; i++)
	{
		printf("Route#: %d to %d\n", i, existingRoutes.routes[i].path.values[existingRoutes.routes[i].path.numValues - 1]);
		fflush(stdout);
		RouteInfo cur = existingRoutes.routes[i];
		if(cur.isActive && cur.cost == cost)
		{
			// printf("Match\n");
			addRouteInfoToVector(*results, cur);
		}
	}
	RouteInfo_vector * r = *results;
	return r->numValues;
}

int findAllRoutesWithNextHop(RouteInfo_vector * existingRoutes, int nodeID, RouteInfo ** results)
{
	int index = 0;
	for(int i = 0; i < existingRoutes->numValues; i++)
	{
		RouteInfo * cur = (existingRoutes->routes) + i;
		if(cur->path.numValues > 0 && cur->path.values[cur->path.numValues - 1] == nodeID)
		{
			results[index] = cur;
			index++;
		}
	}
	return index;
}

int findMatchingRoute(RouteInfo_vector * existingRoutes, RouteInfo check, RouteInfo ** info)
{
	for(int i = 0; i < existingRoutes->numValues; i++)
	{
		RouteInfo * cur = existingRoutes->routes + i;
		if(check.path.numValues == cur->path.numValues && isMatch(cur->path, check.path))
		{
			*info = cur;
			return 1;
		}
	}
	return 0;
}

int anyActiveRoutes(RouteInfo_vector routes)
{
	for(int i = 0; i < routes.numValues; i++)
	{
		if(routes.routes[i].isActive)
		{
			return 1;
		}
	}
	return 0;	
}

// void printRoutingInfo(RouteInfo_vector * vec)
// {
// 	for(int i = 0; i < vec->numValues; i++)
// 	{
// 		RouteInfo info = vec->routes[i];
// 		for(int j = 0; j < info.path.numValues; j++)
// 		{
// 			printf("%d ", info.path.values[j]);
// 		}
// 	}
// }
