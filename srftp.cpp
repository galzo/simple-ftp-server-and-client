/**
 * OS Ex6
 * Gal Zohar, galzo
 *
 * The Server program. binds a socket to a user given port
 * and listens for incoming connections. manages all connected clients
 * and reads data transferred to it from them. for each such client - creates
 * a file, whose name is specified by the client, and copies any data
 * transferred by the client into it.
 */

#include <string>
#include <iostream>
#include <netinet/in.h>
#include <map>
#include <fstream>
#include <vector>
#include <sys/time.h>
#include <errno.h>

#define SUCCESS 0
#define BAD_EXIT 1
#define FAILURE -1
#define DEFAULT_PROTOCOL 0
#define SERVER_PORT 1
#define BUF_READ_SIZE 4096
#define MICRO_TO_SECS 1000000
#define SECS_TO_MICRO 1000000

/**
 * Helper function to check if a given string represents a valid integer.
 * returns true if so, and false otherwise.
 */
static bool isInteger(const std::string & s)
{
   if(s.empty() || ((!isdigit(s[0])) && (s[0] != '-') && (s[0] != '+'))) return false ;

   char * p ;
   strtol(s.c_str(), &p, 10) ;

   return (*p == 0) ;
}

/**
 * A Struct which represents a connected client's data.
 * used by the server to track the state of the client, what data was
 * already read from it, and what is the expected filesize.
 * also holds a pointer to the file to which the server writes the given
 * client data.
 */
struct ClientRecord {
	ClientRecord() :
		state("init"),
		data(""),
		fileSize(0),
		timeSec(0) {
		//connection started - start measuring time
		if (gettimeofday(&this->tBefore, NULL) == FAILURE) {
			this->timeSec = FAILURE;
		}
	}

	~ClientRecord() {
		//connection ended - measure end time
		if (gettimeofday(&this->tAfter, NULL) == FAILURE) {
			this->timeSec = FAILURE;
		}

		//if output file is open - close it.
		if (this->fileToWrite.is_open()) {
			std::cout << "file is open- closing" << std::endl;
			int loc = this->fileToWrite.tellp();
			this->fileToWrite.close();
			//check if file was not fully transferred - remove it
			if (loc != this->fileSize) {
				std::cout << "file is incomplete - removing it" << std::endl;
				remove(this->fileName.c_str());
			}
		}

		//if time measurements succeeded - measure total passed time in seconds
		if (this->timeSec != FAILURE) {
			timersub(&this->tAfter, &this->tBefore , &this->tPassed);
			long timeAgo = ((tPassed.tv_sec * SECS_TO_MICRO) + tPassed.tv_usec);
			this->timeSec = ((double)(timeAgo)) / MICRO_TO_SECS;
			std::cout << "total time passed: " << this->timeSec << std::endl;
		}
	}

	std::ofstream fileToWrite;
	std::string state, fileName, data;
	int fileSize;
	double timeSec; 				   //used to measure performance
	timeval tBefore, tAfter, tPassed; //used to measure performance
};

/**
 * This is the server class, which is in charge of managing new client
 * connections, client disconnections,
 * and the handling of existing client connections: this includes
 * reading data sent from them, processing it, and writing it to
 * relevant files.
 */
class Server {
public:
	Server();
	~Server();
	int establishConnection(int port);
	int run();

private:
	int serverFd;
	std::map<int, ClientRecord*> clientRecords;

	int readFromClient(int fd);
	int clientConnect();
	void clientDisconnect(int fd);
	inline void getFileName(ClientRecord &client);
	inline int getFileSize(ClientRecord &client);
	inline int getFileData(ClientRecord &client);
};

/**
 * Server constructor.
 */
Server::Server() :
	serverFd(-1){}

/**
 * Server destructor.
 * automatically disconnects any connected clients (if there are any such)
 */
Server::~Server() {
	if (this->serverFd != -1) {
		std::cout << "server closing" << std::endl;
		close(this->serverFd);
	}

	//if there are any remaining clients which are still connected
	//to the server - disconnect them.
	std::vector<int> disconnectFds;
	if (!this->clientRecords.empty()) {
		for (auto &client : this->clientRecords) {
			disconnectFds.push_back(client.first);
		}

		for (int fd : disconnectFds) {
			clientDisconnect(fd);
		}
	}

	std::cout << "server closed" << std::endl;
}

/**
 * Creates a socket, binds it to a user given port number,
 * and start listening through that port for any new incoming
 * connections.
 *
 * returns 0 on success, -1 on failure.
 * if connection succeeded - serverFd field can be used to accept
 * new incoming connections.
 */
