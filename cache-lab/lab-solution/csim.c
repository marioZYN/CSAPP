#include "cachelab.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <limits.h>

int COUNTER = 0;
enum STATE {HIT, MISS, MISS_EVICT};

typedef struct cache_line {
	unsigned int v;
	unsigned long tag;
	unsigned long age;
} cache_line;

void process(unsigned long address, int s, int b, 
	unsigned long *tag, unsigned long *s_index) {
	/*
	Get tag, set index, e index from address.
	*/
	*tag = address & ~((1 << (s+b)) - 1);
	*s_index = address >> b & ((1 << s)-1);
}

void calculate(char access_type, unsigned long address, int size,
	cache_line  **cache, unsigned long s_index, int E, unsigned long tag,
	int *hit, int *miss, int *evict, int verbose) {
	/*
	Calculate cache hit, miss and eviction number from a single operation.
	Note that cache can be modified in this function.
	*/
	int hit_tmp = 0, evict_tmp = 0, miss_tmp = 0;
	int n_empty = 0;
	enum STATE state;
	for (int i=0; i<E; i++) {
		if (cache[s_index][i].v == 0) {
			n_empty += 1;
			continue;
		}
		else if (cache[s_index][i].tag == tag) {
			hit_tmp += 1;
			cache[s_index][i].age = COUNTER;
			break;
		}
	}
	if (hit_tmp)
		state = HIT;
	else if (n_empty)
		state = MISS;
	else
		state = MISS_EVICT;
	
	switch (state) {
		case HIT:
			break;
		case MISS:
			for (int i=0; i<E; i++) {
				if (cache[s_index][i].v == 0) {
					cache[s_index][i].v = 1;
					cache[s_index][i].tag = tag;
					cache[s_index][i].age = COUNTER;
					break;
				}
			}
			break;
		case MISS_EVICT:;
			int idx = -1, MIN = INT_MAX;
		  	for (int i=0; i<E; i++) {
				if (cache[s_index][i].age < MIN) {
					MIN = cache[s_index][i].age;
					idx = i;
				}
			}
			cache[s_index][idx].v = 1;
			cache[s_index][idx].tag = tag;
			cache[s_index][idx].age = COUNTER;
			break;
		default:
			break;
	}	

    char str[1024];
    switch (access_type) {
		case 'S':
		case 'L':
			if (state == HIT)
				strcpy(str, "hit");
			else if (state == MISS) {
					miss_tmp += 1;
					strcpy(str, "miss");
				}
			else {
					miss_tmp += 1;
					evict_tmp += 1;
					strcpy(str, "miss eviction");
				}
				break;
		case 'M':
			if (state == HIT) {
					strcpy(str, "hit hit");
					hit_tmp += 1;
			}
			else if (state == MISS) {
				miss_tmp += 1;
				hit_tmp += 1;
				strcpy(str, "miss hit");
			}
			else {
				miss_tmp +=1;
				evict_tmp += 1;
				hit_tmp += 1;  
				strcpy(str, "miss eviction hit");
			}
			break;
		default:
			break;
		}
		if (verbose)
			printf("%c %lx, %d %s -- %lx, %lu\n", access_type, address, size, str, tag, s_index);
	*miss += miss_tmp;
	*hit += hit_tmp;
	*evict += evict_tmp;
	return;
}

int main(int argc, char *argv[])
{
	int s = -1, E = -1, b = -1;
	int verbose = 0;
	char *filename;
	int opt;
	while ((opt = getopt(argc, argv, "s:E:b:t:v")) != -1) {
		switch (opt) {
			case 's':
				s = atoi(optarg);
				break;
			case 'E':
				E = atoi(optarg);
				break;
			case 'b':
				b = atoi(optarg);
				break;
			case 't':
				filename = optarg;
				break;
			case 'v':
				verbose = 1;
				break;
			default:
				fprintf(stderr, "Error, non valid arguments\n");
				exit(-1);
		}
	}
	
	// check if all valid parameters are read
	if (s < 0 || E < 0 || b < 0 || filename == NULL) {
		fprintf(stderr, "Error, please provide all valid arguments, example: ./csim -s 3 -E 1 -b 2 -t ./traces/yi.trace\n");
		exit(-1);
	}

	unsigned long S = 1 << s;
	// initialize cache line
	cache_line **cache = (cache_line **)malloc(S*sizeof(cache_line *));
	for (int i=0; i<S; i++)
		cache[i] = (cache_line *)malloc(E*sizeof(cache_line));
	
	for (int i=0; i<S; i++) 
		for (int j=0; j<E; j++) 
			cache[i][j] = (cache_line){.v = 0, .tag=0};

	// read traces from file
	FILE *pfile;
	pfile = fopen(filename, "r");
	if (!pfile) {
		fprintf(stderr, "Error, the provided file is not found!\n");
		exit(-1);
	}
	char access_type;
	unsigned long address;
	int size;
	unsigned long tag;
	unsigned long s_index;
	int miss = 0, hit = 0, evict = 0;
	while (fscanf(pfile, " %c %lx, %d", &access_type, &address, &size) > 0) {
		if (access_type == 'I')
			continue;
		COUNTER += 1;
		process(address, s, b, &tag, &s_index);
		calculate(access_type, address, size, cache, s_index, E, tag, &hit, &miss, &evict, verbose);
	}
	fclose(pfile);
	printSummary(hit, miss, evict);
    return 0;
}
