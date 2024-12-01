/*
 ------------------------------------------------------------------------------------------------------------------------------------------------
 Name        : radio_controller.c
 Author      :  Shahaf zohar 205978000
                         Haim Cohen   
------------------------------------------------------------------------------------------------------------------------------------------------
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <inttypes.h>
#include <sys/time.h>
#include <stdbool.h>

//define all the states
#define DEFAULT_STATE 0
#define WAIT_WELCOME 1
#define ESTABLISHED 2
#define WAIT_ANNOUNCE 3
#define UPLOAD_SONG 4
#define WAIT_PERMIT 5
#define SEND_SONG 6
#define CLOSE_CONNECTION 7
//define all message types
#define Welcom                    0
#define Announce                 1
#define Permit_Song           2
#define Invalid_Command   3
#define New_Station           4
/*
    getting arguments from Input:
            argv[1] - IP Address of the server
            argv[2] - Port number of the server
    */

#define PERMIT_SONG       2
#define Buffer                     1024
#define SongName             200    
#define MaxInputUser       100   
#define Upload_Rate         10000
#define  Turn_on                   1
#define  Turn_off                 0
#define   Ask_Song              1
#define UPLOAD_RATE      8000
//define all message types

#define menu      "For pressing a number you can get the desired radio station and it will be played.\nTo print the existing radio stations pressing (p).\nFor upload a song pressing (s).\nLeave program pressing (q).\n\n"

typedef struct station{
        int id;
        int play_song;
        struct in_addr UDP_MulticastIP;
        uint16_t UDP_Port;
}station;

//=======================Global variables======================================
int Number_Stations;               //  Radio station play know
int UDP_socket;                         // UDP client  socket
char member[6][6] = {"help", "-help" ,"?"};
struct timeval Time_Out;
//=======================Prototype Function===================================
int checkWelcome(int length);
int checkAnnounce(char *message, int length);
int checkNewstation(int length);
void Process( char label[], int Byte_sent, int size_file );
void* UDPaudio(void * input);                                      // Function for listen audio
bool is_member(char *input);
void RestTime(bool flag);
int checkMessage(char *buff,int byteRec);
//=======================================================================

void RestTime(bool flag){
            ///set timeout
            if(flag){
                    Time_Out.tv_sec = 0;                                         //  Seting Time Out filde seconds to zero
                    Time_Out.tv_usec = 300000;                            //  Seting Time Out filde microseconds to 0.3 ms 
            }
            else{
                    Time_Out.tv_sec = 3;                                        //  Seting Time Out filde seconds to zero
                    Time_Out.tv_usec = 0;                                     //  Seting Time Out filde microseconds to 0.3 ms 
            }
}

void Process( char label[], int Byte_sent, int size_file ){
            const int pwidth = 72;    //progress width
            //minus label len
            int i;
            int width = pwidth - strlen(label);             
            int pos = ( Byte_sent * width ) / size_file ;
            int proportion = ( Byte_sent * 100 ) / size_file;
            printf( "%s[", label );

            //fill progress bar with =
            for ( i = 0; i < pos; i++ )
                printf( "%c", '#' );
            //fill progress bar with spaces
                printf( "%*c", width - pos + 1, ']' );
                printf( " %4d%%\r", proportion );
                fflush(stdout);                                    //function in C programming that is used to clear the output buffer of the stdout stream
}

bool is_member(char *input){
        int i;
        for(i=0; i<4; i++){
            if(strcmp(input, member[i])==0){
                    return true;
            }
        }
        return false;   
}

int checkWelcome(int length){
    /*
        Description : checking message. return the if the message is  according to the protocol or not.
    */
            if(length==9){
                    return 0;           
            }
            else
                    return -1;
}

int checkAnnounce(char *message, int length){
    /*
        Description : checking message. return the if the message is  according to the protocol or not.
    */
            int size=*(uint8_t*)&message[1]+2;
            if(length==size){
                        return 1;
                }
                else
                        return -1;
}        

int checkNewstation(int length){
                if(length==3){
                    return 4;
                }
                else
                    return -1;
    }