int Server::establishConnection(int port) {
	sockaddr_in serv_addr = {};
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);

	//Creates a new socket for the server
	this->serverFd = socket(AF_INET, SOCK_STREAM, DEFAULT_PROTOCOL);
	if (this->serverFd == FAILURE) {
		std::cerr << "ERROR: unable to create server socket" << std::endl;
		return FAILURE;
	}

	//binds the given socket to the given port
	int ret = bind(this->serverFd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
	if (ret < 0) {
		std::cerr << "ERROR: unable to bind socket to server" << std::endl;
		return FAILURE;
	}

	//starts listening for incomming connections
	if (listen(this->serverFd, SOMAXCONN)) {
		std::cerr << "ERROR: unable to listen to bound socket" << std::endl;
		return FAILURE;
	}

	std::cout << "server bound and running" << std::endl;
	return SUCCESS;
}

/**
 * Start running the server - and manage all
 * connections and disconnections - this includes connecting new clients,
 * handling existing clients data and disconnecting clients.
 *
 * returns 0 on success and -1 in case of a serious error.
 */
int Server::run() {
	while(true) {
		std::vector<int> disconnectClients; //used to mark any clients that should be disconnected
		fd_set socketFds, badFds;

		FD_ZERO(&socketFds);
		FD_SET(this->serverFd, &socketFds);
		for (auto &client : this->clientRecords) {
			FD_SET(client.first, &socketFds);
		}
		badFds = socketFds;

		//check what is the current maxFd (used by the select function)
		int maxFd = this->serverFd;
		for (auto &client : this->clientRecords) {
			maxFd = (maxFd < client.first)? client.first : maxFd;
		}

		std::cout << "waiting for connections..." << std::endl;
		int ret = select(maxFd+1, &socketFds, NULL, &badFds, NULL);

		if (ret < 0) {
			switch (errno) {
				case EBADF:
					//if server socket is problematic
					if (FD_ISSET(this->serverFd, &badFds)) {
						FD_CLR(this->serverFd, &socketFds);
					}

					//check if any of the client fd's is problematic
					//if it is - add the client to disconnection list.
					for (auto &client : this->clientRecords) {
						if (FD_ISSET(client.first, &badFds)) {
							disconnectClients.push_back(client.first);
							FD_CLR(client.first, &socketFds);
						}
					}

					break;

				case ENOMEM:
					std::cerr << "ERROR: no memory for new tables allocation" << std::endl;
					break;

				default:
					break;
				}
		}

		//check if any of the clients wrote data or disconnected from server
		for (auto &client : this->clientRecords) {
			if (FD_ISSET(client.first, &socketFds)) {
				std::cout << "handling client number " << client.first << std::endl;
				if (readFromClient(client.first) == FAILURE) {
					disconnectClients.push_back(client.first);
				}
			}
		}

		//check if there are any new connections waiting to the server
		if (FD_ISSET(this->serverFd, &socketFds)) {
			std::cout << "new client connection awaiting..." << std::endl;
			clientConnect();
		}

		//handle all client disconnections
		for (int fd : disconnectClients) {
			std::cout << "disconnecting client (fd number " << fd << ") from server" << std::endl;
			clientDisconnect(fd);
		}
	}

	return SUCCESS;
}

/**
 * Reads any data written by the client. if read operation was successful
 * check what is the client state - and perform actions according to its state.
 *
 * on success returns 0, and failure returns -1.
 * value of -1 is used by the run function to mark the current client
 * as a client that should be disconnected from the server.
 */
int Server::readFromClient(int fd) {
	char buffer[BUF_READ_SIZE];
	int bread = read(fd, buffer, BUF_READ_SIZE);

	//if bytes read is zero - its our cue that the client wants to disconnect
	if (bread == 0) {
		return FAILURE;
	}

	//if there was an error reading from client
	if (bread < 0) {
		std::cerr << "ERROR: unable to read from client" << std::endl;
		return FAILURE;
	}

	//else - if everything's fine - we should start reading from client
	else {
		ClientRecord& client = *(this->clientRecords[fd]);
		std::cout << "reading data from client number: " << fd << std::endl;
		for (int i = 0; i < bread; i++) {
			client.data += buffer[i];
		}

		//client was just connected and this is the first data chunk sent from it
		if (client.state == "init") {
			client.state = "searching_file_name";
		}
		//server expects client to send a filename, followed by a '\n' character.
		if (client.state == "searching_file_name") {
			getFileName(client);
		}
		//server expects client to send filesize, followed by a '\n' character.
		if (client.state == "searching_file_size") {
			if (getFileSize(client) == FAILURE) {
				return FAILURE;
			}
		}
		//server finished reading fileName and fileSize and opens a new target file to write
		//any data that'll be streamed by the client from this point and on
		if (client.state == "start_reading_data") {
			client.fileToWrite.open(client.fileName.c_str(), std::ios::binary | std::ios::out);
			if (!client.fileToWrite.good()) {
				return FAILURE;
			}
			client.state = "reading_data";
		}
		//server expects file data to be sent by the client.
		//any data recieved will be written into the file opened by the server.
		if (client.state == "reading_data") {
			if (getFileData(client) == FAILURE) {
				return FAILURE;
			}
		}
	}

	return SUCCESS;
}

