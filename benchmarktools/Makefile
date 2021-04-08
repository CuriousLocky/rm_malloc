CXX = gcc
CFLAGS = -pthread -fPIC

BIN= gen_big_trace linscal shbench threadTest run_single larson

gen_big_trace: src/gen_big_trace.c
	@$(CXX) $(CFLAGS) -o $@ $<

linscal: src/linscal.c
	@$(CXX) $(CFLAGS) -o $@ $<

shbench: src/shbench.c
	@$(CXX) $(CFLAGS) -o $@ $<

threadTest: src/threadTest.c
	@$(CXX) $(CFLAGS) -o $@ $<

run_single: src/run_single.c
	@$(CXX) $(CFLAGS) -o $@ $<

larson: src/larson.c
	@$(CXX) $(CFLAGS) -o $@ $<

all: $(BIN)

.PHONY: clean
clean:
	rm -f $(BIN)