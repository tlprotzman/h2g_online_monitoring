all:
	g++ -g -O3 -shared -fPIC `root-config --cflags` `root-config --ldflags` `root-config --glibs` -lRHTTP *.cxx -o libMonitoring.so
exec:
	g++ -g -fPIC `root-config --cflags` `root-config --ldflags` `root-config --glibs` -lRHTTP *.cxx -o test.out
