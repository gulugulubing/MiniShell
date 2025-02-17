myShell : shelpers.o
	g++ -std=c++17 -g shelpers.o -o myShell 
shelpers.o : shelpers.cpp shelpers.hpp
	g++ -std=c++17 -g -c shelpers.cpp


.PHONY : clean
clean:
	rm -rf *.o *.out myShell
