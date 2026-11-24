#ifndef _MH
#define _MH
	
#define MAX 1000

	typedef struct {
		int array_int[MAX];
		double fitness;
	} Individuo;
	
	void cruzar(Individuo *, Individuo *, Individuo *, Individuo *, int, int);
	void mutar(Individuo *, int, int, double);
	void fitness(const double *, Individuo *, int, int);
#endif
