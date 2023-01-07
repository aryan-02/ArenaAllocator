// Benchmarks first fit
#include "mavalloc.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "parameters.h"

int main(int argc, char *argv[])
{
	for (int testcase = 0; testcase < NUM_TESTCASES; testcase++)
	{
		int iterations = NUM_ITERATIONS;
		double total_time = 0.0;
		while (iterations--)
		{
			char *stuff[NUM_ALLOCS];
			mavalloc_init(300000, FIRST_FIT);
			clock_t start = clock();
			for (int i = 0; i < NUM_ALLOCS; i++)
			{
				stuff[i] = mavalloc_alloc(sizeof(char) * 10);
				strcpy(stuff[i], "Hello\n");
			}
			for (int i = NUM_ALLOCS / 2; i < NUM_ALLOCS; i += 2)
			{
				mavalloc_free(stuff[i]);
			}
			for (int i = NUM_ALLOCS / 2; i < NUM_ALLOCS; i += 2)
			{
				stuff[i] = mavalloc_alloc(sizeof(char));
				*stuff[i] = 'C';
			}
			for (int i = 0; i < NUM_ALLOCS; i++)
			{
				mavalloc_free(stuff[i]);
			}
			double elapsed_time = 1000 * (double)(clock() - start) / CLOCKS_PER_SEC;
			total_time += elapsed_time;
			mavalloc_destroy();
		}
		printf("%lf \n", total_time);
	}
	return 0;
}
