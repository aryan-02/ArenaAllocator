// The MIT License (MIT)
// 
// Copyright (c) 2021, 2022 Trevor Bakker 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES UTA OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "mavalloc.h"
#include <limits.h>
#include <stdlib.h>

#define MAX_ALLOCS 10000

// pool stores the address of the large memory pool we allocate
// inside of mavalloc_init()
static void *pool = NULL;
// size of the memory pool
static size_t pool_size;

// stores the allocation algorithm
enum ALGORITHM alloc_algorithm;

int ledger_top = -1; // stores index of the last ledger item

enum TYPE
{
	P, // Process Allocation
	H // Hole
};

typedef struct Node
{
	size_t size;
	enum TYPE type;
	void *arena;
	int next;
	int previous;
} Node;

// Keeps track of space - holes and process allocations
Node ledger[MAX_ALLOCS];

// traverses to the back of the ledger to find the index of the first entry
int traverse_back()
{
	static int ptr = 0; // static so we save the last return value
	if(ledger[ptr].next == -1) // if the saved value was freed or destroy was called
		ptr = 0; // start search from index 0
	while(ledger[ptr].previous != -1)
	{
		ptr = ledger[ptr].previous;
	}
	return ptr;
}

int mavalloc_init( size_t size, enum ALGORITHM algorithm )
{
	// Set the algorithm global
	alloc_algorithm = algorithm;
	// negative sizes make no sense, so we don't process that case
	if(size < 0)
		return -1;
	
	// allocate the pool of memory and store its size aligned
	pool_size = ALIGN4(size);
	pool = malloc(pool_size);

	// if the allocation failed, return -1 to indicate failure
	if (pool == NULL)
		return -1;
	
	// initializing the first entry in the ledger
	// to start, we have just one hole being the pool we malloc'd
	ledger_top = 0;
	ledger[0].arena = pool;
	ledger[0].size = pool_size;
	ledger[0].previous = -1; // no next element
	ledger[0].next = -1; // no previous element
	ledger[0].type = H;

	// initialize the rest of the ledger with starting values
	for(int i = 1; i < MAX_ALLOCS; i++)
	{
		ledger[i].arena = NULL;
		ledger[i].size = 0;
		ledger[i].previous = -1;
		ledger[i].next = -1;
	}

	// returns 0 on success
	return 0;
}

void mavalloc_destroy( )
{
	free(pool); // free the pool allocated
	// reset ledger to initial state
	for(int i = 0; i < MAX_ALLOCS; i++)
	{
		ledger[i].arena = NULL;
		ledger[i].size = 0;
		ledger[i].previous = -1;
		ledger[i].next = -1;
	}
	pool_size = 0;
	ledger_top = -1;
	return;
}

int first_fit(size_t size)
{
	int ptr = traverse_back();
	while(ptr != -1 && (ledger[ptr].size < size || ledger[ptr].type == P))
	{
		ptr = ledger[ptr].next;
	}
	return ptr;
}

int next_fit(size_t size)
{
	static int last_ptr = 0;
	int ptr = last_ptr;
	while (ptr != -1 && (ledger[ptr].size < size || ledger[ptr].type == P))
	{
		ptr = ledger[ptr].next;
	}
	if(ptr == -1)
	{
		ptr = first_fit(size);
	}
	last_ptr = ptr;
	return ptr;
}

int worst_fit(size_t size) // take the largest available hole
{
	int max_hole_idx = -1;
	int max_hole_size = -1;
	for(int ptr = traverse_back(); ptr != -1; ptr = ledger[ptr].next)
	{
		if(ledger[ptr].type == H && ledger[ptr].size >= size && (long) ledger[ptr].size > max_hole_size)
		{
			max_hole_size = ledger[ptr].size;
			max_hole_idx = ptr;
		}
	}
	return max_hole_idx;
}

int best_fit(size_t size) // take the smallest available hole
{
	int min_hole_idx = -1;
	int min_hole_size = INT_MAX;
	for (int ptr = traverse_back(); ptr != -1; ptr = ledger[ptr].next)
	{
		if (ledger[ptr].type == H && ledger[ptr].size >= size && (long) ledger[ptr].size < min_hole_size)
		{
			min_hole_size = ledger[ptr].size;
			min_hole_idx = ptr;
		}
	}
	return min_hole_idx;
}

void * mavalloc_alloc( size_t size )
{
	int idx = -1;
	size = ALIGN4(size);
	// get the index of the hole in which memory will be allocated
	// based on the algorithm global set
	switch(alloc_algorithm)
	{
		case FIRST_FIT:
			idx = first_fit(size);
			break;
		case NEXT_FIT:
			idx = next_fit(size);
			break;
		case WORST_FIT:
			idx = worst_fit(size);
			break;
		case BEST_FIT:
			idx = best_fit(size);
			break;
	}
	// only return NULL on failure
	if (idx == -1)
		return NULL;


	if(ledger[idx].size == size)
	{
		ledger[idx].type = P;
		return ledger[idx].arena;
	}
	ledger_top++;

	// allocate memory at the hole in ledger[idx]
	ledger[ledger_top].size = size;
	ledger[ledger_top].type = P;
	ledger[ledger_top].previous = ledger[idx].previous;
	if(ledger[idx].previous != -1)
		ledger[ledger[idx].previous].next = ledger_top;
	ledger[ledger_top].next = idx;
	ledger[idx].previous = ledger_top;
	
	ledger[idx].size -= size;

	ledger[ledger_top].arena = ledger[idx].arena;

	ledger[idx].arena = (void*) ((long long)ledger[idx].arena + size);
	
	return ledger[ledger_top].arena;
	
}

void mavalloc_free( void * ptr )
{
	if(!ptr)
		return;
	int i = traverse_back();
	while(i != -1 && ledger[i].arena != ptr)
	{
		i = ledger[i].next;
	}
	if(i != -1)
	{
		ledger[i].type = H;
		// coalesce backwards
		if(ledger[i].previous != -1 && ledger[ledger[i].previous].type == H)
		{
			ledger[ledger[i].previous].size += ledger[i].size;
			if(i == ledger_top)
			{
				ledger_top--;
			}
			i = ledger[i].previous;
			ledger[i].next = ledger[ledger[i].next].next;
			ledger[ledger[i].next].previous = i;
		}
		// coalesce forward
		if(ledger[i].next != -1 && ledger[ledger[i].next].type == H)
		{
			ledger[i].size += ledger[ledger[i].next].size;
			ledger[i].next = ledger[ledger[i].next].next;
			ledger[ledger[i].next].previous = i;
		}
	}
}

int mavalloc_size( )
{
	int number_of_nodes = 0;
	
	if(ledger[traverse_back()].arena == NULL)
	{
		return 0;
	}

	for(int i = traverse_back(); i >= 0; i = ledger[i].next)
	{
		number_of_nodes++;
	}

	return number_of_nodes;
}
