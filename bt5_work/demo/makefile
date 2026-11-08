TARGET = demo
OBJECTS = $(patsubst %.cpp, %.o, $(wildcard *.cpp)) 
CPPFLAGS+= -std=c++11 -I /opt/datas/lf_basket_trade/include -I /opt/datas/lf_basket_trade/include/kernel -I /opt/datas/lf_basket_trade/include/include
LDFLAGS+= -lrt -L/opt/datas/lf_basket_trade/lib -Wl,-rpath,/opt/datas/lf_basket_trade/lib -Wl,-rpath-link,/opt/datas/lf_basket_trade/lib -llf_utillq -llf_bt5 -llquant_kernel 

all: ${OBJECTS}
	echo ${OBJECTS}
	g++ ${OBJECTS} -o ${TARGET} $(CPPFLAGS) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f ${OBJECTS} ${TARGET}
