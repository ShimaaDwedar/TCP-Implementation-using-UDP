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
#define MAXLINE 508
#define max_seq UINT32_MAX
using namespace std;

int ack_no = 1;
int sockfd;
char * file_name;
char* server;
char* port;
int cntDuplicate = 0;
char buffer[MAXLINE];
struct sockaddr_in servaddr, cliaddr;

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

ack_packet get_ack(){
    ack_packet ack;
    ack.cksum = 0;
    ack.len = 8;
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

void set_seconds(int sec, int sockfd){
    //set timeout on receining acks
    struct timeval tv;
    tv.tv_sec = sec;
    tv.tv_usec = 100000;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
        perror("Error");
    }
}

int receive_ack(int sockfd, sockaddr_in cliaddr){
    char buf[sizeof(ack_packet)+1];
    socklen_t len = sizeof(cliaddr);
    cout << "Iam hereee" <<endl;
    set_seconds(10, sockfd);
    int no_bytes = recvfrom(sockfd, (char *)buf, 9,
                    0, (struct sockaddr *) &cliaddr,
                    &len);
    cout << no_bytes <<endl;
    return no_bytes;

}


void request_file(char *file_name){
	int ns = sendto(sockfd, (const char *)file_name, strlen(file_name),
		0, (const struct sockaddr *) &servaddr,
			sizeof(servaddr));

    int rcv = receive_ack(sockfd, servaddr);
    if (rcv == -1){
        cout << "Requested File doesn't reach the server..!"<<endl;
        exit(0);
    }
}

bool check_fin(packet *temp){
    return temp->seqno == max_seq;
}

packet* receive_pck(){
    socklen_t len = sizeof(cliaddr);
    int n = recvfrom(sockfd, (char *)buffer, MAXLINE,
                    0, (struct sockaddr *) &servaddr,
                    &len);

    buffer[n] = '\0';
    packet *temp= (packet*) buffer;
    return temp;

}


bool correct_ack(packet *pck){
    return pck->seqno == ack_no-1;
}

void new_pck_actions(packet *pck, ack_packet ack){
    store_file(pck->data);
    cntDuplicate  = 0;
    ack.ackno = ack_no;
    send_ack(&ack, sockfd, servaddr);
    ack_no++;
}

void lost_pck_actions(packet *pck, ack_packet ack){
    printf("**************************************************************************\n");
    printf("Not Expected packet no. %d, Packet %d is lost!\n", pck->seqno, ack_no-1);
    printf("**************************************************************************\n");
    cntDuplicate ++;
    ack.ackno = ack_no-1;
    send_ack(&ack, sockfd, servaddr);
}

int creat_socket(){
    // Creating socket file descriptor
	if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}

	memset(&servaddr, 0, sizeof(servaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(stoi(port));
	servaddr.sin_addr.s_addr = INADDR_ANY;

	return sockfd;
}

int main() {
    // read the parameters from the client.in file
    vector <string> info =  read_file("client.in");
    file_name = &info[2][0];
    server = &info[0][0];
    port = &info[1][0];

    sockfd = creat_socket();
    request_file(file_name);
    while(true){

        packet *pck= receive_pck();

        if(check_fin(pck)){
            break;
        }

        ack_packet ack = get_ack();
        if (correct_ack(pck)){

            new_pck_actions(pck, ack);

        }else if(cntDuplicate<3){
            lost_pck_actions(pck, ack);
        }

    }

	close(sockfd);
	return 0;
}
