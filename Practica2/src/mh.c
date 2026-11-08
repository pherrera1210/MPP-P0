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

// Tags de los mensajes
#define TAG_MIGRA 10 // isla -> maestro: migrantes
#define TAG_REPLY 20 // maestro -> isla: individuos de vuelta
#define TAG_FINAL 30 // isla -> maestro: mejor individuo final

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
	// qsort pasa un puntero al elemento que está ordenando
	// Se compara sin restar
	double fa = (*(Individuo **)a)->fitness;
	double fb = (*(Individuo **)b)->fitness;
	return (fb > fa) - (fb < fa); // orden descendente
}

// Copia un individuo (solo los m genes útiles + el fitness)
static void copiar_individuo(Individuo *dst, const Individuo *src, int m)
{
	memcpy(dst->array_int, src->array_int, m * sizeof(int));
	dst->fitness = src->fitness;
}

// Sustituye a los NEM peores individuos de la isla por los NEM migrantes recibidos
static void incorporar_migrantes(Individuo **poblacion, int tam_pob_isla, const Individuo *migrantes, int m)
{
	for (int i = 0; i < NEM; i++)
		copiar_individuo(poblacion[tam_pob_isla - NEM + i], &migrantes[i], m);
	qsort(poblacion, tam_pob_isla, sizeof(Individuo *), comp_fitness);
}

/* MAESTRO: una vez recibidos los migrantes de TODAS las islas, mezcla y ordena la
   élite global, devuelve NEM individuos a cada isla con MPI_Isend y se queda con los mejores NEM. */
