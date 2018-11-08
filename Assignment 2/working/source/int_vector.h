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

int isMatch(int_vector vec1, int_vector vec2)
{

	if(vec1.numValues != vec2.numValues)
	{
		return 0;
	}

	for(int i = 0; i < vec1.numValues; i++)
	{
		if(vec1.values[i] != vec2.values[i])
		{
			return 0;
		}
	}
	return 1;
}

int contains(int_vector vec, int value)
{
	if(vec.numValues <= 0)
	{
		return 0;
	}

	for(int i = 0; i < vec.numValues; i++)
	{
		if(vec.values[i] == value)
		{
			return 1;
		}
	}
	return 0;
}

void printValues(int_vector vec)
{
	for(int i = 0; i < vec.numValues; i++)
	{
		printf(" %d ", vec.values[i]);
	}
	printf("\n");
}