/**
 * Reads, or tries to read the fileName specified by the client
 * from the data that was received thus far.
 *
 * if filename was found in the data, updates the client's filename
 * field and updates the server state, in order to try reading filesize.
 */
inline void Server::getFileName(ClientRecord &client) {
	size_t loc = client.data.find('\n');
	//if '\n' character was found - retrieve the filename and update server state
	//otherwise - wait for the next data chunk(s), which should include the fileName.
	if (loc != std::string::npos) {
		client.fileName = client.data.substr(0, loc);
		client.data = client.data.substr(loc + 1, std::string::npos);
		client.state = "searching_file_size";
	}
}

/**
 * Reads, or tries to read the fileSize specified by the client
 * from the data that was received thus far.
 *
 * if filesize was found in the data, updates the client's filename
 * field and updates the server state, in order to try reading the file's data
 * for writing.
 *
 * returns 0 on success (or if filesize was not yet found)
 * and -1 in case filesize is not a valid integer.
 */
inline int Server::getFileSize(ClientRecord &client) {
	size_t loc = client.data.find('\n');
	//if '\n' character was found - retrieve the filesize, validate it and update server state
	//otherwise - wait for the next data chunks(s), which should include the fileSize.
	if (loc != std::string::npos) {
		if (!isInteger(client.data.substr(0, loc))) {
			std::cerr << "ERROR: invalid filesize argument given by client" << std::endl;
			return FAILURE;
		}

		client.fileSize = atoi(client.data.substr(0, loc).c_str());
		client.data = client.data.substr(loc + 1, std::string::npos);
		client.state = "start_reading_data";
	}

	return SUCCESS;
}

/**
 * Read the data sent from the client and write it to the file opened by the server
 *
 * returns 0 on a successful write operation, and -1 on failure.
 */
inline int Server::getFileData(ClientRecord &client) {
	std::cout << "writing data to file..." << std::endl;
	client.fileToWrite << client.data;
	if (!client.fileToWrite.good()) {
		std::cerr << "ERROR: unable to write into file" << std::endl;
		return FAILURE;
	}

	client.data.clear();
	return SUCCESS;
}

/**
 * Handles a new client connection.
 * if a new client is indeed connected to server - create
 * a record storing all of the client's relevant data and add it
 * to a map, using the client's file descriptor as the key value.
 *
 * returns 0 on a successful connection, and -1 on failure.
 */
int Server::clientConnect() {
	int clientFd = accept(this->serverFd, NULL, NULL);
	if (clientFd == FAILURE) {
		std::cerr << "ERROR: unable to accept new client connection" << std::endl;
		return FAILURE;
	}

	std::cout << "new client is connected" << std::endl;
	ClientRecord* newClient = new ClientRecord;
	this->clientRecords[clientFd] = newClient;

	return SUCCESS;
}

/**
 * Handles a client disconnection:
 * removes its record from the client records map, closes its
 * file descriptor and deallocates its record's data from the heap.
 *
 * freeing the client record's data from the heap also causes the underlying file
 * into which the server was writing data, to be closed.
 */
void Server::clientDisconnect(int fd) {
	ClientRecord* currClient = this->clientRecords[fd];
	this->clientRecords.erase(fd);
	delete currClient;
	close(fd);

	std::cout << "client disconnected" << std::endl;
}

int main(int argc, char** argv) {
	Server server;

	//validate that port number is a legal integer
	if (!isInteger(argv[SERVER_PORT])) {
		std::cerr << "ERROR: invalid port number argument" << std::endl;
		exit(BAD_EXIT);
	}
	//bind server to socket and start listening on given port
	if (server.establishConnection(atoi(argv[SERVER_PORT]))) {
		exit(BAD_EXIT);
	}
	//start waiting for incoming connections
	if (server.run()) {
		exit(BAD_EXIT);
	}

	return SUCCESS;
}
