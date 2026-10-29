CC = mpicc
CFLAGS = -O3 -Wall -std=c99 -g -fopenmp

N = 
M = 
N_GEN = 
T_POB = 
M_RATE =

EXEC_SEC = sec
EXEC_PARALLEL = parallel
OUTFILE = ../output/out
C_FILES = main.c io.c mh.c

# Versión secuencial
sec: $(C_FILES)
	$(CC) $(CFLAGS) $(C_FILES) -o $(EXEC_SEC) -DTIME -DDEBUG

test_sec:
	./$(EXEC_SEC) $(N) $(M) $(N_GEN) $(T_POB) $(M_RATE) >> $(OUTFILE)_$(N)_$(M)_$(N_GEN)_$(T_POB)_$(M_RATE).txt


# Versión paralela
parallel: $(C_FILES)
	$(CC) $(CFLAGS) $(C_FILES) -o $(EXEC_PARALLEL) -DTIME -DDEBUG

test_parallel:
	mpirun -np $(HILOS) ./$(EXEC_PARALLEL) $(N) $(M) $(N_GEN) $(T_POB) $(M_RATE) >> $(OUTFILE)_$(N)_$(M)_$(N_GEN)_$(T_POB)_$(M_RATE)_hilos$(HILOS).txt

clean:
	rm -f $(EXEC_SEC) $(EXEC_PARALLEL)
