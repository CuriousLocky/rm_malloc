CXX = gcc
CFLAGS = -pthread -fPIC -Og

BIN= linux_scale,shbench,threadTest,larson,auto_test,false_sharing

bin/linux_scale: src/linux_scale.c
	@$(CXX) $(CFLAGS) -o $@ $<

bin/largeTest: src/largeTest.c
	@$(CXX) $(CFLAGS) -o $@ $<

bin/threadTest: src/threadTest.c
	@$(CXX) $(CFLAGS) -o $@ $<

bin/larson: src/larson.c
	@$(CXX) $(CFLAGS) -o $@ $< -ldl

bin/auto_test: src/auto_test.c
	@$(CXX) $(CFLAGS) -o $@ $< -ldl

bin/false_sharing: src/false_sharing.c
	@$(CXX) $(CFLAGS) -o $@ $<

bin/shbench: src/shbench.c
	@$(CXX) $(CFLAGS) -o $@ $<

dir:
	mkdir bin

all: dir bin/linux_scale bin/shbench bin/threadTest bin/larson bin/auto_test 

.PHONY: clean
clean:
	rm -f bin/{$(BIN)}