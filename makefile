CXX = gcc
CFLAGS = -lpthread -D DMEM_AVL -fPIC -O0 -I ./include/

SRC_ALL=$(wildcard src/*.c) test/test.c
SRC=$(filter-out du , $(SRC_ALL))
OBJ=$(SRC:src/%.c=obj/%.o) obj/test.o
DEPS=$(wildcard include/*.h)

autotest_ori: src/autotest_ori.c
	@$(CXX) $(CFLAGS) -o $@ $<

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

obj/test.o: test/test.c $(DEPS)
	@$(CXX) $(CFLAGS) -c -o $@ $<

run2: $(OBJ)
	@$(CXX) $(CFLAGS) -o $@ $<

run: $(SRC_ALL) $(DEPS)
	@$(CXX) $(SRC_ALL) $(DEPS) $(CFLAGS) -o run

.PHONY: clean
clean:
	rm -f obj/*.o *.so *.a run
