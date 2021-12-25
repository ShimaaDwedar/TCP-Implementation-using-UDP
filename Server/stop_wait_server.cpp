// Server side implementation of UDP client-server model
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <vector>
#include <utility>
#include <fstream>
#include <ctime>
#include <cstring>
#include <chrono>
#define MAXLEN 499
using namespace std;

int send_base =0;
int window_sz = 1;
int ssthresh = 4096;
int dup_ack = 0 ;
int rnd;
int port;
float prob;
char buffer[MAXLEN];
struct sockaddr_in servaddr, cliaddr;

/* Data-only packets */
struct packet {
    /* Header */
    uint16_t cksum; /* Optional bonus part */
    uint16_t len;
    uint32_t seqno;
    /* Data */
    char data [500]= {0}; /* Not always 500 bytes, can be less */
};

/* Ack-only packets are only 8 bytes */
struct ack_packet {
    uint16_t cksum; /* Optional bonus part */
    uint16_t len;
    uint32_t ackno;
};


string get_content (string file_path){
    fstream f(file_path, fstream::in );
    string content;
    getline(f, content, '\0');
    return content;
}
vector<string> read_file(string file_name){
    vector<string> vec;
    fstream f(file_name, fstream::in );
    string line;
    while (getline(f, line)){
        vec.push_back(line);
    }
    return vec;
}

vector <packet> get_packets(string file){
    string content = get_content (file);
    //csum, len, seq, data
    vector <packet> vec;
    int length = content.length();
    int left = length;
    int packet_len;
    int ind = 0;
    int seq = 0;
    while(left>0){

        packet_len = min(MAXLEN, left);
        char data [packet_len+1];
        packet pck;
        pck.cksum = 0;
        pck.len = packet_len + 8;
        pck.seqno = seq;
       // cout << pck.seqno <<endl;
        for (int i = ind; i < packet_len+ind ; i++){
            pck.data[i-ind] = content[i];
        }
        pck.data[packet_len] = '\0';
        ind += packet_len;
        left -= packet_len;
        seq = !seq;
        vec.push_back(pck);
    }

    return vec;
 }

 int send_packet(packet pck, int sockfd, sockaddr_in cliaddr){
    char* buf;

    buf = (char*)&pck;
    socklen_t len = sizeof(cliaddr);

    int no_bytes = sendto(sockfd, (const char *)buf, pck.len,
            0, (const struct sockaddr *) &cliaddr,
                len);
    return no_bytes;
}


int FSM (pair <ack_packet, int> p){
    int resend = 0;
    if (p.second == -1){
        cout << "Timeout!"<<endl;
        resend = 1;
    }else if (p.first.ackno == 3){
        cout <<"***Duplicate ack, resend it!***"<<endl;
        resend = 1;
    }
    return resend;
}


pair<ack_packet, int> receive_ack(int sockfd, sockaddr_in cliaddr){
    char buf[sizeof(ack_packet)+1];
    socklen_t len = sizeof(cliaddr);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
        perror("Error");
    }
    int no_bytes = recvfrom(sockfd, (char *)buf, 9,
                    0, (struct sockaddr *) &cliaddr,
                    &len);

    ack_packet *ack= (ack_packet*) buf;
    return make_pair(*ack, no_bytes);

}

bool greater_than_prop(float prob ){

    float r= (float)rand() / RAND_MAX;
    return r > prob;
}
int creat_socket(){
    // Creating socket file descriptor
    int sockfd;

	if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}


	memset(&servaddr, 0, sizeof(servaddr));
	memset(&cliaddr, 0, sizeof(cliaddr));

	// Filling server information
	servaddr.sin_family = AF_INET; // IPv4
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(port);

	// Bind the socket with the server address
	if ( bind(sockfd, (const struct sockaddr *)&servaddr,
			sizeof(servaddr)) < 0 )
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	return sockfd;
}


int main() {

    vector <string> info =  read_file("server.in");
    rnd = stoi(info[1]);
    port = stoi(info[0]);
    prob = stof(info[2]);

    int sockfd = creat_socket();
    srand(rnd);


	socklen_t len = sizeof(cliaddr);
	time_t start, stop;
	int n = recvfrom(sockfd, (char *)buffer, MAXLEN,
				0, ( struct sockaddr *) &cliaddr,
				&len);
	buffer[n] = '\0';

	printf("Client : %s\n", buffer);

	string requested_file = buffer;


    //split the file into chunks of data of fixed length
    vector <packet> all_packets = get_packets(requested_file);
    cout << "No. packets to be sent "<<all_packets.size()<<endl;

   // pid_t pid = fork();
    packet fin_pck;
    fin_pck.cksum = 0;
    fin_pck.len =  508;
    fin_pck.seqno = 2;

    time(&start);
    auto started = std::chrono::high_resolution_clock::now();

    int i = 0;
    while (1){
        cout << i <<endl;
        if (i == all_packets.size()){
            cout << fin_pck.seqno<<endl;
            send_packet(fin_pck, sockfd , cliaddr);
            break;
        }
        if (greater_than_prop(prob)){
            send_packet(all_packets[i], sockfd , cliaddr);
        }else{
            printf("Packet %d will not be sent..!\n", i);
        }

        pair<ack_packet, int> p = receive_ack(sockfd , cliaddr);
        if (p.second != -1)
            printf("Ack no. %d received\n", (int)p.first.ackno);

        if (i==all_packets.size()-1){
            cout << "All Acks Received!" <<endl;
            auto done = std::chrono::high_resolution_clock::now();
            std::cout <<" Duration = "<< std::chrono::duration_cast<std::chrono::milliseconds>(done-started).count()<<endl;;
            time(&stop);
            int duration = stop - start;
            cout << "Taken Time = " <<duration<< endl;
        }

        int resend = FSM(p);
        if (resend){    //happening of timeout a duplicate ack
            i--;
        }
        resend = 0;
        i++;
    }

    close(sockfd);
	return 0;
}
