#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

#include "../include/mh.h"

#define MUTATION_RATE 0.15
#define PRINT 0

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

double aplicar_mh(const double *d, int n, int m, int n_gen, int tam_pob, int *sol)
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
			mutar(poblacion[i], n, m);
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
	int corte = 1 + aleatorio(m - 1);

	// Los primeros genes del padre1 van al hijo1. Idem para el padre2 e hijo2.
	for (int i = 0; i < corte; i++) {
        hijo1->array_int[i] = padre1->array_int[i];
        hijo2->array_int[i] = padre2->array_int[i];
    }

	// Y los restantes son del otro padre, respectivamente.
	for (int i = corte; i < m; i++) {
        hijo1->array_int[i] = padre2->array_int[i];
        hijo2->array_int[i] = padre1->array_int[i];
    }
	
	// Factibilizar: eliminar posibles repetidos de ambos hijos
	// Si encuentro alguno repetido en el hijo1, lo cambio por otro que no este en el conjunto
	
	// Primero, recorro el hijo1 desde el corte hasta el final, 
	// y si encuentro un elemento que ya estaba en la primera parte del hijo1, 
	// lo reemplazo por un valor aleatorio que no esté en el hijo1.
	for (int i = corte; i < m; i++) {
        // Si el elemento proveniente del padre2 ya existe en el segmento izquierdo de hijo1
        if (find_element(hijo1->array_int, corte, hijo1->array_int[i])) {
            int nuevo_val;
            // Busca un valor aleatorio de [0, n-1] que no esté en hijo1
            do {
                nuevo_val = aleatorio(n);
            } while (find_element(hijo1->array_int, i, nuevo_val));
            hijo1->array_int[i] = nuevo_val;
        }
    }

	// Luego lo mismo, pero para el hijo2
	for (int i = corte; i < m; i++) {
        // Si el elemento proveniente del padre1 ya existe en el segmento izquierdo de hijo2
        if (find_element(hijo2->array_int, corte, hijo2->array_int[i])) {
            int nuevo_val;
            // Busca un valor aleatorio de [0, n-1] que no esté en hijo2
            do {
                nuevo_val = aleatorio(n);
            } while (find_element(hijo2->array_int, i, nuevo_val));
            hijo2->array_int[i] = nuevo_val;
        }
    }
}

void mutar(Individuo *actual, int n, int m)
{
	// Decidir cuantos elementos mutar:
	// Si el valor es demasiado pequeño la convergencia es muy pequeña y si es demasiado alto diverge
	int num_mutaciones = (int)(m * MUTATION_RATE);
    if (num_mutaciones < 1) num_mutaciones = 1; // Al menos 1 mutación por individuo
	
	// Cambia el valor de algunos elementos de array_int de forma aleatoria
	// teniendo en cuenta que no puede haber elementos repetidos:
        // una posibilidad podría ser usar una variable, m_rate, para establecer la intensidad de la mutación 
        // (un bucle for con un número de iteraciones que dependa, por ejemplo, de m_rate*m)
	for (int k = 0; k < num_mutaciones; k++) {
        // Selecciona una posición aleatoria para alterar
        int pos = aleatorio(m);
        int nuevo_val;
        
        // Genera un valor aleatorio que no esté repetido en el individuo
        do {
            nuevo_val = aleatorio(n);
        } while (find_element(actual->array_int, m, nuevo_val));
        
        actual->array_int[pos] = nuevo_val;
    }
}

double distancia_ij(const double *d, int i, int j, int n)
{
        // Devuelve la distancia entre dos elementos i, j de la matriz 'd'
}


void fitness(const double *d, Individuo *individuo, int n, int m)
{
	// Determina la calidad del individuo calculando la suma de la distancia entre cada par de enteros
}