#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

#include "c-twist.h"

// from https://en.wikipedia.org/wiki/Wald–Wolfowitz_runs_test
double RunsStat(double *v, int N)
{
	int i;
	double n_plus = 0;
	double n_minus = 0;
	double mu;
	double sigma_sq;
	double stat;
	double runs;
	int b1;
	int b2;

	if(N == 0) {
		return(-1);
	}

	for(i=0; i < N; i++) {
		if(v[i] >= 0.5) {
			n_plus++;
		} else {
			n_minus++;
		}
	}

	mu = ((2*n_plus*n_minus) / (double)N) + 1.0;
	sigma_sq = ((mu - 1) * (mu - 2)) / ((double)N-1.0);

	runs = 1;

	for(i=1; i < N; i++) {
		if(v[i-1] >= 0.5) {
			b1 = 1;
		} else {
			b1 = 0;
		}
		if(v[i] >= 0.5) {
			b2 = 1;
		} else {
			b2 = 0;
		}

#ifdef DEBUG
		if(b1 == 1) {
			printf("+");
		} else {
			printf("-");
		}
#endif

		if(b1 != b2) {
			runs++;
		}
	}


#ifdef DEBUG
	if(b2 == 1) {
		printf("+\n");
	} else {
		printf("-\n");
	}
	printf("runs: %f\n",runs);
	fflush(stdout);
#endif

	stat = (runs - mu) / sqrt(sigma_sq);

	return(stat);
}

#ifdef STANDALONE 

#define ARGS "c:TS:"
char *Usage = "usage: c-runstest -c count\n\
\t-S seed\n\
\t-T <use threads>\n";

int UseThread;
uint32_t Seed;

int main(int argc, char **argv)
{
	int c;
	int count;
	int i;
	int has_seed;
	struct timeval tm;
	double *r;
	double stat;

	count = 10;
	UseThread = 0;
	has_seed = 0;

	while((c = getopt(argc,argv,ARGS)) != EOF) {
		switch(c) {
			case 'c':
				count = atoi(optarg);
				break;
			case 'T':
				UseThread = 1;
				break;
			case 'S':
				Seed = atoi(optarg);
				has_seed = 1;
				break;
			default:
				fprintf(stderr,"uncognized command -%c\n",
					(char)c);
				fprintf(stderr,"%s",Usage);
				exit(1);
		}
	}

	if(count <= 0) {
		fprintf(stderr,"count must be non-negative\n");
		fprintf(stderr,"%s",Usage);
		exit(1);
	}

	if(has_seed == 0) {
		gettimeofday(&tm,NULL);
		Seed = (uint32_t)((tm.tv_sec + tm.tv_usec) % 0xFFFFFFFF);
	}

	r = (double *)malloc(count * sizeof(double));
	if(r == NULL) {
		fprintf(stderr,"no space\n");
		exit(1);
	}

	CTwistInitialize(Seed);

	for(i=0; i < count; i++) {
		r[i] = CTwistRandom();
//		printf("r: %f\n",r[i]);
//		fflush(stdout);
	}

	stat = RunsStat(r,count);
	printf("runs statistic: %f\n",stat);

	return(0);
}

#endif
