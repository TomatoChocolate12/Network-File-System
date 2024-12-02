# gcc -o naming_server naming_server.c -lpthread
# gcc -o storage_server storage_server.c

all: naming_server storage_server client

naming_server: NamingServer/ns.c
	gcc -o naming_server NamingServer/ns.c NamingServer/log.c NamingServer/trie.c NamingServer/ns_commands.c NamingServer/lru.c Utils/errors.c -lpthread -fsanitize=address -g

storage_server: StorageServer/ss.c
	gcc -o storage_server StorageServer/server_commands.c  StorageServer/async.c Utils/errors.c StorageServer/log.c -lpthread -fsanitize=address -g

client: Clients/client.c
	gcc -o client Clients/client.c Utils/errors.c -lpthread -fsanitize=address -g

clean:
	rm -rf naming_server storage_server client naming_server.log Test/SS1/backup* Test/SS2/backup* Test/SS3/backup* Test/SS4/backup*
