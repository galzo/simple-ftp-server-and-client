/**
 * OS Ex6
 * Gal Zohar, galzo
 *
 * The client program. opens a connection to a server (whose details
 * are given as input arguments) and sends a file (whose path is given
 * as input argument) to the server.
 */

#include <iostream>
#include <fstream>
#include <netdb.h>
#include <strings.h>
#include <sstream>

#define SUCCESS 0
#define BAD_EXIT 1
#define FAILURE -1
#define DEFAULT_PROTOCOL 0
#define SERVER_PORT 1
#define SERVER_HOSTNAME 2
#define LOCAL_FILENAME 3
#define SERVER_FILENAME 4
#define BUFFER_SEND_SIZE 1024

/**
 * This is the client class, which is in charge of establishing
 * connection to the remote server - and send data to it, according
 * to our "file transfer protocol"
 */
class Client {
public:
	Client();
	~Client();
	int establishConnection(int serverPort, char* serverHost);
	int writeToServer(std::string filePath, std::string serverFname);

private:
	int serverSocket;
	std::ifstream fileToWrite;
};

/**
 * The client's constructor
 */
Client::Client() :
		serverSocket(-1) {}

/**
 * The client's destructor
 */
Client::~Client() {
	if (this->serverSocket != -1) {
		close(this->serverSocket);
		std::cout << "disconnected" << std::endl;
	}

	if (this->fileToWrite.is_open()) {
		this->fileToWrite.close();
	}
}

/**
 * Creates a socket and opens a connection to the remote server.
 * returns 0 on success, -1 on failure.
 * if connection succeeded - serverSocket field can be used to communicate
 * with remote server.
 */
int Client::establishConnection(int serverPort, char* serverHost) {
	hostent* serverEntry = NULL;
	sockaddr_in serverAddr = {};

	//get server details, according to given hostname
	serverEntry = gethostbyname(serverHost);
	if (serverEntry == NULL) {
		std::cerr << "ERROR: unable to get server host details" << std::endl;
		return FAILURE;
	}

	//create a TCP socket
	this->serverSocket = socket(AF_INET, SOCK_STREAM, DEFAULT_PROTOCOL);
	if (this->serverSocket == FAILURE) {
		std::cerr << "ERROR: unable to create client socket" << std::endl;
		return FAILURE;
	}

	serverAddr.sin_family = AF_INET;
	bcopy((char *)serverEntry->h_addr_list[0],
		  (char *)&serverAddr.sin_addr.s_addr, serverEntry->h_length);
	serverAddr.sin_port = htons(serverPort);

	//connect to the remote server
	int ret = connect(this->serverSocket,((struct sockaddr*)&serverAddr),sizeof(serverAddr));
	if (ret < 0) {
		std::cerr << "ERROR: unable to connect to server" << std::endl;
		return FAILURE;
	}

	return SUCCESS;
}

/**
 * reads the file specified in filePath and transfers its data
 * to the server in chunks. the remote server creates a new file
 * whose name is specified in the given serverFname parameter and whose
 * contents are equivalent to the given file's contents.
 *
 * returns 0 if transfer was successfully completed, and -1 otherwise.
 */
int Client::writeToServer(std::string filePath, std::string serverFname) {
	char buffer[BUFFER_SEND_SIZE];
	std::stringstream dataStream;
	std::streambuf *filebuf;
	int fileSize;

	/*
	 * open the file with two flags:
	 * binary flag, which instructs to read file in binary mode
	 * ate flag, which instructs to start reading the file from its end position
	 * (this is used to measure the file's size)
	 *
	 */
	this->fileToWrite.open(filePath.c_str(), std::ios::binary | std::ios::ate);
	if (!this->fileToWrite.good()) {
		std::cerr << "ERROR: unable to open file" << std::endl;
		return FAILURE;
	}

	//measure the file size
	fileSize = this->fileToWrite.tellg();
	if (fileSize < 0) {
		std::cerr << "ERROR: unable to measure file size" << std::endl;
		return FAILURE;
	}

	//set the position to the file's beginning - to start reading
	this->fileToWrite.seekg(0, std::ios::beg);
	if (!this->fileToWrite.good()) {
		std::cerr << "ERROR: unable to seek file's beginning" << std::endl;
		return FAILURE;
	}

	dataStream << serverFname << '\n';
	dataStream << fileSize  << '\n';
	filebuf = this->fileToWrite.rdbuf();
	dataStream << filebuf;

	while (dataStream) {
		//start writing to server in chunk sizes of 1024
		dataStream.read(buffer, BUFFER_SEND_SIZE);
		int toWrite = dataStream.gcount();
		int offset = 0;
		while (toWrite > 0) {
			int bwritten = write(this->serverSocket, buffer + offset, toWrite);
			if (bwritten < 0) {
				std::cerr << "ERROR: unable to write to server" << std::endl;
				return FAILURE;
			}
			toWrite -= bwritten;
			offset += bwritten;
		}
	}

	return SUCCESS;
}

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

int main(int argc, char** argv) {
	Client client;

	//validate that port number is a legal integer
	if (!isInteger(argv[SERVER_PORT])) {
		std::cerr << "ERROR: invalid port number argument" << std::endl;
		exit(BAD_EXIT);
	}
	std::cout << "connecting to server..." << std::endl;
	//try establishing connection with server
	if (client.establishConnection(atoi(argv[SERVER_PORT]), argv[SERVER_HOSTNAME])) {
		exit(BAD_EXIT);
	}
	std::cout << "connected" << std::endl;
	std::cout << "sending data to server..." << std::endl;
	//send data to server
	if (client.writeToServer(argv[LOCAL_FILENAME], argv[SERVER_FILENAME])) {
		exit(BAD_EXIT);
	}
	std::cout << "done sending data, disconnecting..." << std::endl;

	return SUCCESS;
}