void* UDPaudio(void * input){
        /*function Description: listening to UDP tream, changing UDP multicast IP according to the current station. 
            function run on seprate thread */
            int Udp_len;
            station* RadioStation = (station*)input;
            station ptrStation  = *(station*)input;
            struct sockaddr_in UdpAddr;
            struct ip_mreq mreq;
            struct in_addr UDPMulticastIP;
            FILE* FP;
            fd_set fdset;
            int SockStatus;
            int Variable_Select;
            int receive;
            int selectcheck;
            bool flag =true;
            bool Errorflag=false;
            char UDPbuffer[Buffer] = {[0 ... Buffer-1] = 0};
            struct timeval Time_Out_UDP;
            //open a pipe. output is sent to dev/null (hell).The popen() function opens a process by creating a pipe, forking,
        /*        and invoking the shell.  Since a pipe is by definition unidirectional, the type argument may specify only reading or
                    writing, not both; the resulting stream is correspondingly read- only or write-only. */
                    
            FP = popen("play -t mp3 -> /dev/null 2>&1", "w");                  
            if(FP==NULL){
                        printf("Can't open file Audio to listen mp3\n");
            }
 
            Udp_len=sizeof(UdpAddr);                                                
            if((UDP_socket=socket(AF_INET, SOCK_DGRAM,0))<0){                   //create UDP socket and check 
                    printf("Error socket creation\n");
                    pthread_exit(NULL);                                                                         //kill the thread 
            }
//====================Configurtion address======================
            UdpAddr.sin_family=AF_INET;                                                              // Set to use internet ipv4
            UdpAddr.sin_addr.s_addr=htonl(INADDR_ANY);                             //Description below the code, set the IP address 
            UdpAddr.sin_port=htons(RadioStation->UDP_Port);                      //Set the destination port
            memset(UdpAddr.sin_zero, '\0', sizeof(UdpAddr.sin_zero));         // Rest of the bits to zero

            //bind  address from udp socket
            if(bind(UDP_socket, (struct sockaddr *) &UdpAddr, sizeof(UdpAddr))==-1){
                        printf("Error whit binding address from the UDP socket\n");
                        close(UDP_socket);                                             // close the socket 
                        pthread_exit(NULL);                                           // kill the thread
                }      
                            /* Preparatios for using Multicast */ 
            mreq.imr_interface.s_addr=htonl(INADDR_ANY);                                        //set interface to ANY IP Description down bello the code
            mreq.imr_multiaddr.s_addr=RadioStation->UDP_MulticastIP.s_addr;  //set the Multicast address of the station by increment IP by the station ID

            ptrStation=*RadioStation;                                                                              
            SockStatus=setsockopt(UDP_socket , IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));   //  calling setsockopt with IP_ADD_MEMBERSHIP to set up a multicast group
            if(SockStatus == -1){
                printf("Error on set socket\n");
                    close(UDP_socket);                                                 // close the socket 
                    pthread_exit(NULL);                                               // kill the thread
            }
            
            while(RadioStation->play_song){                                                                                     // check if the filed play song is true =1 infinity loop 
                if(ptrStation.id != RadioStation->id){                                                                          //check if the station change
                        mreq.imr_multiaddr.s_addr=ptrStation.UDP_MulticastIP.s_addr;                  //drop membership (last station multicast)
                        SockStatus=setsockopt(UDP_socket,IPPROTO_IP,IP_DROP_MEMBERSHIP,&mreq,sizeof(mreq));     // Cancellation of group Multocast Registration
                        mreq.imr_multiaddr.s_addr=RadioStation->UDP_MulticastIP.s_addr;          //new Multicast address           
                if((SockStatus=setsockopt(UDP_socket,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq)))==-1){     // request to join the new multicast group 
                        printf("Error whit socket\n");
                        pthread_exit(NULL);
                }
                ptrStation=*RadioStation;
                }
            /*  when we want to UP load song filed play song turn_off and then the Client want to upload new song,
                 The server needs to respond with a NewStations message within 3 seconds.*/
            //set up TIme Out
            Time_Out_UDP.tv_sec = 2;
            Time_Out_UDP.tv_usec = 0;
            // Clears the bit for the file descriptor FD in the file descriptor set fdset
            FD_CLR(UDP_socket,&fdset);                               //reset FD flag on the UDP socket
             // initialization: Sets the bit for the file descriptor FD in the file descriptor set fdset
            FD_SET(UDP_socket, &fdset);                              // set FD flag on the UDP soket

            Variable_Select = select(UDP_socket+1,&fdset,NULL,NULL,&Time_Out);       
            if(FD_ISSET(UDP_socket, &fdset)){        //Returns a non-zero value if the bit for the socket  is set in the pointed to by fdset, and 0 otherwise
                // if true write a buffer of size numbyts into FP
                    (recvfrom(UDP_socket, UDPbuffer, Buffer, 0 ,(struct sockaddr *) &UdpAddr,&Udp_len)>0) ? fwrite(UDPbuffer , sizeof(char), Buffer, FP) :  !(Errorflag); 
                    if(Errorflag){ 
                        printf("Error on receiving UDP stream\n");
                    break;}
                    else{
                        continue;}
            }
        }
            /*  where we have Error on receiving UDP stream orwe want to upload new song to new station,
                  drop membership from last station multicast*/
        mreq.imr_multiaddr.s_addr=ptrStation.UDP_MulticastIP.s_addr;                                                  //drop membership (last station multicast)
        SockStatus=setsockopt(UDP_socket,IPPROTO_IP,IP_DROP_MEMBERSHIP,&mreq,sizeof(mreq));        // Cancellation of group Multocast Registration
        pclose(FP);                                   // close the file descriptor
        pthread_exit(NULL);                  //kill the threads
}

