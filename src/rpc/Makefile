-include ../../Global.mk

ROOTDIR = ../..

all: $(LIB_DIR)/libmr-rpc.a

$(LIB_DIR)/libmr-rpc.a: mapreduce
	$(AR) $@ $</*.o

mapreduce: mapreduce.thrift
	rm -rf $@
	thrift -r --gen cpp $<
	mv gen-cpp $@
	$(CC) $(CPPFLAGS) $(INC) -c $@/mapreduce_types.cpp -o $@/mapreduce_types.o
	$(CC) $(CPPFLAGS) $(INC) -c $@/Master.cpp -o $@/Master.o
	mkdir -p $(INC_DIR)/rpc/mapreduce
	cp $@/*.h $(INC_DIR)/rpc/mapreduce

clean:
	rm -rf $(LIB_DIR)/libmr-rpc.a mapreduce
	rm -rf $(INC_DIR)/rpc/mapreduce
