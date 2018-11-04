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