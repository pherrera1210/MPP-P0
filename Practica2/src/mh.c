#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <stddef.h>
#include <mpi.h>
#include "../include/mh.h"

#define PRINT 0
#define PORCENTAJE_CONVERGENCIA 0.05
#define MAX_ITER_SIN_MEJORA 5

// Parámetros de migración
#define NGM 10 // Frecuencia de migración (cada 10 generaciones)
#define NEM 2  // Número de individuos migrantes por isla

void crear_tipo_datos(int m, MPI_Datatype *tipo_individuo);

int aleatorio(int n) {
	return (rand() % n);  // genera un numero aleatorio entre 0 y n-1
}

int find_element(int *array, int end, int element)
{
	int i=0;
	int found=0;
	
	// comprueba que un elemento no está incluido en el individuo (el cual no admite enteros repetidos)
	while((i < end) && ! found) {
		if(array[i] == element) {
			found = 1;
		}
		i++;
	}
	return found;
}

int *crear_individuo(int n, int m, int *array_int)
{
	int i=0, value;
	
	// inicializa array de elementos
	memset(array_int, -1, m * sizeof(int));
	
	while(i < m) {
		value = aleatorio(n);
		// si el nuevo elemento no está en el array...
		if(!find_element(array_int, i, value)) {
			array_int[i] = value;  // lo incluimos
			i++;
		}
	}
	return array_int;
}

int comp_array_int(const void *a, const void *b) {
	return (*(int *)a - *(int *)b);
}

int comp_fitness(const void *a, const void *b) {
	/* qsort pasa un puntero al elemento que está ordenando */
	return (*(Individuo **)b)->fitness - (*(Individuo **)a)->fitness;
}

