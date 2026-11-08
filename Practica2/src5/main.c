#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <mpi.h>

#include "../include/io.h"

extern double aplicar_mh(const double *, int, int, int, int, double, int *);

static double mseconds() {
	struct timeval t;
	gettimeofday(&t, NULL);
	return t.tv_sec*1000 + t.tv_usec/1000;
}

int main(int argc, char **argv)
{
	// inicialización del entorno MPI
    MPI_Init(&argc, &argv);

//	Check Number of Input Args
	if(argc < 6) {
		fprintf(stderr,"Ayuda:\n"); 
		fprintf(stderr,"  ./programa n m nGen tamPob tamRate\n");
		MPI_Finalize();
		return(EXIT_FAILURE);
	}
	
	int n = atoi(argv[1]);
	int m = atoi(argv[2]);
	int n_gen = atoi(argv[3]);
	int tam_pob = atoi(argv[4]);
	double m_rate = atof(argv[5]);
	
//	Check that 'm' is less than 'n'
	assert(m < n);
	
//	Generate matrix D with distance values among elements
	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	int n_dist = (n * n - n) / 2;
	double *d;

	if (rank == 0) {
		d = read_distances(n);
	} else {
		d = (double *) malloc(n_dist * sizeof(double));
		assert(d);
	}
	
	MPI_Bcast(d, n_dist, MPI_DOUBLE, 0, MPI_COMM_WORLD);
	
	#ifdef DEBUG
	//	print_distances(d, n);
	#endif
	
	//	Allocate memory for output data
	int *sol = (int *) malloc(m * sizeof(int));
	
	#ifdef TIME
		// sincroniza todas las islas antes de medir
		MPI_Barrier(MPI_COMM_WORLD); 
		double ti = mseconds();
	#endif
	
	//	Call Metaheuristic
	double value = aplicar_mh(d, n, m, n_gen, tam_pob, m_rate, sol);

	#ifdef TIME
		// espera a que todas terminen para congelar el reloj
		MPI_Barrier(MPI_COMM_WORLD);
		double tf = mseconds();
		if (rank == 0)
			printf("Execution Time: %.2lf sec\n", (tf - ti)/1000);
	#endif
	
	
	#ifdef DEBUG
		if (rank == 0)
			print_solution(n, m, sol, value);
	#endif
	
	//	Free Allocated Memory	
	free(sol);

	free(d);

	// finalización del entorno MPI
	MPI_Finalize();

	return(EXIT_SUCCESS);
}