static void procesar_migracion_maestro(Individuo **poblacion, int tam_pob_isla, int m, int num_procs,
                                       Individuo *recv_all, Individuo *send_all, Individuo **orden,
                                       MPI_Request *req_send, MPI_Datatype tipo)
{
	int total = num_procs * NEM;

	// Los envíos de la migración anterior deben haber terminado antes de reutilizar send_all
	MPI_Waitall(num_procs - 1, req_send, MPI_STATUSES_IGNORE);

	// mezcla y ordenación global
	for (int k = 0; k < total; k++) orden[k] = &recv_all[k];
	qsort(orden, total, sizeof(Individuo *), comp_fitness);

	// se copian ordenados a un array contiguo de envío
	for (int k = 0; k < total; k++) copiar_individuo(&send_all[k], orden[k], m);

	// envío asíncrono de un único mensaje de NEM individuos a cada isla
	for (int p = 1; p < num_procs; p++)
		MPI_Isend(&send_all[p * NEM], NEM, tipo, p, TAG_REPLY, MPI_COMM_WORLD, &req_send[p - 1]);

	// el maestro incorpora a su isla los mejores NEM individuos globales
	incorporar_migrantes(poblacion, tam_pob_isla, send_all, m);
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

	// crea poblacion inicial
	// cada proceso con su propia subpoblacion
	int tam_pob_isla = tam_pob / num_procs;
	Individuo **poblacion = (Individuo **) malloc(tam_pob_isla * sizeof(Individuo *));
	assert(poblacion);
	
	// crea cada individuo
	for(i = 0; i < tam_pob_isla; i++) {
		poblacion[i] = (Individuo *) malloc(sizeof(Individuo));
		crear_individuo(n, m, poblacion[i]->array_int);
		
		// calcula el fitness del individuo
		fitness(d, poblacion[i], n, m);
	}
	
	// ordena individuos segun la funcion de bondad
	qsort(poblacion, tam_pob_isla, sizeof(Individuo *), comp_fitness);
	
	int hay_migracion = (num_procs > 1);
	int pendiente = 0; // 1 = hay una migración en curso (respuesta del maestro aún no incorporada)

	// --- islas (rank != 0)
	Individuo *env = NULL, *rec = NULL;
	MPI_Request req_env = MPI_REQUEST_NULL, req_rec = MPI_REQUEST_NULL;

	// --- maestro (rank == 0)
	Individuo *recv_all = NULL, *send_all = NULL, **orden = NULL;
	MPI_Request *req_recv = NULL, *req_send = NULL;

	if (rank != 0) {
		env = (Individuo *) malloc(NEM * sizeof(Individuo));
		rec = (Individuo *) malloc(NEM * sizeof(Individuo));
		assert(env && rec);
	} else {
		recv_all = (Individuo *) malloc(num_procs * NEM * sizeof(Individuo));
		send_all = (Individuo *) malloc(num_procs * NEM * sizeof(Individuo));
		orden    = (Individuo **) malloc(num_procs * NEM * sizeof(Individuo *));
		req_recv = (MPI_Request *) malloc(num_procs * sizeof(MPI_Request));
		req_send = (MPI_Request *) malloc(num_procs * sizeof(MPI_Request));
		assert(recv_all && send_all && orden && req_recv && req_send);
		for (i = 0; i < num_procs; i++) req_recv[i] = req_send[i] = MPI_REQUEST_NULL;
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

		if (hay_migracion) {
			int es_migracion = (g > 0) && (g % NGM == 0);

			if (rank != 0) {
				// Islas
				if (es_migracion) {
					// 1) cerrar la migración anterior si seguía pendiente
					if (pendiente) {
						MPI_Wait(&req_rec, MPI_STATUS_IGNORE);
						incorporar_migrantes(poblacion, tam_pob_isla, rec, m);
						pendiente = 0;
					}
					// 2) el buffer de envío anterior debe estar libre antes de rellenarlo
					MPI_Wait(&req_env, MPI_STATUS_IGNORE);

					// 3) copia contigua de los NEM mejores y lanzamiento asíncrono
					for (i = 0; i < NEM; i++) copiar_individuo(&env[i], poblacion[i], m);
					MPI_Isend(env, NEM, MPI_INDIVIDUO, 0, TAG_MIGRA, MPI_COMM_WORLD, &req_env);
					MPI_Irecv(rec, NEM, MPI_INDIVIDUO, 0, TAG_REPLY, MPI_COMM_WORLD, &req_rec);
					pendiente = 1;
					// la isla continúa evolucionando mientras viajan los mensajes
				} else if (pendiente) {
					int flag;
					MPI_Test(&req_rec, &flag, MPI_STATUS_IGNORE);
					if (flag) { // ya ha llegado la respuesta del maestro: se incorpora
						incorporar_migrantes(poblacion, tam_pob_isla, rec, m);
						pendiente = 0;
					}
				}
			} else {
				// Maestro
				if (es_migracion) {
					// 1) cerrar la migración anterior si seguía pendiente
					if (pendiente) {
						MPI_Waitall(num_procs - 1, req_recv, MPI_STATUSES_IGNORE);
						procesar_migracion_maestro(poblacion, tam_pob_isla, m, num_procs, recv_all, send_all,
						                           orden, req_send, MPI_INDIVIDUO);
						pendiente = 0;
					}
					// 2) sus propios NEM mejores ocupan las primeras posiciones del buffer global
					for (i = 0; i < NEM; i++) copiar_individuo(&recv_all[i], poblacion[i], m);

					// 3) una recepción asíncrona por isla (cada una a su zona del buffer)
					for (int p = 1; p < num_procs; p++)
						MPI_Irecv(&recv_all[p * NEM], NEM, MPI_INDIVIDUO, p, TAG_MIGRA, MPI_COMM_WORLD, &req_recv[p - 1]);
					pendiente = 1;
					// el maestro continúa evolucionando su isla mientras llegan los migrantes
				} else if (pendiente) {
					int flag;
					MPI_Testall(num_procs - 1, req_recv, &flag, MPI_STATUSES_IGNORE);
					if (flag) { // han llegado los migrantes de todas las islas
						procesar_migracion_maestro(poblacion, tam_pob_isla, m, num_procs, recv_all, send_all,
						                           orden, req_send, MPI_INDIVIDUO);
						pendiente = 0;
					}
				}
			}
		}
	}

	/* Al acabar el bucle puede quedar una migración en curso: se completa antes de elegir la mejor solución */
	if (hay_migracion) {
		if (rank != 0) {
			if (pendiente) {
				MPI_Wait(&req_rec, MPI_STATUS_IGNORE);
				incorporar_migrantes(poblacion, tam_pob_isla, rec, m);
				pendiente = 0;
			}
			MPI_Wait(&req_env, MPI_STATUS_IGNORE);
		} else {
			if (pendiente) {
				MPI_Waitall(num_procs - 1, req_recv, MPI_STATUSES_IGNORE);
				procesar_migracion_maestro(poblacion, tam_pob_isla, m, num_procs, recv_all, send_all,
				                           orden, req_send, MPI_INDIVIDUO);
				pendiente = 0;
			}
			MPI_Waitall(num_procs - 1, req_send, MPI_STATUSES_IGNORE);
		}
	}

	// elección de la mejor solución global (también asíncrona)
	double mejor_valor_final = 0.0;
	if (rank != 0) {
		MPI_Request req_fin;
		MPI_Isend(poblacion[0], 1, MPI_INDIVIDUO, 0, TAG_FINAL, MPI_COMM_WORLD, &req_fin);
		MPI_Wait(&req_fin, MPI_STATUS_IGNORE); // poblacion[0] no se modifica hasta aquí
	} else {
		Individuo *mejor_global = poblacion[0];

		Individuo *finales = (Individuo *) malloc(num_procs * sizeof(Individuo));
		MPI_Request *req_fin = (MPI_Request *) malloc(num_procs * sizeof(MPI_Request));
		assert(finales && req_fin);

		// se lanzan todas las recepciones y se procesan según van llegando (MPI_Waitany)
		for (int p = 1; p < num_procs; p++)
			MPI_Irecv(&finales[p - 1], 1, MPI_INDIVIDUO, p, TAG_FINAL, MPI_COMM_WORLD, &req_fin[p - 1]);

		for (int k = 1; k < num_procs; k++) {
			int idx;
			MPI_Waitany(num_procs - 1, req_fin, &idx, MPI_STATUS_IGNORE);
			if (finales[idx].fitness > mejor_global->fitness)
				copiar_individuo(mejor_global, &finales[idx], m);
		}

		qsort(mejor_global->array_int, m, sizeof(int), comp_array_int);
		memmove(sol, mejor_global->array_int, m * sizeof(int));
		mejor_valor_final = mejor_global->fitness;

		free(finales);
		free(req_fin);
	}

	// liberación de memoria
	if (rank == 0) {
		free(recv_all);
		free(send_all);
		free(orden);
		free(req_recv);
		free(req_send);
	} else {
		free(env);
		free(rec);
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