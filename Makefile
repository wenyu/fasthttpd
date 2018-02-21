CXX      = g++
CXXFLAGS = -O3
LD       = g++
LDFLAGS  = -O3 -lpthread -ldl

TARGET	 = myhttpd
SRCS	 = main.cc \
		server.cc iterative_server.cc fork_server.cc thread_server.cc pool_server.cc \
		service.cc http_service.cc

OBJS	 = $(SRCS:.cc=.o)

all: $(TARGET)
.SUFFIXES: .cc .o
.PHONY: all clean

$(TARGET): $(OBJS)
	$(LD) $^ $(LDFLAGS) -o $@

.cc.o:
	$(CXX) $(CXXFLAGS) -c $^ -o $@

clean:
	rm -f $(TARGET) $(OBJS)

cleanall: clean
	rm -f *~ \#* *.o gmon.out core
	chmod 600 *.cc *.h Makefile
