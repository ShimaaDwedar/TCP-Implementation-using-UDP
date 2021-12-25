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
#define max_seq UINT32_MAX
using namespace std;

int send_base =0;
int window_sz = 1;
int ssthresh = 4096;
int dup_ack = 0 ;
int rnd;
int port;
float prob;
int slow_start =  1;
int congestion_avoidance = 0;
int fast_recovery = 0;
int sockfd;
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

        for (int i = ind; i < packet_len+ind ; i++){
            pck.data[i-ind] = content[i];
        }
        pck.data[packet_len] = '\0';
        ind += packet_len;
        left -= packet_len;
        seq++;
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
void set_seconds(int sec){
    //set timeout on receining acks
    struct timeval tv;
    tv.tv_sec = sec;
    tv.tv_usec = 100000;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
        perror("Error");
    }
}
pair<ack_packet, int> receive_ack(int sockfd, sockaddr_in cliaddr){
    char buf[sizeof(ack_packet)+1];
    socklen_t len = sizeof(cliaddr);

    set_seconds(0);
    int no_bytes = recvfrom(sockfd, (char *)buf, 9,
                    0, (struct sockaddr *) &cliaddr,
                    &len);

    ack_packet *ack= (ack_packet*) buf;
    return make_pair(*ack, no_bytes);

}


int FSM (pair <ack_packet, int> p){
    //handling timeout for slow start, congestion avoidance and fast_recovery
    if (p.second == -1){
        cout << "Timeout!"<<endl;
        ssthresh = window_sz/2;
        window_sz = 1;
        dup_ack = 0;

        slow_start = 1;
        fast_recovery = 0;
        congestion_avoidance = 0;
        return send_base;
    }else if (p.first.ackno != send_base+1){
        dup_ack ++;
        //apply for slow start and congestion avoidance
        if (dup_ack == 3){
            cout << "3 Duplicate Acks!!"<<endl;
            cout <<"-------------------"<<endl;
            // fast_recovery_mode
            fast_recovery = 1;
            slow_start = 0;
            congestion_avoidance = 0;

            ssthresh = max(1, window_sz/2);
            window_sz =  ssthresh + 3 ;

            dup_ack = 0;
            return send_base ;
        }
    }else{
        dup_ack = 0;
    //  cout <<" slow_start "<< slow_start <<"  congestion_avoidance  "<< congestion_avoidance  <<" fast_recovery  "<<fast_recovery <<endl;
        if (slow_start){
            window_sz *= 2;
            if (window_sz >= ssthresh){
                window_sz = max(1,ssthresh);

                congestion_avoidance = 1;
                slow_start = 0;
                fast_recovery = 0;
            }
        }else if (congestion_avoidance){
            window_sz++;
        }else if (fast_recovery){
            congestion_avoidance = 1;
            fast_recovery = 0;
            slow_start =0;

            window_sz  = max(1,ssthresh);
        }
        send_base++;
    }
        return -1;
}


bool greater_than_prop(float prob ){
    //makes prob_fraction between 0 & 1
    float prob_fraction = (float)rand() / RAND_MAX;
    return prob_fraction > prob;
}
int creat_socket(){
    // Creating socket file descriptor


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

ack_packet get_ack(){
    ack_packet ack;
    ack.cksum = 0;
    ack.len = 8;
    return ack;
}
int send_ack(ack_packet* ack){
    char* buf;
    buf = (char*)ack;
    socklen_t len = sizeof(servaddr);
    ack_packet *pck= (ack_packet*) buf;
    int n = sendto(sockfd, (const char *)buf , 9,
		MSG_CONFIRM, (const struct sockaddr *) &servaddr,
			len);
    cout << n <<endl;
    return n;
}

void get_requested_file(){
    socklen_t len = sizeof(cliaddr);
	int n = recvfrom(sockfd, (char *)buffer, MAXLEN,
				0, ( struct sockaddr *) &cliaddr,
				&len);
	buffer[n] = '\0';

	ack_packet ack = get_ack();
	ack.ackno = 0;
    send_ack(&ack);
}

packet make_fin_pck (){
    packet fin_pck;
    fin_pck.cksum = 0;
    fin_pck.len =  508;
    fin_pck.seqno = max_seq ;
    return fin_pck;
}
int main() {

    vector <string> info =  read_file("server.in");
    rnd = stoi(info[1]);
    port = stoi(info[0]);
    prob = stof(info[2]);
    srand(rnd);

    sockfd = creat_socket();
    time_t start, stop;
	time(&start);
    set_seconds(10);

    //receiving the request of the file from the client
    get_requested_file();


	printf("Client : %s\n", buffer);
	string requested_file = buffer;


    //split the file into chunks of data of fixed length
    vector <packet> all_packets = get_packets(requested_file);
    cout << "No. packets to be sent "<<all_packets.size()<<endl;

   // pid_t pid = fork();
        int next_to_sent;
        //start sendnig packets depending on the window size
        for (int ind = send_base ; ind < send_base + window_sz ; ind++){      //if we start window size by 1, we don't need to loop over frist packets to be sent
            int er = send_packet(all_packets[ind], sockfd , cliaddr);
        }
        next_to_sent = send_base + window_sz;

        vector <int> w_arr;
        //start receiving ack packets from the client
        int temp_i = 0;
        int iteration = 0;
        while (1){
            //cout <<"Iteration : " << iteration << endl;
           // cout << "Window Size: " << window_sz <<endl;
            w_arr.push_back(window_sz);
            //cout << "-----------------------------------" <<endl;
            iteration++;
            pair<ack_packet, int> p = receive_ack(sockfd , cliaddr);
            if (p.second != -1){
                printf("Ack no. %d received\n", p.first.ackno);
            }
            if (p.first.ackno==all_packets.size()){
                packet fin_pck = make_fin_pck();
                send_packet(fin_pck, sockfd , cliaddr);
                cout << "All Acks Received!" <<endl;
                time(&stop);
                int duration = stop - start;
                cout << "Taken Time = " <<duration<< endl;
                /*for (int i =0; i < w_arr.size(); i++){
                    printf("%d, ", w_arr[i]);

                }*/
                close(sockfd);
                return 0;
            }

            int new_base = FSM(p);
            if (new_base > -1){    //happening of timeout or 3 duplicate acks
                next_to_sent = new_base ;
            }
            for (int i = next_to_sent; i < send_base + window_sz ; i++){
                if (i >= all_packets.size()){
                    break;
                }
                if (greater_than_prop(prob)){
                    send_packet(all_packets[i], sockfd , cliaddr);
                }else{
            //        printf("Packet %d will not be sent..!\n", all_packets[i].seqno);
                }
                temp_i = i;
            }
            next_to_sent = temp_i+1;
            iteration++;
        }
 //   close(sockfd);
	return 0;
}
