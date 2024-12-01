#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/time.h>
#include <pthread.h>

//------------------all the server options
#define BUFFER_SIZE 1024
#define MAX_SONGNAME 255
#define STREAM_RATE 62500
#define MAX_INPUT 200

//----------------Timeouts
#define UPLOAD_TIMEOUT 3
#define GENERAL_TIMEOUT 300000

//-----------------all available states
#define DEFAULT 0
#define HELLO 1
#define CLOSE 2
#define WELCOME 3
#define ESTABLISHED 4
#define ANNOUNCE 5
#define SEND_SONG 6
#define RECIEVE_SONG 7

#define WAIT_CLIENT 3

//------------------define all client's messages types
#define HELLO_MSG 0
#define ASKSONG_MSG 1
#define UPSONG_MSG 2

//Station structure contains tcp socket ip address and thread
typedef struct client{
    int connected;
    int tcpSocket;
    int NewStations;
    struct in_addr clientIp;
    pthread_t thr;
}client;

//Station structure contains id, udp socket, ip address, port, name of song and thread.
typedef struct station{
	int id;
	int UDPSocket;
	struct in_addr UDPMulticast_IP;
	uint16_t UDPPort;
	char name[MAX_SONGNAME];
	pthread_t thr;
}station;

//------------------global variables
station * stations;     // array for all stations
client * clients;       // array for all clients
int streamStations = 1; // flag that shows if stream stations are online
int permitUpload = 1;   // flag that check if new song is may to upload ---------mutex is needed
int numOfClients = 0;   // counter for all active clients
int stationSize = 0;    //counter for all multicast groups

pthread_mutex_t stations_update = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t uploadPermission = PTHREAD_MUTEX_INITIALIZER;

int checkClientFree(int numClients);
int isFileExist(char * filename);
int msgTypeAndValid(char *clientMSG,int msgBytes);
void newStation(station addNewStation);
void newClient(client clientToAdd);
void printAllStations(int stationsAmount);
void printAllClients(int clientsAmount);
void handleInvalidCommand(int clientSocket, char* errorMsgToSend);
int checkFileSize(FILE * fp);
void* udpStream(void* input);
void* HandleClient(void* input);