int checkMessage(char *buff,int byteRec){

        int i,size=0;
        int msgtype=*(uint8_t*)&buff[0];
        switch (msgtype){
        //Welcome message
        case Welcom:
            if(byteRec==9){
                return Welcom;
            }
            else
                return -1;
                break;
        case Announce:
                size=*(uint8_t*)&buff[1];
                size+=2;
                if(byteRec==size){
                    return Announce;
                }
            else
                return -1;
                break;
        case Permit_Song:
                if(byteRec==2){
                    return Permit_Song;
                }
                else
                        return -1;
                    break;
        case Invalid_Command:
                    size=*(uint8_t*)&buff[1];
                    size+=2;
                    if(byteRec==size){
                    //print the invalid command message
                    printf("Got invalid command!\n");
                    size=*(uint8_t*)&buff[1];
                    //printf("%d\n\n",nameLenght);
                    for(i=2;i<size+2;i++)
                        printf("%c",buff[i]);
                    printf("\n");

                    return Invalid_Command;
            }
            else
                    return -1;
                break;
        case New_Station:
                if(byteRec==3){
                    return New_Station;
            }
            else
                    return -1;
            break;

        default:
            return -1;
        }
}
//=======================================================================

int main(int argc, char*argv[]){
            int Tcp_Socket;
            int msgtype;
            int SelectNumber=0;
            int fileNameSize;
            int songlen;
            int Receive;
            int messagetype;
            int state=0;
            int BytesRead;
            int receivMsg;
            int Bytesent=0;
            int SendFileStatus;
            int c;
            int selectcheck;
            bool flag =true;
            char *s;
            int bytes_sent ;
            char input[MaxInputUser];
            char SendMessage[Buffer] = {[0 ... Buffer-1] = 0};
            char ReceiverBuffer[Buffer] = {[0 ... Buffer-1] = 0};
            struct sockaddr_in Server_Addr;
            struct hostent * ipAddr;
            struct in_addr Multicast_IP;
            fd_set fds;                                                             // Socket file descriptors we want to wake
            station Radio_station;
            FILE* fp=NULL;
            pthread_t thread;
            RestTime(flag);                                                 //set timeout
            int sizename;
            uint16_t portn;
            char* hostname;
            //initiate stations number to 0
            //if the user didn't enter any arguments or too many
            if(argc!=3){
                    printf("radio_control: Invalid option\nUseage: ./radio_controller [Server ip Address or Hostname] [TCP Port]\n");
                    return EXIT_FAILURE;
            }

            while(1){
                switch(state){
                    case 0:                                          //Default state 
//------------------------------------------Configurtion server address-------------------------------------------------------------------------------
                        /* Fill in server address and port */
                       // bzero(&Server_Addr, sizeof(Server_Addr));                           //set the rest of the bits to zero
                        sizename = strlen(argv[1]);
                        hostname = (char *)calloc(sizename+1, sizeof(char));
                        memcpy(hostname, argv[1], sizename);
                        portn = (uint16_t) atoi(argv[2]);
                        /* Open TCP socket */
                        if( (Tcp_Socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                                free(hostname);
                                return 7;
                            }
/*------------------------- Configure settings of the server address struct ---------------------------------------
                        * Setting zeros in serverAddr */
                        bzero((char *) &Server_Addr, sizeof(Server_Addr));
                        /* Address family = Internet */
                        Server_Addr.sin_family = AF_INET;
                        /* Set port number, using htons function to use proper byte order */
                        Server_Addr.sin_port = htons(portn);
                        /* Set IP address */
                        Server_Addr.sin_addr.s_addr = inet_addr(hostname);
/*----------------------- Connect the socket to the server using the address struct ---------------------------*/
                        if (connect(Tcp_Socket, (struct sockaddr *) &Server_Addr,sizeof(Server_Addr)) < 0) {
                                    free(hostname);
                                    state=7;
                                    break;
                                }
                        free(hostname);
                                    
                        printf("Succesful connected to the server.\nServer IP address is - %s\nPort is - %s\n", inet_ntoa(Server_Addr.sin_addr), argv[2]);
                        state=1;                                                                  //Move Wait welcom state
                        break;
                    case 1:                                                                        // Wait wellcome
//-------------------------------------------Hello message--------------------------------------------------------------------------
                        if(send(Tcp_Socket,SendMessage, 3, 0)==-1){                                // Send Hello message + check 
                                printf("Error on sending hello/n");
                                state=7;                                                                                       // move to state - Close Connction   
                                break;
                        }
                        RestTime(flag);                                                                         //set timeout
                        FD_ZERO(&fds);                                                                        //  reset file descriptor
                        FD_SET(Tcp_Socket, &fds);                                                     // set socket -> fd 
//-------------------------------------------wait for welcom message---------------------------------------------------------
                        SelectNumber = select(Tcp_Socket+1, &fds, NULL, NULL, &Time_Out);
                        if(SelectNumber == 0){                                                                         // check for error or timeout 
                                printf("Time out exceeded");
                                state=7;                                                                                             //  move to state close connection  
                                break;
                        }
                        else if(SelectNumber==-1){
                                printf ("Error open select");
                                state=7;                                                                                                      // move to state close connection 
                                break;
                        }
                        if(FD_ISSET(Tcp_Socket, &fds)){                                                                //TCP socket is open return number else return 0
                        // Format : int recv(int socket, char *buffer, int length, int flags);  005255.255.255.25579   9 size buffer
                                Receive = recv(Tcp_Socket, ReceiverBuffer, 9, 0);                   // Received a message from the server "welcom"
                        }
                        if(checkWelcome(Receive)!=Welcom){
                                printf("message error - not welcom message\n");                         //error massege 
                                state=7;                                                                                                   //  move to state close connection 
                                break;
                        }
                       
                        Number_Stations = ntohs(*(uint16_t*)&ReceiverBuffer[1]);                 //get the number of station from the wellcome message
                        Multicast_IP.s_addr = ntohl(*(uint32_t*)&ReceiverBuffer[3]);              // Multicast IP 
                        printf("-----------------------------------------------------------------------------------\n");
                        printf("welcom\n MultiCast IP: %s\n", inet_ntoa(Multicast_IP));            // print welcome message information 
                        printf(" Number of stations: %d\n", Number_Stations);
                        printf("Port : %d\n",ntohs(*(uint16_t*)&ReceiverBuffer[7]));
                        printf("-----------------------------------------------------------------------------------\n");
                        Radio_station.id = 0; 
                        Radio_station.UDP_MulticastIP = Multicast_IP;
                        Radio_station.UDP_Port = ntohs(*(uint16_t*)&ReceiverBuffer[7]);
                        Radio_station.play_song = Turn_on;
//----------------------------------------------Start to stream station and create thread-------------------------------------------------------------------------------
                        if(pthread_create(&thread ,NULL ,UDPaudio , &Radio_station)!=0){
                                printf("Error on creating new Thread");
                                state=7;                                                                                                                // move to state - Close Connction 
                                break;
                        }
                        printf(menu);
                        state=2;            // Move to Established state 
                        break;
                    
                case 2:                      /* Established state (input from the user or for a newstation messages)  */
                            /* First, we initialize fds so that fd zero and the socket connection are set:   */
                        FD_ZERO(&fds);                                                     // clear file descriptor
                        FD_SET(0, &fds);                                                    // set the stdin
                        FD_SET(Tcp_Socket, &fds);                                  // set socket
                        selectcheck =select(Tcp_Socket+1,&fds,NULL,NULL,NULL);
                        // check for error
                        if(selectcheck ==-1){
                                printf("Error whit select function (Established state)\n");
                                state=7;                                                  // close connection state
                            break;
                        }
                        else if(selectcheck==0){
                                printf("Timeout exceeded wait for wellcome message (Established state)\n");
                                state=7;                                                     // close connection state
                                break;}
                    
                        if(FD_ISSET(Tcp_Socket, &fds)){                                                     // TCP  socket now activated  
                                receivMsg = recv(Tcp_Socket, ReceiverBuffer, 3, 0);
                                messagetype=*(uint8_t*)&receivMsg;
                                if(messagetype != 4){                                                                   // check if the message is NEW STATIONS 
                                        printf("Close connection, We didn't  receive a message for a new station\n");
                                        state=7;                                                                                     // close connection state
                                        break;     
                                }
                               else{
                                        Number_Stations++;                                                           // increment the stations counter by 1
                                        printf("Another radio station was created, now there are %d active stations.\n",Number_Stations);
                            }
                        }
 
                       if(FD_ISSET(0, &fds)){                                  //  FD_ISSET() to test to see if a particular bit is set.
                                    gets(input);
                                    /* AskSong:
                                                uint8_t commandType = 1;
                                                uint16_t stationNumber;   */
       
                                    // in the biginning all the client listen Radio station number 0.
                                    if(strcmp(input,"0")==0 || (atoi(input)>0 && atoi(input)<Number_Stations)){
                                                     // if is True we need to send message Ask song to the server 
                                                if(Number_Stations==0){
                                                        printf("No stations avaliable, try again later.\n");
                                                        break;
                                                    }     
                                                SendMessage[0]=1;                                                  // type message  Ask song 
                                                *(uint16_t*)&SendMessage[1]=htons(atoi(input));             // Station Number
                                                if(send(Tcp_Socket, SendMessage,3,0)>=0){
                                                        state = 3;                                                                             // move to wait announce state   
                                                        break;
                                                }
                                                else{
                                                        printf("Error sending type message Ask song\n");
                                                        state=7;                                                                             //  move to state close connection     
                                                        break;
                                                }
                                    }
                                    else if(strcmp(input,"s")==0){
                                        //go to UpSong state
                                        state=UPLOAD_SONG;
                                        break;

                                    }
                                else if(strcmp(input,"q")==0){
                                            printf("Bye!\n");
                                            state=CLOSE_CONNECTION;
                                            break;
                                        }
                            else if(strcmp(input,"help")==0 || strcmp(input,"?")==0 || strcmp(input,"-help")==0){
                                printf("\nRadio Controller \nproject Shahaf Telecom\n\ncommands:\n[0-%d]	"
                                            "Change station with the number typed and get the name of the song currently being played"
                                            " on that station.\ns	Entering to Upload song to the connected serverAfter \nq"
                                            "	Leave program.\n\n",Number_Stations==0 ? 0 : Number_Stations-1);
                            }
                            else{
                                printf("Incorrect input, type -help or ? for help\n");
                            }

                        }
                        break;
                
                case 3:                                                                      //announce state after ask song
                /*  check for ANNOUNCE from the server or for a newstation message.   */
                        FD_ZERO(&fds);                                              // clear file descriptor
                        FD_SET(Tcp_Socket, &fds);                         // set socket
                        RestTime(flag);
                        selectcheck = select(Tcp_Socket+1, &fds, NULL, NULL, &Time_Out);
                        if(selectcheck==-1){
                                printf("Error whit select function (announce state)\n");
                                state=7;                                                     // close connection state
                        break;
                        }
                        else if(selectcheck==0){
                                printf("Timeout exceeded  whit for wellcome message (announce state)\n");
                                state=7;                                                      // close connection state
                                break;}
               
                        if(FD_ISSET(Tcp_Socket, &fds)){                    // TCP  socket now activated  
                                /*  Newstation 
                                        uint8_t replayType =4 
                                        uint16_t newStationNumber;
                                */
                                msgtype=checkMessage(ReceiverBuffer,recv(Tcp_Socket, ReceiverBuffer, Buffer, 0));
                                //we expect to get only NEW_STATIONS message from the server
                                if(msgtype==New_Station){               //check if the message is NEW STATIONS
                                        Number_Stations++;
                                        printf("A new radio station has been announced.\nNow we have %d Radio stations avaliable!\n",Number_Stations);   // after Number_Stations++
                                }
                                else if(msgtype==Announce){                                                              // announce message
                                            songlen=*(uint8_t*)&ReceiverBuffer[1];              //length of the song name
                                            char upSongName[songlen+1];
                                            int start_index = 2;
                                            int substring_length = songlen+2;
                                            memcpy(upSongName, ReceiverBuffer + start_index, substring_length);
                                            upSongName[substring_length] = '\0';
                                            printf("Got Announce message\nThe song name in station %d is:\n",atoi(input));
                                            printf("%s",upSongName);
                                            //chane current station details for the UDP listener thread
                                            Radio_station.id= atoi(input);
                                            Radio_station.UDP_MulticastIP.s_addr =htonl(ntohl(Multicast_IP.s_addr)+atoi(input));
                                            state=2;
                                        break;
                                }
                                else{
                                    //got non welcome or annonce message
                                    //close connection function
                                    printf("Wrong message type, Close connection\n");
                                    state=7;
                                    break;
                                }
                            }
                            break;
                            
                    case 4:                      //  Upload song state 
                            printf("Upload song, \n"
                                        "enter the song file name '*.mp3'\n");
                            gets(input);                                      //get the song name by the user input
                            fp = fopen (input ,"r");
                            if(fp==NULL){
                                        printf("Error: Could not open file\n");
                                        state=2;
                                        break;      
                            }

                            long size;
                            fseek(fp, 0L, SEEK_END);
                            size = ftell(fp);
                            rewind(fp);                                                               // Return cursor to the top of the file
                            if(size<2000 || size>10000000){                         // check the size file 
                                    printf("The file size should be between 2KB and 10MBB\n");
                                    fclose(fp);
                                    state=2;
                                    break;
                            }
                            
                            int NameSize=strlen(input);
                            //send song to the server
                            SendMessage[0]=2;                                                       // type message
                            *(uint32_t*)&SendMessage[1]=htonl(size);        //  Size file song
                            SendMessage[5]=NameSize;
                            //copy song name string to the buffer we will send
                            strncpy(SendMessage+6, input, NameSize);
                            if(send(Tcp_Socket,SendMessage,NameSize+6,0)==-1){          //send Upsong message to the server + check 
                                printf("Error: sending upload request\n");
                                state=7;                                                             //Close the connecnet
                                break;
                    }
                    state=5;                                                                         // move to permit state    
                    break;
                            
                         break;   
                    case 5:                                        // permit state
                            // here we need to wait to premition to send the song or get a newstation message from the server
                            FD_ZERO(&fds);                                                     // clear file descriptor
                            FD_SET(Tcp_Socket, &fds);                               // set socket 
                                                //set timeout
                            RestTime(flag);
                            selectcheck=select(Tcp_Socket+1,&fds,NULL,NULL,&Time_Out);
                            if(selectcheck ==0){
                                    printf("Timeout exceeded while waiting for permit");
                                    state=7;
                                    break;}
                            else if(selectcheck==-1){
                                    puts("Error whit select function (permit state)");
                                    state=7;
                                    break;
                                }
                            else if(FD_ISSET(Tcp_Socket,&fds)){                 //we expect to get only NEW_STATIONS or permit messages from the server
                                        msgtype= checkMessage( ReceiverBuffer,(recv(Tcp_Socket, ReceiverBuffer, Buffer, 0)));
                                        //msgtype=checkNewstation(recv(Tcp_Socket, ReceiverBuffer, Buffer, 0));   // check if the message is NEW STATIONS
                                        if(msgtype==New_Station){                                             // check if the message is type new station =4 
                                                Number_Stations++;
                                                printf("A new radio station has been announced.\nNow we have %d Radio stations avaliable\n",Number_Stations);
                                                state=5;
                                                break;
                                        }
                                        else if(msgtype==PERMIT_SONG){
                                                    if((int)ReceiverBuffer[1]==1){                      //check permit type if its true we can Upload song
                                                                printf("Got permit to upload\n");
                                                                state=6;
                                                                break;
                                                            }
                                                    else{
                                                        //cant upload song, go back to ESTABLISH
                                                        printf("There is no permit from server, Please try again later.\n");
                                                        state=2;                    //move to established state
                                                        break;
                                                    }
                                        }
                                        else{
                                                printf("Expected to get Permit song  or new station but got something else. closing the connected\n");
                                                state=7;
                                                break ; }
                            }
                        break; 
    
                    case 6:              //Send song state
                        printf("Uploading %ld. .%2ld MB to the server\n",size/1000000,size%1000000);
                        BytesRead=Buffer;
                        Bytesent=0;

                        while(Bytesent<size && Bytesent==Buffer){
                            Bytesent=fread(SendMessage,1,Buffer,fp);
                            SendFileStatus = send(Tcp_Socket,SendMessage,BytesRead,0);
                            if(SendFileStatus==-1){
                                    printf("Error: on upload the song");                              
                                    state=7;
                                    break;
                            }
                            else{                                                      //send success
                                Bytesent+=SendFileStatus;
                                Process("Please wait... ",Bytesent, size);
                            }
                            usleep(8000);                        //  you must set the upload rate at the client to 1KiB every 80000 usec. // 
                                                                                                    //Use #define to properly include these constants in your code!
                            }
                            if(state==7)
                                        break;
                            printf("\nFinish upload the song\n\n");
                            while ( getchar() != '\n' );       //is a loop that is used to clear the input buffer of the standard input stream (stdin). //clear the input buffer
                            state=2;                                                           // back to establise state 
                            break; 
     
                    case 7:
                        printf("Clos the conncted \n");
                        Radio_station.play_song=0;                   // off play mode 
                        if(fp!=NULL)    
                            fclose(fp);
                        pthread_join(thread,NULL);
                        if(Tcp_Socket>0)
                            close(Tcp_Socket);                      // close the Socket tcp from the server
                        if(UDP_socket>0)
                            close(UDP_socket);                     // close the Socket udp from the multicast group 
                        return EXIT_SUCCESS;
                    state=-1;
                    break;
                }
            }
        return EXIT_SUCCESS;
}


/*     * The htonl() function translates a long integer from host byte order to network byte order. The unsigned 
            long integer to be put into network byte order.
            Is typed to the unsigned long integer to be put into network byte order.
             
         * INADDR_ANY
            This is an IP address that is used when we don't want to bind a socket to any specific IP. Basically, 
            while implementing communication, we need to bind our socket to an IP address. 
            When we don't know the IP address of our machine, we can use the special IP address INADDR_ANY.
            It allows our server to receive packets that have been targeted by any of the interfaces.
             
        *  IPPROTO_IP
            This constant has the value 0. It's actually an automatic choice depending on socket type and family.
            If you use it, and if the socket type is SOCK_STREAM and the family is AF_INET, then the protocol will 
            automatically be TCP (exactly the same as if you'd used IPPROTO_TCP). 
            But if you use IPPROTO_IP together with AF_INET and SOCK_RAW, you will have an error, because the kernel
            cannot choose a protocol automatically in this case.
              
            
         *  FD_ISSET
             Returns a non-zero value if the bit for the file descriptor fd is set in the file descriptor set pointed to by fdset, and 0 otherwise.
 */
 
 
 
 
 
 
 
 