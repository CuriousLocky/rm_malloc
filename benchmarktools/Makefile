CXX = gcc
CFLAGS = -pthread -fPIC -Og

BIN= gen_big_trace,linscal,shbench,threadTest,run_single,larson,auto_test,false_sharing

bin/gen_big_trace: src/gen_big_trace.c
	@$(CXX) $(CFLAGS) -o $@ $<

bin/linscal: src/linscal.c
	@$(CXX) $(CFLAGS) -o $@ $<

bin/largeTest: src/largeTest.c
	@$(CXX) $(CFLAGS) -o $@ $<

bin/threadTest: src/threadTest.c
	@$(CXX) $(CFLAGS) -o $@ $<

bin/run_single: src/run_single.c
	@$(CXX) $(CFLAGS) -o $@ $<

bin/larson: src/larson.c
	@$(CXX) $(CFLAGS) -o $@ $<

bin/larson_rm: src/larson_rm.c
	@$(CXX) $(CFLAGS) -o $@ $< -ldl 

bin/auto_test: src/auto_test.c
	@$(CXX) $(CFLAGS) -o $@ $< -ldl

bin/false_sharing: src/false_sharing.c
	@$(CXX) $(CFLAGS) -o $@ $<

dir:
	mkdir bin

all: dir bin/gen_big_trace bin/linscal bin/shbench bin/threadTest bin/run_single bin/larson bin/auto_test

.PHONY: clean
clean:
	rm -f bin/{$(BIN)}