double aplicar_mh(const double *d, int n, int m, int n_gen, int tam_pob, double m_rate, int *sol)
{
	int rank, num_procs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    // para reiniciar la secuencia pseudoaleatoria en cada ejecución
	// añadimos el rank del proceso para que cada isla tenga una secuencia diferente
    srand(time(NULL) + getpid() + rank);
	int i, g, mutation_start;
	
	// definimos el tipo derivado de MPI para Individuo
    MPI_Datatype MPI_INDIVIDUO;
    crear_tipo_datos(m, &MPI_INDIVIDUO);

	// crea poblacion inicial (array de individuos)
	// cada proceso con su propia subpoblacion
	int tam_pob_isla = tam_pob / num_procs;
	Individuo **poblacion = (Individuo **) malloc(tam_pob_isla * sizeof(Individuo *));
	assert(poblacion);
	
	// crea cada individuo (array de enteros aleatorios)
	for(i = 0; i < tam_pob_isla; i++) {
		poblacion[i] = (Individuo *) malloc(sizeof(Individuo));
		crear_individuo(n, m, poblacion[i]->array_int);
		
		// calcula el fitness del individuo
		fitness(d, poblacion[i], n, m);
	}
	
	// ordena individuos segun la funcion de bondad (mayor "fitness" --> mas aptos)
	qsort(poblacion, tam_pob_isla, sizeof(Individuo *), comp_fitness);
	
	// buffers para las migraciones
    Individuo **buffer_master = NULL;
    if (rank == 0) {
        buffer_master = (Individuo **) malloc(num_procs * NEM * sizeof(Individuo *));
        for (i = 0; i < num_procs * NEM; i++) {
			buffer_master[i] = (Individuo *) malloc(sizeof(Individuo));
        }
    }
	
	// evoluciona la poblacion durante un numero de generaciones
	for(g = 0; g < n_gen; g++)
	{

		// cruce local
		// los hijos de los ascendientes mas aptos sustituyen a la ultima mitad de los individuos menos aptos
		for(i = 0; i < (tam_pob_isla/2) - 1; i += 2) {
			cruzar(poblacion[i], poblacion[i+1], poblacion[tam_pob_isla/2 + i], poblacion[tam_pob_isla/2 + i + 1], n, m);
		}
		
		// mutacion local
		// inicia la mutacion a partir de 1/4 de la poblacion
		mutation_start = tam_pob_isla / 4;
		for(i = mutation_start; i < tam_pob_isla; i++) {
			mutar(poblacion[i], n, m, m_rate);
		}
		
		// evaluación del fitness local
		// recalcula el fitness del individuo
		for(i = 0; i < tam_pob_isla; i++) {
			fitness(d, poblacion[i], n, m);
		}
		
		// ordena individuos segun la funcion de bondad (mayor "fitness" --> mas aptos)
		qsort(poblacion, tam_pob_isla, sizeof(Individuo *), comp_fitness);
		
		if (PRINT) {
			printf("Generacion %d - ", g);
			printf("Fitness = %.0lf\n", poblacion[0]->fitness);
		}

		// migración síncrona con MPI_ANY_SOURCE y MPI_STATUS_IGNORE
        if ((g > 0) && (g % NGM == 0)) {
            if (rank != 0) {
                // las islas envían sus NEM mejores individuos al maestro
                for (i = 0; i < NEM; i++) {
                    MPI_Send(poblacion[i], 1, MPI_INDIVIDUO, 0, 10, MPI_COMM_WORLD);
                }
                // reciben de vuelta los nuevos individuos sustitutos
                for (i = 0; i < NEM; i++) {
					int idx = tam_pob_isla - NEM + i;
					// MPI_STATUS_IGNORE sirve para ignorar el estado de la recepción
                    MPI_Recv(poblacion[idx], 1, MPI_INDIVIDUO, 0, 20, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                }
            } else {
                // el maestro (Rank 0) recopila sus propios individuos primero
                for (i = 0; i < NEM; i++) {
                    memcpy(buffer_master[i]->array_int, poblacion[i]->array_int, m * sizeof(int));
                    buffer_master[i]->fitness = poblacion[i]->fitness;
                }

                // recibe de las islas usando MPI_ANY_SOURCE para poder recibir de cualquier proceso en cualquier orden
                MPI_Status status;
                for (int p = 1; p < num_procs; p++) {
                    for (i = 0; i < NEM; i++) {
                        int idx = p * NEM + i;
                        MPI_Recv(buffer_master[idx], 1, MPI_INDIVIDUO, MPI_ANY_SOURCE, 10, MPI_COMM_WORLD, &status);
                    }
                }

                // ordena la élite global mezclada
                qsort(buffer_master, num_procs * NEM, sizeof(Individuo *), comp_fitness);

                // devuelve los mejores seleccionados a cada isla
                for (int p = 1; p < num_procs; p++) {
                    for (i = 0; i < NEM; i++) {
                        int idx = p * NEM + i;
                        MPI_Send(buffer_master[idx], 1, MPI_INDIVIDUO, p, 20, MPI_COMM_WORLD);
                    }
                }
                // el maestro reemplaza los suyos
                for (i = 0; i < NEM; i++) {
					int idx = tam_pob_isla - NEM + i;
                    memcpy(poblacion[idx]->array_int, buffer_master[i]->array_int, m * sizeof(int));
                    poblacion[idx]->fitness = buffer_master[i]->fitness;
                }
            }
            qsort(poblacion, tam_pob_isla, sizeof(Individuo *), comp_fitness);
        }

	}

	// elección de la mejor solución global
    double mejor_valor_final = 0.0;
    if (rank != 0) {
		MPI_Send(poblacion[0], 1, MPI_INDIVIDUO, 0, 10, MPI_COMM_WORLD);
    } else {
        Individuo *mejor_global = poblacion[0];
        
        Individuo candidato;

        for (int p = 1; p < num_procs; p++) {
			MPI_Recv(&candidato, 1, MPI_INDIVIDUO, MPI_ANY_SOURCE, 10, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            if (candidato.fitness > mejor_global->fitness) {
                memcpy(mejor_global->array_int, candidato.array_int, m * sizeof(int));
                mejor_global->fitness = candidato.fitness;
            }
        }
        
        qsort(mejor_global->array_int, m, sizeof(int), comp_array_int);
        memmove(sol, mejor_global->array_int, m * sizeof(int));
        mejor_valor_final = mejor_global->fitness;
    }

	if (rank == 0) {
        for (i = 0; i < num_procs * NEM; i++) {
            free(buffer_master[i]);
        }
        free(buffer_master);
    }
    for (i = 0; i < tam_pob_isla; i++) {
        free(poblacion[i]);
    }
    free(poblacion);

    MPI_Type_free(&MPI_INDIVIDUO);
    return mejor_valor_final;
}

void cruzar(Individuo *padre1, Individuo *padre2, Individuo *hijo1, Individuo *hijo2, int n, int m)
{
	// Elegir un "punto" de corte aleatorio a partir del que se realiza el intercambio de los genes
	int corte = 1 + aleatorio(m-1);
	
	// Los primeros genes del padre1 van al hijo1. Idem para el padre2 e hijo2.
	for (int i=0; i<corte; i++) {
		hijo1->array_int[i] = padre1->array_int[i];
		hijo2->array_int[i] = padre2->array_int[i];
	}
	
	// Y los restantes son del otro padre, respectivamente.
	for (int i=corte; i<m; i++) {
		hijo1->array_int[i] = padre2->array_int[i];
		hijo2->array_int[i] = padre1->array_int[i];
	}
	
	// Factibilizar: eliminar posibles repetidos de ambos hijos
	// Hijo 1: reemplazamos duplicados comprobando desde el inicio hasta la posición actual i
	for (int i=corte; i<m; i++) {
		if (find_element(hijo1->array_int, corte, hijo1->array_int[i])) {
			int nuevo_val;
			do {
				nuevo_val = aleatorio(n);
			} while (find_element(hijo1->array_int, i, nuevo_val));
			hijo1->array_int[i] = nuevo_val;
		}
	}
	
	// Hijo 2:
	for (int i=corte; i<m; i++) {
		if (find_element(hijo2->array_int, corte, hijo2->array_int[i])) {
			int nuevo_val;
			do {
				nuevo_val = aleatorio(n);
			} while (find_element(hijo2->array_int, i, nuevo_val));
			hijo2->array_int[i] = nuevo_val;
		}
	}
}

void mutar(Individuo *actual, int n, int m, double m_rate)
{
	// Decidir cuantos elementos mutar:
	// Si el valor es demasiado pequeño la convergencia es muy pequeña y si es demasiado alto diverge
	int num_mutaciones = (int)(m*m_rate);
	if (num_mutaciones < 1) num_mutaciones = 1; 
	
	// Cambia el valor de algunos elementos del array_int de forma aleatoria
	for (int k=0; k<num_mutaciones; k++) {
		int pos = aleatorio(m);
		int nuevo_val;
		
		do {
			nuevo_val = aleatorio(n);
		} while (find_element(actual->array_int, m, nuevo_val));
		
		actual->array_int[pos] = nuevo_val;
	}
}

double distancia_ij(const double *d, int i, int j, int n)
{
        // Devuelve la distancia entre dos elementos i, j de la matriz 'd'
        // Como existe simetría aseguramos que i < j
        if (i == j) return 0;
        if (i > j) {
        	int temp = i;
        	i = j;
        	j = temp;
        }
        
        // Aplicamos la fórmula k = f (i, j, n)
        int k = ((n*n-n)/2) - (((n-i) * (n-i) - (n-i)) / 2) + j - i - 1;
        
        return d[k];
}


void fitness(const double *d, Individuo *individuo, int n, int m)
{
	// Determina la calidad del individuo calculando la suma de la distancia entre cada par de enteros
	double suma_distancias = 0.0;
	
	// Doble bucle para recorrer todos los pares de elementos seleccionados
	for (int i=0; i<m-1; i++) {
		for (int j=i+1; j<m; j++) {
			int elem1 = individuo->array_int[i];
			int elem2 = individuo->array_int[j];
			suma_distancias += distancia_ij(d, elem1, elem2, n);
		}
	}
	
	// Asignamos el valor objetivo maximizado al fitness del individuo
	individuo->fitness = suma_distancias;
}


/**
 *  Crea un nuevo tipo de datos derivado en MPI
 *  Necesario para el envio/recepcion de mensajes con datos de tipo Individuo
 **/
void crear_tipo_datos(int m, MPI_Datatype *individuo_type)
{
	int blocklen[2] = {m, 1};
	MPI_Datatype dtype[2] = { MPI_INT, MPI_DOUBLE };
	
	MPI_Aint disp[2];
	disp[0] = offsetof(Individuo, array_int);
	disp[1] = offsetof(Individuo, fitness);
	
	MPI_Type_create_struct(2, blocklen, disp, dtype, individuo_type); 
	MPI_Type_commit(individuo_type);
}