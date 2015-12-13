#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef COUNTS
unsigned nswaps;
unsigned ncmps;
#define countswap (nswaps++)
#define countcmp  (ncmps++)
#else
#define countswap ((void)0)
#define countcmp  ((void)0)
#endif

typedef void *Tuplesortstate;
typedef long long int Datum;
typedef int  bool;

typedef struct
{
	void	   *tuple;			/* the tuple proper */
	Datum		datum1;			/* value of first key column */
	bool		isnull1;		/* is first key column NULL? */
	int			tupindex;		/* see notes above */
} SortTuple;

typedef SortTuple elem_t;

typedef int (*SortTupleComparator) (const SortTuple *a, const SortTuple *b,
									Tuplesortstate *state);

typedef struct SortSupportData *SortSupport;

static int comparator(Datum a, Datum b, SortSupport ssup) {
	if (b > a)
		return -1;
	else if (b < a)
		return 1;
	else return 0;
};

struct SortSupportData {
	bool ssup_nulls_first;
	bool ssup_reverse;
	int (*comparator)(Datum,Datum,SortSupport);
};

struct SortSupportData ssup = {0, 0, comparator};

static inline int
ApplySortComparator(Datum datum1, bool isNull1,
					Datum datum2, bool isNull2,
					SortSupport ssup)
{
	int			compare;

	countcmp;

	if (isNull1)
	{
		if (isNull2)
			compare = 0;		/* NULL "=" NULL */
		else if (ssup->ssup_nulls_first)
			compare = -1;		/* NULL "<" NOT_NULL */
		else
			compare = 1;		/* NULL ">" NOT_NULL */
	}
	else if (isNull2)
	{
		if (ssup->ssup_nulls_first)
			compare = 1;		/* NOT_NULL ">" NULL */
		else
			compare = -1;		/* NOT_NULL "<" NULL */
	}
	else
	{
		compare = (*ssup->comparator) (datum1, datum2, ssup);
		if (ssup->ssup_reverse)
			compare = -compare;
	}

	return compare;
}

#define CHECK_FOR_INTERRUPTS() do {} while (0)

#define Min(a,b) ((a)<(b)?(a):(b))
#define Max(a,b) ((a)<(b)?(b):(a))

#include "qsort_tuple.c"

#define mycmp(a,b)					\
	(ApplySortComparator(list[a].datum1, list[a].isnull1, \
						 list[b].datum1, list[b].isnull1, \
						 &ssup))

#define myswap(a,b)					\
	do {							\
		elem_t _tmp;				\
		_tmp = list[a];				\
		list[a] = list[b];			\
		list[b] = _tmp;				\
		countswap;					\
	} while (0)

#define myswapif(a,b)				\
	do {							\
		if (mycmp(a,b) > 0)			\
			myswap(a,b);				\
	} while (0)


static int insertion_ok(unsigned N) {
	return N<1000;
}

static void insertion(elem_t *list, unsigned N) {
	if (N > 1000) {
		printf("insertion sort not feasible for large N\n");
		exit(1);
	}

	for (unsigned pm = 1; pm < N; pm++)
		for (unsigned pl = pm; pl && mycmp(pl-1, pl) > 0; pl--)
			myswap(pl, pl-1);
}

static int sort_networks_ok(unsigned N) {
	return N<=8;
}

static void sort_networks(elem_t *list, unsigned N) {
	if (N > 8) {
		printf("sort_networks only implemented for N in 0..8\n");
		exit(1);
	}
	switch(N) {
	case 0: 
	case 1:
		break;
		
	case 2:
		myswapif(0,1);
		break;
		
	case 3:
		myswapif(0,1); myswapif(1,2);
		myswapif(0,1);
		break;
		
	case 4:
		myswapif(0,1); myswapif(2,3);
		myswapif(1,3); myswapif(0,2);
		myswapif(1,2);
		break;
		
	case 5:
		myswapif(1,2); myswapif(3,4);
		myswapif(1,3); myswapif(0,2);
		myswapif(2,4); myswapif(0,3);
		myswapif(0,1); myswapif(2,3);
		myswapif(1,2);
		break;
		
	case 6:
		myswapif(0,1); myswapif(2,3); myswapif(4,5);
		myswapif(0,2); myswapif(3,5); myswapif(1,4);
		myswapif(0,1); myswapif(2,3); myswapif(4,5);
		myswapif(1,2); myswapif(3,4);
		myswapif(2,3);
		break;
		
	case 7:
		myswapif(1,2); myswapif(3,4); myswapif(5,6);
		myswapif(0,2); myswapif(4,6); myswapif(3,5);
		myswapif(2,6); myswapif(1,5); myswapif(0,4);
		myswapif(2,5); myswapif(0,3);
		myswapif(2,4); myswapif(1,3);
		myswapif(0,1); myswapif(2,3); myswapif(4,5);
		break;
		
	case 8:
		myswapif(0, 1); myswapif(2, 3); myswapif(4, 5); myswapif(6, 7);
		myswapif(0, 3); myswapif(1, 2); myswapif(4, 7); myswapif(5, 6);
		myswapif(0, 1); myswapif(2, 3); myswapif(4, 5); myswapif(6, 7);
		myswapif(0, 7); myswapif(1, 6); myswapif(2, 5); myswapif(3, 4);
		myswapif(1, 3); myswapif(0, 2); myswapif(5, 7); myswapif(4, 6);
		myswapif(0, 1); myswapif(2, 3); myswapif(4, 5); myswapif(6, 7);
		break;
	}
}

