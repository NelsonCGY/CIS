TARGETS = keyValueStore masterNode storage admin mail SMTPserver httpServer penncloud loadbalancer
all: $(TARGETS)

%.o: %.cc
	g++ $^ -c -o $@

keyValueStore: keyValueStore.cc
	g++ -std=c++11 $^ -lpthread -g -o $@

masterNode: masterNode.cc
	g++ -std=c++11 $^ -lpthread -g -o $@

storage: storage.cc
	g++ -std=c++11 $^ -lpthread -g -o $@ -I ./cpp-base64/

admin: admin.cc
	g++ -std=c++11 $^ -lpthread -g -o $@ -I ./cpp-base64/

mail: mail.cc
	g++ -std=c++11 $^ -lpthread -lresolv -g -o $@

SMTPserver: SMTPserver.cc
	g++ -std=c++11 $^ -lpthread -lresolv -g -o $@

penncloud: penncloud.cc
	g++ -std=c++11 $^ -lpthread -lresolv -g -o $@

httpServer: httpServer.cc
	g++ -std=c++11 $^ -lpthread -g -o $@

loadbalancer: loadbalancer.cc
	g++ -std=c++11 $^ -lpthread -g -o $@

pack:
	rm -f Team-12.zip
	zip -r Team-12.zip *

clean::
	rm -fv $(TARGETS) *~ *.o Team-12.zip
