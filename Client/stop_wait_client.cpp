// Client side implementation of UDP client-server model
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <iostream>
#include <string>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <vector>
#include <signal.h>
#include <sstream>
#include <fstream>
#include <ctime>
#include <cstring>
#define MAXLINE 509
using namespace std;

uint32_t ack_no = 0;

/* Data-only packets */
struct packet {
    /* Header */
    uint16_t cksum; /* Optional bonus part */
    uint16_t len;
    uint32_t seqno;
    /* Data */
    char data [500]; /* Not always 500 bytes, can be less */
};

/* Ack-only packets are only 8 bytes */
struct ack_packet{
    uint16_t cksum; /* Optional bonus part */
    uint16_t len;
    uint32_t ackno;
};

vector<string> read_file(string file_name){
    vector<string> vec;
    fstream f(file_name, fstream::in );
    string line;
    while (getline(f, line)){
        vec.push_back(line);
    }
    return vec;
}

ack_packet* get_ack(){
    ack_packet* ack;
    ack->cksum = 0;
    ack->len = 8;
    return ack;
}
int send_ack(ack_packet* ack, int sockfd, sockaddr_in servaddr){
    char* buf;
    buf = (char*)ack;
    socklen_t len = sizeof(servaddr);
    ack_packet *pck= (ack_packet*) buf;
    int n = sendto(sockfd, (const char *)buf , 9,
		MSG_CONFIRM, (const struct sockaddr *) &servaddr,
			len);
    return n;
}

void store_file(string content){
    std::ofstream ofs;
    ofs.open ("requested_file", std::ofstream::out | std::ofstream::app);
    ofs << content;
    ofs.close();
}

int main() {

	int sockfd;
	char buffer[MAXLINE];
	struct sockaddr_in	 servaddr;

	// Creating socket file descriptor
	if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}

	memset(&servaddr, 0, sizeof(servaddr));
    vector <string> info =  read_file("client.in");
    char * file_name = &info[2][0];
    char* server = &info[0][0];
    char* port = &info[1][0];
	// Filling server information
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(stoi(info[1]));
	servaddr.sin_addr.s_addr = INADDR_ANY;
   // getaddrinfo(server, port, NULL, ( addrinfo**) &servaddr);
	int n;
    socklen_t len;
	sendto(sockfd, (const char *)file_name, strlen(file_name),
		0, (const struct sockaddr *) &servaddr,
			sizeof(servaddr));
    int cntDuplicate = 0;
    while(true){


        n = recvfrom(sockfd, (char *)buffer, MAXLINE,
                    0, (struct sockaddr *) &servaddr,
                    &len);

            buffer[n] = '\0';
            packet *temp= (packet*) buffer;
            if(temp->seqno == 2){
                break;
            }
            ack_packet* ack = get_ack();
            printf("SEQ      %d  \n", temp->seqno);
            if (temp->seqno == ack_no){
                //printf("%s\n", temp->data);
                store_file(temp->data);
                cntDuplicate  = 0;
                ack->ackno = ack_no;
                //  cout << ack->ackno <<endl;
                n = send_ack(ack, sockfd, servaddr);
                ack_no = !ack_no;

            }else if(cntDuplicate<1){
                printf("**************************************************************************\n");
                printf("Not Expected packet no. %d, Packet %d is lost!\n", temp->seqno, ack_no);
                printf("**************************************************************************\n");
                cntDuplicate ++;
                ack->ackno = 3;
                n = send_ack(ack, sockfd, servaddr);
            }
    }

	close(sockfd);
	return 0;
}