static int bitonic_ok(unsigned N) {
	return (N&(N-1))==0;
}

static void bitonic_serial(elem_t *list, unsigned N) {

	if (N & (N-1)) {
		printf("bitonic sort implemented only for powers of 2\n");
		exit(1);
	}

    int i,j,k;
    for (k=2;k<=N;k=2*k) {
		for (j=k>>1;j>0;j=j>>1) {
			for (i=0;i<N;i++) {
				int ixj=i^j;
				if (ixj > i) {
					if ((i & k) == 0)
						myswapif(i,ixj);
					if ((i & k) != 0)
						myswapif(ixj, i);
				}
			}
		}
    }
}

static int qsort_compare(const void *_a, const void *_b) {
	const elem_t *a = _a, *b = _b;

	return ApplySortComparator(a->datum1, a->isnull1,
							   b->datum1, b->isnull1,
							   &ssup);
}

static void quicksort(elem_t *list, unsigned N) {
	qsort(list, N, sizeof(elem_t), qsort_compare);
}


static void test_qsort_ssup(elem_t *list, unsigned N) {
	qsort_ssup(list, N, &ssup);
}

static elem_t *generate_list(unsigned N) {
	elem_t *list = aligned_alloc(256, sizeof(elem_t) * N);
    memset(list, 0, sizeof(elem_t) * N);
	for (unsigned i=0 ; i<N; i++) {
		SortTuple x = {NULL, 0, 0, 0};
		x.datum1 =  (Datum)rand() << (8*(sizeof(x.datum1) - sizeof(int)));
		list[i] = x;
	}

	return list;
}	


static void time_sort(char *label, unsigned seed, unsigned N, unsigned M, void (*sort)(elem_t*, unsigned)) {
	unsigned reps = M/N;
	elem_t **lists = malloc(sizeof(elem_t*) * reps);

	srand(seed);
	for (unsigned i=0; i<reps; i++)
		lists[i] = generate_list(N);

#ifdef COUNTS
	nswaps = 0;
	ncmps = 0;
#endif

	struct timespec t1, t2;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t1);
	for (unsigned i=0;i<reps; i++)
		sort(lists[i], N);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t2);

	for (unsigned i=0; i<reps; i++)
		free(lists[i]);
	free(lists);

	double elapsed = ((double)
					  (t2.tv_sec - t1.tv_sec) * 1000 * 1000 * 1000 +
					  (t2.tv_nsec - t1.tv_nsec)) / reps;

	char *units = "ns";
	if (elapsed > 1000) {
		elapsed /= 1000;
		units = "us";
		if (elapsed > 1000) {
			elapsed /= 1000;
			units = "ms";
			if (elapsed > 1000) {
				elapsed /= 1000;
				units = "s";
				if (elapsed > 119) {
					elapsed /= 60;
					units = "m";
					if (elapsed > 119) {
						elapsed /= 60;
						units = "h";
					}
				}
			}
		}
	}

	printf("using %s sort %0.3f%s per sort of %u %lu-byte items",
		   label,
		   elapsed, units,
		   N, sizeof(elem_t)/sizeof(char));
#ifdef COUNTS
	if (ncmps)
		printf(" %0.1f compares/sort", (double)ncmps/reps);
	if (nswaps)
		printf(" %0.1f swaps/sort", (double)nswaps/reps);
#endif
	printf("\n");
}

struct sort {
	char *label;
	void (*func)(elem_t*, unsigned);
	int (*ok)(unsigned);
} sorts[] = {
	{"bitonic", bitonic_serial, bitonic_ok},
	{"insertion", insertion, insertion_ok},
	{"sort networks", sort_networks, sort_networks_ok},
	{"libc quicksort", quicksort, NULL},
	{"qsort_ssup", test_qsort_ssup, NULL},

};

#define NSORTS (sizeof(sorts)/sizeof(*sorts))

static void time_several_sorts(unsigned N, unsigned M) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	unsigned seed = getpid() ^ tv.tv_sec ^ tv.tv_usec;
	srand(seed);

	seed = rand();
	for (unsigned i=0; i<NSORTS; i++) {
		if (!sorts[i].ok || sorts[i].ok(N))
			time_sort(sorts[i].label, seed, N, M, sorts[i].func);
	}
}

int main (int argc, char *argv[], char *envp[]) {
	unsigned N;
	if (argc > 1) {
		N = atoi(argv[1]);
	} else {
		N = 16384;
	}


	time_several_sorts(N, 1<<23);
}
