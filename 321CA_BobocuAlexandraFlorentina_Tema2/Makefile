all: subscriber server
# Compileaza server.cpp
server:
	g++ -Wall -Wextra server.cpp -o server

# Compileaza subscriber.cpp
subscriber:
	g++ -Wall -Wextra subscriber.cpp -o subscriber

clean:
	rm -rf server subscriber
.PHONY: clean