int main(int argc, char*argv[]){
	int i,state=DEFAULT,welcomeSocket,bindStatus,selectAns,clientIndex,enable=1;
	int stationID[MAX_INPUT];
	int clientID[MAX_INPUT];
	char userInput[MAX_INPUT];
	struct sockaddr_in serverAddress;
	struct sockaddr_in clientAddress;
	station stationToAdd;
	client clientToAdd;
	fd_set fds;                 // Socket file descriptors we want to wake
	socklen_t addr_size;

	stations=NULL;

	if(argc<4){
		printf("Invalid arguments\n");
		return 1;
	}
	for(i=0;i<argc-4;i++){                              //create lists
		if(!isFileExist(argv[i+4])){            //check if the file exist
			printf("Song %s not found\n",argv[i+4]);
			if(stations!=NULL)
				free(stations);
			return 1;
		}                                               //insert new stations to the array by the songs in the CLA
		strcpy(stationToAdd.name,argv[i+4]);
		stationToAdd.UDPMulticast_IP.s_addr=htonl(ntohl(inet_addr(argv[2]))+i);
		stationToAdd.UDPPort=htons(atoi(argv[3]));
		stationToAdd.id=i;
		newStation(stationToAdd);
	}

	while(state!=-1){
		switch(state){
			case DEFAULT:
				setsockopt(welcomeSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
				welcomeSocket = socket(AF_INET, SOCK_STREAM, 0);
				serverAddress.sin_family=AF_INET;
				serverAddress.sin_port=htons(atoi(argv[1]));
				serverAddress.sin_addr.s_addr=htonl(INADDR_ANY);
				memset(serverAddress.sin_zero, '\0', sizeof(serverAddress.sin_zero));

				bindStatus=bind(welcomeSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
				if(bindStatus==-1){
					printf("Bind function error\n");
					state=CLOSE;
					break;
				}
				if(listen(welcomeSocket,1)==0)
					printf("Main socket created, waiting for clients\n");
				else{
					printf("Listen error function\n");
					state=CLOSE;
					break;
				}

				for(i=0;i < stationSize; i++){
					stationID[i]=i;
					if(pthread_create(&(stations[i].thr),NULL,udpStream,&stationID[i])!=0){
						printf("Error with thread creating\n");
					}
				}
				state=WAIT_CLIENT;

				break;

			case WAIT_CLIENT:
				FD_CLR(0,&fds);              //standard input
				FD_CLR(welcomeSocket,&fds);

				FD_SET(0, &fds);             // set the stdin
				FD_SET(welcomeSocket, &fds); // set socket

				selectAns=select(welcomeSocket+1,&fds,NULL,NULL,NULL);
				if(selectAns==-1){
					printf("Error with select function\n");
					state=CLOSE;
					break;
				}
				else if(FD_ISSET(welcomeSocket, &fds)){
					//add new client to the server
					//accept new connection
					addr_size = sizeof(clientAddress);

					clientToAdd.tcpSocket = accept(welcomeSocket, (struct sockaddr *) &clientAddress, &addr_size);
					clientToAdd.connected=1;
					clientToAdd.NewStations=0;
					clientToAdd.clientIp=clientAddress.sin_addr;

					clientIndex = checkClientFree(numOfClients);

					if(clientIndex==-1){
						newClient(clientToAdd);
						clientIndex=numOfClients-1;
						clientID[clientIndex]=clientIndex;

					}else{
						clients[clientIndex]=clientToAdd;
					}
					if(pthread_create(&(clients[clientIndex].thr),NULL,HandleClient,&clientID[clientIndex])!=0){
						printf("Error with thread creating for client\n");
						state=CLOSE;
						break;
					}
					else{
						printf("Thread created for client number %d \n",clientIndex);
					}

					break;
				}
				else if(FD_ISSET(0, &fds)){//handle user input                  //check the input of the user (fd 0)
					fgets(userInput, sizeof(userInput), stdin);
					if(userInput[0] == 'p'){                         //print all data
                        printAllStations(stationSize);
						printf("\n");
						printAllClients(numOfClients);
					}
					else if(userInput[0] == 'q'){                   //change to close state
						state=CLOSE;
						break;
					}
					else{
						printf("Incorrect input, type p for print current status or q to quit\n");
						break;
					}
				}

				break;
			case CLOSE:
				                                    //close all stations
				streamStations=0;
				for(i=0;i<stationSize;i++){
					pthread_join(stations[i].thr,NULL);
				}
				free(stations);

                                                    //close all clients
				for(i = 0;i < numOfClients;i++){
					//disconnect client
					clients[i].connected = 0;
					pthread_join(clients[i].thr,NULL);

				}
				free(clients);
                free(stations);
				close (welcomeSocket);

                pthread_mutex_unlock(&stations_update);
                int ret = pthread_mutex_destroy(&stations_update);
                if (ret != 0)
                    printf("Error with mutex destroy");
                pthread_mutex_unlock(&uploadPermission);
                ret = pthread_mutex_destroy(&uploadPermission);
                if (ret != 0)
                    printf("Error with mutex destroy");
				return EXIT_SUCCESS;

				break;
            default:
                printf("Unexpected error");
                state = CLOSE;
                break;
		}

	}
	return EXIT_SUCCESS;
}

int checkClientFree(int numClients){
    /*
     * This function get the total value of all clients and check which index is available client.
     */
    int i;
    for(i=0; i < numClients; i++)
        if(clients[i].connected==0)
            return i;
    return -1;
}

int isFileExist(char * file){
    /*
     * function get file name and checks if exits on the server exe file's folder.
     */
    FILE* fp;
    fp=fopen(file,"r");
    if(fp==NULL)
        return 0;
    else {
        fclose(fp);
        return 1;
    }
}

int msgTypeAndValid(char *clientMSG,int msgBytes){
    /*
     * The function get the msg from the client and its size on bytes and checks which type
     * the message is and if its valid. in case its valid its send the message value.
     * in case its invalid its return -1.
     */
    int size = 0;
    int msgTbyBuffer = *(uint8_t*)&clientMSG[0]; //cast the commandTYpe field to int.

    switch (msgTbyBuffer){
        //Welcome message
        case HELLO_MSG:
            if(msgBytes==3){  //
                if(*(uint16_t*)&clientMSG[1]==0) //cast the reserved field to int.
                    return HELLO_MSG;
            }
            return -1;
            break;
        case UPSONG_MSG:
            size=*(uint8_t*)&clientMSG[5];
            size+=6;
            if(msgBytes==size){
                return UPSONG_MSG;
            }
            return -1;
            break;

        case ASKSONG_MSG:
            if(msgBytes==3){
                return ASKSONG_MSG;
            }
            return -1;
            break;

        default:
            return -1;
    }
}

void newStation(station addNewStation){
    /*
     * The functions get new station struct in order to add it to the existing stations global array.
     * The stations list is global and in use of multithreading thus mutex is implement here.
     * The function use realloc to change the size of the array.
     */
    pthread_mutex_lock(&stations_update);
    if(stations==NULL)
        stations=(station*)malloc(sizeof(station));
    else
        stations = (station*) realloc (stations, (stationSize + 1) * sizeof(station));
    stations[stationSize]=addNewStation;
    strcpy(stations[stationSize++].name,addNewStation.name);
    pthread_mutex_unlock(&stations_update);

}

void newClient(client clientToAdd){
    /*
     * The functions get new client struct in order to add it to the existing clients global array.
     * The function use realloc to change the size of the array.
     */
    if(clients==NULL)
        clients=(client*)malloc(sizeof(client));
    else
        clients = (client*) realloc (clients, (numOfClients+1) * sizeof(client));
    clients[numOfClients++]=clientToAdd;
}

void printAllStations(int stationsAmount){
    /*
     * The functions get the total amount of stations and print status of all of them.
     */
    int i;
    printf("All available stations and their details:\n---------------\n");
    pthread_mutex_lock(&stations_update);
    for(i=0; i < stationsAmount ;i++){
        printf("ID: %d\n",stations[i].id);
        printf("Station %d: Multicast IP: %s\n", stations[i].id, inet_ntoa(stations[i].UDPMulticast_IP));
        printf("Station %d: UDP Port: %d\n", stations[i].id,ntohs(stations[i].UDPPort));
        printf("Station %d: Song name on this station: %s\n",stations[i].id ,stations[i].name);
        printf("---------------\n");
    }
    pthread_mutex_unlock(&stations_update);

}

void printAllClients(int clientsAmount){
    /*
     * The functions get the total amount of clients and print status of all of them.
     */
    int i;
    printf("Connected clientes:\n---------------\n");
    for(i=0; i < clientsAmount; i++){
        if(clients[i].connected)
            printf("Client number [%d]'s IP: %s\n",i,inet_ntoa(clients[i].clientIp));
    }
}

void handleInvalidCommand(int clientSocket, char* errorMsgToSend){
    /*
     * The functions get open client socket and error message to send.
     * The functions send the error message to the client
     */
    int lenOfMSG;
    char sendBuffer[BUFFER_SIZE];

    lenOfMSG = strlen(errorMsgToSend);
    sendBuffer[1]=htons(lenOfMSG);
    sendBuffer[0] = 3;
    strcpy(&sendBuffer[2],errorMsgToSend);
    send(clientSocket,sendBuffer,2+lenOfMSG,0);
}

void* udpStream(void* input){
    /*
     * THis function is handled by threads. The function handle the udp Stream of all the clients
     * each station is a thread.
    */

    struct sockaddr_in Addr;
    char sendBuffer[BUFFER_SIZE];
    int ttl = 10, byteSent = 0;
    int  sizeOfFIle ,bytesRead ,sendStatus ,stationIndex;
    FILE * fp;

    stationIndex = *(int*)input;
    printf("The station is up - number %d\n",stationIndex);
    fp=fopen(stations[stationIndex].name,"r");       //Try to open the file by name
    if(fp==NULL){
        printf("file is unreachable \n");
        pthread_exit(NULL);
    }
    sizeOfFIle = checkFileSize(fp);
    stations[stationIndex].UDPSocket = socket(AF_INET, SOCK_DGRAM, 0);
    Addr.sin_family=AF_INET;                                            //IPV4
    Addr.sin_port=stations[stationIndex].UDPPort;                       //configure the port to the struct
    Addr.sin_addr.s_addr=stations[stationIndex].UDPMulticast_IP.s_addr; //configure the address to the struct
    memset(Addr.sin_zero, '\0', sizeof(Addr.sin_zero));           //set 0 to the rest of the struct
    setsockopt(stations[stationIndex].UDPSocket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)); //set UDP socket options
    while(streamStations){      //as long the stream flag is on - global flag
        bytesRead = BUFFER_SIZE;
        while(byteSent < sizeOfFIle && bytesRead == BUFFER_SIZE && streamStations){   //as long
            bytesRead = fread(sendBuffer,1,BUFFER_SIZE,fp);
            sendStatus = sendto(stations[stationIndex].UDPSocket,sendBuffer,bytesRead,0,(struct sockaddr *) &Addr,sizeof(Addr));

            if(sendStatus < 0 ){               //send failed for some reason
                printf("error in sending...\n");
                pthread_exit(NULL);
                break;
            }
            else{                               //send succeed
                byteSent += sendStatus;
            }
            usleep(STREAM_RATE);       //stream rate sleep between resend.
        }
        byteSent = 0;                          //when finish to stream
        rewind(fp);                     //go back to the beginning of the file for resend
    }
    if(fp != NULL)                            //In case of stop streaming
        fclose(fp);
    close(stations[stationIndex].UDPSocket);
    pthread_exit(NULL);
}

void* HandleClient(void* input){
    int state = HELLO;
    int selectVal, recieveVal, messageType, wantedStation, stationIndex, idOfClient, i;
    int upSongSize, upSongNameSize;
    int recBytes = 0, sendPermit = 1;
    char gotbuffer[BUFFER_SIZE];
    char sendbuffer[BUFFER_SIZE];
    char songName [BUFFER_SIZE];
    station stationforUpSong;
    fd_set fds;
    FILE* fp;
    struct timeval timeout;

    idOfClient=*(int*)input;

    while(clients[idOfClient].connected){
        switch(state){
            case HELLO:
                //FD_ZERO(&fds);
                FD_CLR(clients[idOfClient].tcpSocket, &fds); // make sure the socket is not in the fd
                FD_SET(clients[idOfClient].tcpSocket, &fds); // set socket to fd

                timeout.tv_sec = 0;
                timeout.tv_usec = GENERAL_TIMEOUT; //300 ms
                selectVal = select(clients[idOfClient].tcpSocket+1,&fds,NULL,NULL,&timeout);
                if(selectVal == 0){
                    printf("Hello message Timeout - 300 ms.\n");
                    state=CLOSE;
                    break;
                }
                else if(selectVal==-1){
                    printf("Error with select function\n");
                    state=CLOSE;
                    break;
                }
                else if(FD_ISSET(clients[idOfClient].tcpSocket, &fds)){
                    recieveVal=recv(clients[idOfClient].tcpSocket, gotbuffer, 3, 0);
                    if(msgTypeAndValid(gotbuffer,recieveVal)==HELLO_MSG){
                        printf("Got HELLO from: %s\n",inet_ntoa(clients[idOfClient].clientIp));
                        state=WELCOME;
                        break;
                    }
                    else{
                        printf("Expected to get HELLO from %s - closing the connection\n",inet_ntoa(clients[idOfClient].clientIp));
                        state=CLOSE;
                        break;
                    }
                }

                break;

            case WELCOME:
                sendbuffer[0]=0;
                *(uint16_t*)&sendbuffer[1]=htons(stationSize);
                *(uint32_t*)&sendbuffer[3]=htonl(stations[0].UDPMulticast_IP.s_addr);
                *(uint16_t*)&sendbuffer[7]=stations[0].UDPPort;
                send(clients[idOfClient].tcpSocket,sendbuffer,9,0);
                state=ESTABLISHED;
                break;

            case ANNOUNCE:
                wantedStation=ntohs(*(uint16_t*)&gotbuffer[1]);
                //check if the wanted station exist
                if(wantedStation + 1 > stationSize){
                    handleInvalidCommand(clients[idOfClient].tcpSocket,"Wrong station number, try again");
                    state = CLOSE;
                    break;
                }

                sendbuffer[0] = 1;
                sendbuffer[1] = strlen(stations[wantedStation].name);
                strcpy(&sendbuffer[2],stations[wantedStation].name);
                printf("client %s asked for %s - this song is play in station %d\n",inet_ntoa(clients[idOfClient].clientIp),stations[wantedStation].name,stations[wantedStation].id);
                send(clients[idOfClient].tcpSocket,sendbuffer,2 + strlen(stations[wantedStation].name),0);
                state = ESTABLISHED;
                break;

            case SEND_SONG:
                upSongSize = ntohl(*(uint32_t*)&gotbuffer[1]);
                upSongNameSize = gotbuffer[5];

                for(i = 0; i < upSongNameSize; i++){
                    songName[i] = gotbuffer[i+6];
                }
                pthread_mutex_lock(&uploadPermission);
                if (permitUpload){// if all other clients are not uploading
                    sendPermit=1;
                    permitUpload=0;
                    if(upSongSize>10000000 || upSongSize<2000){
                        printf("File is too large from client %s\n",inet_ntoa(clients[idOfClient].clientIp));
                        sendPermit=0;
                        permitUpload=1;
                    }

                    //check if the song already exist
                    for(i=0;i<stationSize;i++){
                        if(strcmp(stations[i].name,songName)==0){
                            printf("Song already stream in %d, permission denied for client %s\n",i ,inet_ntoa(clients[idOfClient].clientIp));
                            sendPermit = 0;
                            permitUpload = 1;
                            break;
                        }
                    }
                }
                else{
                    sendPermit=0;
                }
                pthread_mutex_unlock(&uploadPermission);

                sendbuffer[0] = 2;
                sendbuffer[1] = sendPermit;
                send(clients[idOfClient].tcpSocket,sendbuffer,2,0);

                if(sendPermit)
                    state=RECIEVE_SONG;
                else{
                    state=ESTABLISHED;
                }
                break;

            case RECIEVE_SONG:
                printf("Getting song %s from client %s\n",songName,inet_ntoa(clients[idOfClient].clientIp));
                fp=fopen(songName,"w");
                if(fp==NULL){
                    printf("File error\n");
                    state=CLOSE;
                    break;

                }
                recBytes=0;

                while(state==RECIEVE_SONG && recBytes < upSongSize){
                    //set timeout to 3 sec
                    timeout.tv_sec = UPLOAD_TIMEOUT;
                    timeout.tv_usec = 0;

                    FD_CLR(clients[idOfClient].tcpSocket,&fds);  //reset the fd before the select.
                    FD_SET(clients[idOfClient].tcpSocket, &fds);

                    selectVal = select(clients[idOfClient].tcpSocket + 1,&fds,NULL,NULL,&timeout);
                    if(selectVal==0){
                        printf("3 seconds timeout while receiving song ,The client  %s is disconnecting... \n",inet_ntoa(clients[idOfClient].clientIp));
                        state=CLOSE;
                        break;
                    }

                    else if(selectVal == -1){
                        printf("error in select function\n");
                        state=CLOSE;
                        break;
                    }

                    else if(FD_ISSET(clients[idOfClient].tcpSocket, &fds)){
                        recieveVal = recv(clients[idOfClient].tcpSocket, gotbuffer, BUFFER_SIZE, 0);
                        if(recieveVal<=0){
                            state=CLOSE;
                            break;
                        }
                        recBytes += recieveVal;
                        fwrite(gotbuffer,sizeof(char),recieveVal,fp);
                    }
                }

                fclose(fp);
                printf("Client : %s finished upload song: %s \n",inet_ntoa(clients[idOfClient].clientIp),songName);
                permitUpload = 1;
                if(state==CLOSE){
                    break;
                }

                strcpy(stationforUpSong.name,songName);
                stationforUpSong.UDPMulticast_IP.s_addr=htonl(ntohl(stations[0].UDPMulticast_IP.s_addr) + stationSize);
                stationforUpSong.UDPPort=stations[0].UDPPort;
                stationforUpSong.id=stationSize;
                newStation(stationforUpSong);
                stationIndex = stationSize - 1;

                if(pthread_create(&(stations[stationSize-1].thr),NULL,udpStream,&stationIndex)!=0){
                    printf("Failed creating new stations - failed with opening new thread");
                }


                for(i=0; i < numOfClients ;i++){    //send for all client about the new station
                    if(clients[i].connected){
                        clients[i].NewStations = 1;
                    }
                }
                state = ESTABLISHED;
                break;

            case ESTABLISHED:
                //set timeout
                timeout.tv_sec=0;
                timeout.tv_usec=300000; //100 ms

                FD_CLR(clients[idOfClient].tcpSocket,&fds);
                FD_SET(clients[idOfClient].tcpSocket, &fds); // set socket to fd
                FD_SET(clients[idOfClient].NewStations,&fds);
                selectVal=select(clients[idOfClient].tcpSocket+1,&fds,NULL,NULL,&timeout);

                if(clients[idOfClient].connected==0){
                    state=CLOSE;
                    break;
                }

                if(selectVal==-1){
                    printf("Error with select function");
                    state=CLOSE;
                    break;
                }
                else if(FD_ISSET(clients[idOfClient].tcpSocket, &fds)){
                    recieveVal=recv(clients[idOfClient].tcpSocket, gotbuffer, BUFFER_SIZE, 0);

                    messageType = msgTypeAndValid(gotbuffer,recieveVal);

                    switch(messageType){    //check message type from client
                        case ASKSONG_MSG:   //case of station choosing
                            state=ANNOUNCE;
                            break;
                        case UPSONG_MSG:    //case of new song
                            state=SEND_SONG;
                            break;
                        case HELLO_MSG:     //dub hello msg
                            printf("Got HELLO twice from %s\n",inet_ntoa(clients[idOfClient].clientIp));
                            handleInvalidCommand(clients[idOfClient].tcpSocket,"duplicate HELLO message");
                            state=CLOSE;
                            break;
                        default:            //any other case
                            printf("Got non ASKSONG or UPSONG from client %s\n",inet_ntoa(clients[idOfClient].clientIp));
                            handleInvalidCommand(clients[idOfClient].tcpSocket,"wrong message received, ASKSONG or UPSONG are expected");
                            state=CLOSE;
                            break;
                    }
                }
                else if(selectVal==0){                  //in case that select "jumped" because of enw station
                    if(clients[idOfClient].NewStations){
                        //send new stations;
                        sendbuffer[0]=4;
                        *(uint16_t*)&sendbuffer[1]=htons(stationSize);
                        send(clients[idOfClient].tcpSocket,sendbuffer,3,0);
                        clients[idOfClient].NewStations=0;
                    }
                }
                break;

            case CLOSE:
                printf("Client %s disconnected\n",inet_ntoa(clients[idOfClient].clientIp));
                close(clients[idOfClient].tcpSocket);
                clients[idOfClient].connected=0;
                break;

            default:
                printf("Unexpected error");
                state = CLOSE;
                break;
        }
    }

    close(clients[idOfClient].tcpSocket);
    pthread_exit(NULL);
}

int checkFileSize(FILE * fp)
{
    int sizeOfFIle;
    //check the file size by moving fp to the end of the file and then rewind it to begining
    fseek(fp,0L,SEEK_END);      //go to the end of file
    sizeOfFIle = ftell(fp);               //keep the size of the file in integer
    rewind(fp);                           //go back to the beginning og the file
    return sizeOfFIle;
}