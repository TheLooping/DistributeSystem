TARGETS = rpc mMapReduce
.PHONY: $(TARGETS) count-line clean

all: $(TARGETS)

rpc:
	make -C src/$@

MapReduce: rpc
	make -C src/$@

count-line:
	find . -type f    \
		| grep -E ".*\.(cpp|h|hpp|sh|py|thrift)|Makefile"   \
		| grep -v -E "rpc/(MapReduce|KVRaft)/.*"  \
		| grep -v "include/rpc"  \
		| xargs wc -l \

clean:
	rm -rf objs/*
	make -C src/MapReduce clean
	make -C src/rpc clean