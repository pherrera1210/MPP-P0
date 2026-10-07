#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

#include "../include/mh.h"

#define PRINT 0
#define PORCENTAJE_CONVERGENCIA 0.05
#define MAX_ITER_SIN_MEJORA 5

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

int *crear_individuo(int n, int m)
{
	int i=0, value;
	int *individuo = (int *) malloc(m * sizeof(int));
	
	// inicializa array de elementos
	memset(individuo, -1, m * sizeof(int));
	
	while(i < m) {
		value = aleatorio(n);
		// si el nuevo elemento no está en el array...
		if(!find_element(individuo, i, value)) {
			individuo[i] = value;  // lo incluimos
			i++;
		}
	}
	return individuo;
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
        // para reiniciar la secuencia pseudoaleatoria en cada ejecución
        srand(time(NULL) + getpid());
	int i, g, mutation_start;
	
	// crea poblacion inicial (array de individuos)
	Individuo **poblacion = (Individuo **) malloc(tam_pob * sizeof(Individuo *));
	assert(poblacion);
	
	// crea cada individuo (array de enteros aleatorios)
	for(i = 0; i < tam_pob; i++) {
		poblacion[i] = (Individuo *) malloc(sizeof(Individuo));
		poblacion[i]->array_int = crear_individuo(n, m);
		
		// calcula el fitness del individuo
		fitness(d, poblacion[i], n, m);
	}
	
	// ordena individuos segun la funcion de bondad (mayor "fitness" --> mas aptos)
	qsort(poblacion, tam_pob, sizeof(Individuo *), comp_fitness);
	
	// Variables para el criterio de convergencia
	double mejor_fitness_previo = poblacion[0]->fitness;
	int iter_sin_mejora = 0;
	
	// evoluciona la poblacion durante un numero de generaciones
	for(g = 0; g < n_gen; g++)
	{
		// los hijos de los ascendientes mas aptos sustituyen a la ultima mitad de los individuos menos aptos
		for(i = 0; i < (tam_pob/2) - 1; i += 2) {
			cruzar(poblacion[i], poblacion[i+1], poblacion[tam_pob/2 + i], poblacion[tam_pob/2 + i + 1], n, m);
		}
		
		// inicia la mutacion a partir de 1/4 de la poblacion
		mutation_start = tam_pob/4;
		
		// muta 3/4 partes de la poblacion
		for(i = mutation_start; i < tam_pob; i++) {
			mutar(poblacion[i], n, m, m_rate);
		}
		
		// recalcula el fitness del individuo
		for(i = 0; i < tam_pob; i++) {
			fitness(d, poblacion[i], n, m);
		}
		
		// ordena individuos segun la funcion de bondad (mayor "fitness" --> mas aptos)
		qsort(poblacion, tam_pob, sizeof(Individuo *), comp_fitness);
		
		if (PRINT) {
			printf("Generacion %d - ", g);
			printf("Fitness = %.0lf\n", poblacion[0]->fitness);
		}
		
		// Criterio de convergencia
		double fitness_actual = poblacion[0]->fitness;
		double mejora_minima = mejor_fitness_previo * PORCENTAJE_CONVERGENCIA;
		
		// Si el fitness ha mejorado más del porcentaje mínimo exigido, actualizamos
		if (fitness_actual >= (mejor_fitness_previo + mejora_minima)) {
			mejor_fitness_previo = fitness_actual;
			iter_sin_mejora = 0; // Reiniciamos el contador
		} else {
			iter_sin_mejora++;
		}
		
		// Si superamos las iteraciones permitidas sin mejora, paramos
		if (iter_sin_mejora >= MAX_ITER_SIN_MEJORA) {
			break;
		}
	}
	
	// ordena el array solucion
	qsort(poblacion[0]->array_int, m, sizeof(int), comp_array_int);

        // y lo mueve a sol para escribirlo
	memmove(sol, poblacion[0]->array_int, m*sizeof(int));
	
	// almacena el mejor valor obtenido para el fitness
	double value = poblacion[0]->fitness;
	
	// Liberamos los punteros y sus arrays
    for(i = 0; i < tam_pob; i++) {
        free(poblacion[i]->array_int);
        free(poblacion[i]);
	}
	// se libera la memoria reservada
	free(poblacion);
	
	// devuelve el valor obtenido para el fitness
	return value;
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
