CXX = gcc
CFLAGS = -I ./include/ -lpthread -fPIC -fvisibility=hidden -g -Og -nostdlib -D __NOISY_DEBUG

SRC_ALL=$(wildcard src/*.c) test/test.c
SRC=$(filter-out du , $(SRC_ALL))
OBJ=$(SRC:src/%.c=obj/%.o)
DEPS=$(wildcard include/*.h)
SRC_BENCHMARK=$(wildcard benchmarktools/src/*.c)

librm_malloc.so: $(DEPS) $(SRC)
	@$(CXX) $(CFLAGS) -DRUNTIME -ldl -shared $(DEPS) $(SRC) -o librm_malloc.so 

obj/datastructure_tree.o: src/datastructure_tree.c $(DEPS)
	@$(CXX) $(CFLAGS) -c -o $@ $<

obj/rm_malloc_interface.o: src/rm_malloc_interface.c $(DEPS)
	@$(CXX) $(CFLAGS) -c -o $@ $<

obj/rm_malloc.o: src/rm_malloc.c $(DEPS)
	@$(CXX) $(CFLAGS) -c -o $@ $<

obj/mempool.o: src/mempool.c $(DEPS)
	@$(CXX) $(CFLAGS) -c -o $@ $<

obj/datastructure_bitmap.o: src/datastructure_bitmap.c $(DEPS)
	@$(CXX) $(CFLAGS) -c -o $@ $<

obj/datastructure.o: src/datastructure.c $(DEPS)
	@$(CXX) $(CFLAGS) -c -o $@ $<

obj/datastructure_payload.o: src/datastructure_payload.c $(DEPS)
	@$(CXX) $(CFLAGS) -c -o $@ $<

# obj/test.o: test/test.c $(DEPS)
# 	@$(CXX) $(CFLAGS) -c -o $@ $<

# run2: $(OBJ)
# 	@$(CXX) $(CFLAGS) -o $@ $<

run: $(SRC_ALL) $(DEPS)
	@$(CXX) $(SRC_ALL) $(DEPS) $(CFLAGS) -o run

benchmark: $(SRC_BENCHMARK)
	cd benchmarktools; make clean; make all

.PHONY: clean
clean:
	rm -f obj/*.o *.so *.a run
