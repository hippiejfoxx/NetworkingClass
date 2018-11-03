#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct dyn_int_array 
{
	int size;
	int numValues;
	int * values;
} int_vector;

int_vector * newIntVector()
{
	int_vector * newVec = (int_vector *)malloc(sizeof(int_vector));
	newVec->size = 10;
	newVec->numValues = 0;
	newVec->values = (int *)malloc(sizeof(int) * 10);
	return newVec;
}

void * addValueToIntVector(int_vector * vec, int val)
{
	if(vec->numValues >= vec->size)
	{
		int * newArr = (int *)malloc(sizeof(int) * (vec->size * 2));
		memcpy(newArr, vec->values, vec->size);
		vec->size = vec->size * 2;
	}
	vec->values[vec->numValues] = val;
	vec->numValues = vec->numValues + 1;
}

// void * printVals(int_vector * vec)
// {
// 	for(int i = 0; i < vec->numValues; i++)
// 	{
// 		printf("%d\n", vec->values[i]);
// 	}
// }

