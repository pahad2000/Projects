#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>

extern int errno;

struct udp_args {
char args[3][50];
//struct sockaddr_in servaddr;
//struct sockaddr_in succ1addr;
//struct sockaddr_in succ2addr;
};

struct ping_args {
char args[3][50];
//struct sockaddr_in succ1addr;
//struct sockaddr_in succ2addr;
};


struct file_args {
char args[3][50];
//struct sockaddr_in succ1addr;
};

struct tcp_args {
char args[5][50];
//struct sockaddr_in servaddr;
//struct sockaddr_in succ1addr;
clock_t timer;
}; 

int pseq1 = 0, pseq2 = 0, terminate = 0, lastrec1 = 0, lastrec2 = 0 ;
struct sockaddr_in pred1, pred2, servaddr, succ1addr, succ2addr;

int digits_only(const char *s) {
    while (*s) {
        if (isdigit(*s++) == 0) return 0;
    }
    return 1;
}
// Generates a random number in range [min, max]
float randgen(float min, float max)
{
    float factor = rand() / (float) RAND_MAX; 
    return min + factor * (max - min);      
} 

int hasfile(int filename, int pred, int curr) {
    // curr and pred are the identifiers of the current peer and it's predecessor 
    int hash = filename % 256;
    // check if hash is in the interval (pred,curr] 
    
    // if hash is in the interval (pred,curr], and pred < curr e.g. 1->2 etc.
    if (curr >= hash && pred < curr && pred < hash) return 1;
    // if hash is in the interval (pred,curr], and pred > curr e.g. 15->1 etc.
    if (curr <= hash && pred > curr && pred < hash) return 1;
    else return 0;       
}
// function to receive a file using udp
void filereceive(int sockfd, char args[5][50], clock_t time, int sender) {
    FILE *fp, *rec;
    struct sockaddr_in from, to;
    socklen_t flen = sizeof(from);
    socklen_t tlen = sizeof(to);
    char buf[10000], data[1000], event[2][100] = {"snd","rcv"}, rceit1[50], rceit2[50];
    char resp[100], flag[] = "0\n", filename[1000] = "received_file.pdf";
    int seq, ack, start = 0;
    char cseq[10], cack[10], mss[10];
    double progtime;
    // Initialise acknowledgement
    seq = 0;
    ack = 1;
    sprintf(cseq, "%d", seq);
    sprintf(cack, "%d", ack);
    strcpy(resp, cseq);
    strcat(resp, "\n");
    strcat(resp, cack);
    strcat(resp, "\n");
    strcat(resp, flag); 
    strcat(resp, "\n");
    // Open file
    if ((fp = fopen(filename, "wb+")) == NULL) {
        perror("Could not open file\n");
        exit(EXIT_FAILURE);
    }
    if ((rec = fopen("requesting_log.txt", "w+")) == NULL) {
        perror("Could not open file\n");
        exit(EXIT_FAILURE);
    }
    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Could not open socket\n");
        exit(EXIT_FAILURE);
    }
     // Make socket reusable 
    int reuse1 = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse1, sizeof(reuse1)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    #ifdef SO_REUSEPORT
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse1, sizeof(reuse1)) < 0) 
        perror("setsockopt(SO_REUSEPORT) failed");
    #endif
    memset(&from, 0, sizeof(from));
    from.sin_family = AF_INET;
	from.sin_addr.s_addr = INADDR_ANY;
	from.sin_port = htons(60000 + atoi(args[0]));
	memset(&to, 0, sizeof(to));
    to.sin_family = AF_INET;
	to.sin_addr.s_addr = INADDR_ANY;
	to.sin_port = htons(60000 + sender);
	
	if (bind(sockfd, (struct sockaddr *)&from, sizeof(from)) < 0) { 
        perror("could not bind\n"); 
        exit(EXIT_FAILURE); 
    }  
    // Loop to receive file using stop and wait mechanism
    while (1) {
          start = 0;
          if (recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&from, &flen) < 0) {
            printf("Could not receive message from peers\n");
            exit(EXIT_FAILURE);
          } 
          strcpy(rceit1, strtok(buf, "\n"));
          start += strlen(rceit1);
          strcpy(rceit2, strtok(NULL, "\n"));
          start += strlen(rceit2);
          strcpy(mss, strtok(NULL, "\n"));
          start += strlen(mss);
          strcpy(data, strtok(NULL, "\n"));
          start += strlen(data);
          start += 4; 
          progtime = (double)1000*(clock() - time)/CLOCKS_PER_SEC;
          fprintf(rec, "%-20s %10.2f %10d %10d %10d\n", event[1], progtime, atoi(rceit1), atoi(mss), atoi(rceit2));  
          // Structure is seqno\nackno\nMSS\nflag\nDATA\n    

          if (atoi(data) == 0) {
            fwrite(&buf[start], atoi(mss), 1, fp);
            ack += atoi(mss);
            sprintf(cack, "%d", ack);
            strcpy(resp, cseq);
            strcat(resp, "\n");
            strcat(resp, cack);
            strcat(resp, "\n");
            strcat(resp, flag);
            strcat(resp, "\n");
            if (sendto(sockfd, resp, sizeof(resp), 0, (struct sockaddr *)&to, tlen) < 0) { 
	            printf("Could not send packet\n");
            }
            progtime = (double)1000*(clock() - time)/CLOCKS_PER_SEC;
            fprintf(rec, "%-20s %10.2f %10d %10d %10d\n", event[0], progtime, seq, atoi(mss), ack);  
          } else {
            fwrite(&buf[start], atoi(mss), 1, fp);
            strcpy(flag,"1");
            ack += atoi(mss);
            sprintf(cack, "%d", ack);
            strcpy(resp, cseq);
            strcat(resp, "\n");
            strcat(resp, cack);
            strcat(resp, "\n");
            strcat(resp, flag);
            strcat(resp, "\n");
            if (sendto(sockfd, resp, sizeof(resp), 0, (struct sockaddr *)&to, tlen) < 0) { 
	        printf("Could not send packet\n");
            }
            progtime = (double)1000*(clock() - time)/CLOCKS_PER_SEC;
            fprintf(rec, "%-20s %10.2f %10d %10d %10d\n", event[0], progtime, seq, atoi(mss), ack);  
            break;
          }
          
    }
 
    printf("The file is received.\n");
    fclose(rec);
    fclose(fp);
    close(sockfd);
    return;
}






// function to send a file using udp
void filesend (int sockfd, char filename[50], int requestor, char args[5][50], clock_t time) {
    clock_t before;
    struct sockaddr_in from, to;
    int secs, seq, ack;
    double progtime, drop = atof(args[4]), num;
    char cseq[10], cack[10], datlen[10], recbuf[1000], rceit1[50], rceit2[50], flag[10], done[10];
    char event[5][50] = {"snd","rcv","Drop","RTX","RTX/Drop"};
    strcpy(datlen, args[3]);
    char segment[1000], payload[1000] = {0};
    FILE *fp, *rec;
    socklen_t tlen = sizeof(to);
    socklen_t flen = sizeof(from);
    seq = 1;
    ack = 0;
    memset(&from, 0, sizeof(from));
    from.sin_family = AF_INET;
	from.sin_addr.s_addr = INADDR_ANY;
	from.sin_port = htons(60000 + atoi(args[0]));
	memset(&to, 0, sizeof(to));
    to.sin_family = AF_INET;
	to.sin_addr.s_addr = INADDR_ANY;
	to.sin_port = htons(60000 + requestor);
	sleep(3);
    // Create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Could not open socket\n");
        exit(EXIT_FAILURE);
    
    } 
    
    int reuse1 = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse1, sizeof(reuse1)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    #ifdef SO_REUSEPORT
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse1, sizeof(reuse1)) < 0) 
        perror("setsockopt(SO_REUSEPORT) failed");
    #endif
	if (bind(sockfd, (struct sockaddr *)&from, sizeof(from)) < 0) 
    { 
        perror("could not bind\n"); 
        exit(EXIT_FAILURE); 
    }     

    strcpy(flag, "0");
    if ((rec = fopen("responding_log.txt", "w+")) == NULL) {
        perror("Couldnt open file\n");
        exit(EXIT_FAILURE);
    }
    // Open binary file to be transferred
    strcat(filename, ".pdf");
    if ((fp = fopen(filename, "rb")) == NULL) {
        perror("Couldnt open file\n");
        exit(EXIT_FAILURE);
    }
    fseek(fp, 0, SEEK_END);
    long int filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    // Transfer data in loops
    while (1) {
        sprintf(cseq, "%d", seq);
        sprintf(cack, "%d", ack);
        // Initialise packet with header information 
        // Structure is seqno\nackno\nMSS\nfilesize\nflag\nDATA\n
        strcpy(segment, cseq);
        strcat(segment, "\r\n");
        strcat(segment, cack);
        strcat(segment, "\r\n");
        strcat(segment, datlen);
        strcat(segment, "\r\n");
        strcat(segment, flag);
        strcat(segment, "\r\n");
        // Read in MSS bytes of data
        if (feof(fp) == 0) {
            int read = filesize - ftell(fp);
            if (read > 0) {
                if (read >= atoi(datlen)) {
                   fread(payload, atoi(datlen), 1, fp);
                   strcat(segment, payload);                  
                } else {
                   fread(payload, read, 1, fp);
                   sprintf(datlen, "%d", read);
                   strcpy(flag, "1");
                   strcpy(segment, cseq);
                   strcat(segment, "\n");
                   strcat(segment, cack);
                   strcat(segment, "\n");
                   strcat(segment, datlen);
                   strcat(segment, "\n");
                   strcat(segment, flag);
                   strcat(segment, "\n");
                   strcat(segment, payload);
                }
            } else if (read == 0) {
                printf("Nothing left to read\n");
            }                
        }

        // Either drop or send segment 
        if ((num = randgen(0,1)) < drop) {
            progtime = (double)1000*(clock() - time)/CLOCKS_PER_SEC;
            fprintf(rec, "%-20s %10.2f %10d %10d %10d\n", event[2], progtime, seq, atoi(datlen), ack);    
            // Start timer
            before = clock();
            secs = 0;
            
            while (secs < 1) {
                secs = (int)(clock() - before)/CLOCKS_PER_SEC;
                
                if (secs == 1) {
                    if ((num = randgen(0,1)) < drop) {
                        progtime = (double)1000*(clock() - time)/CLOCKS_PER_SEC;
                        fprintf(rec, "%-20s %10.2f %10d %10d %10d\n", event[4], progtime, seq, atoi(datlen), ack);
                    } else {
                        if (sendto(sockfd, segment, sizeof(segment), 0, (struct sockaddr *)&to, tlen) < 0) {
                        printf("Could not send packet\n");
                        exit(EXIT_FAILURE);
                        }
                        progtime = (double)1000*(clock() - time)/CLOCKS_PER_SEC;
                        fprintf(rec, "%-20s %10.2f %10d %10d %10d\n", event[3], progtime, seq, atoi(datlen), ack);
                        break;
                    }
                    before = clock();
                    secs = 0;
                }
            }
            
        } else { 
            if (sendto(sockfd, segment, sizeof(segment), 0, (struct sockaddr *)&to, tlen) < 0)          {
                printf("Could not send packet\n");
                exit(EXIT_FAILURE);
            }          
            progtime = (double)1000*(clock() - time)/CLOCKS_PER_SEC;
            fprintf(rec, "%-20s %10.2f %10d %10d %10d\n", event[0], progtime, seq, atoi(datlen), ack);
        } 
        
        if (recvfrom(sockfd, recbuf, sizeof(recbuf), 0, (struct sockaddr *)&from, &flen) < 0)        {
                printf("Could not receive ack\n");
                exit(EXIT_FAILURE);
          }   
        strcpy(rceit1, strtok(recbuf, "\n"));
        strcpy(rceit2, strtok(NULL, "\n"));
        strcpy(done, strtok(NULL, "\n"));
        progtime = (double)1000*(clock() - time)/CLOCKS_PER_SEC;
        fprintf(rec, "%-20s %10.2f %10d %10d %10d\n", event[1], progtime, atoi(rceit1), atoi(datlen), atoi(rceit2));

        if (atoi(done)) break;
        seq += atoi(datlen);
    }
    // Close UDP socket and file
    fclose(fp);
    fclose(rec);
    close(sockfd);
    printf("The file is sent.\n");    
    return;
}

// function to listen for ping requests
void *userv (void *arg1) {
    struct udp_args *uargs = (struct udp_args *)arg1;
    char sendbuf[1000], recbuf[1000]; 
	int sockfd, update, i = 0;
	char rec[3][100], ureq[50];
	char resp[] = "Response\n";
	struct sockaddr_in to; 
	socklen_t tolen = sizeof(to);
	socklen_t servlen = sizeof(servaddr);
    memset(&to, 0, sizeof(to));
    
	sockfd = socket(AF_INET,SOCK_DGRAM,0);
	
	int reuse1 = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse1, sizeof(reuse1)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    #ifdef SO_REUSEPORT
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse1, sizeof(reuse1)) < 0) 
        perror("setsockopt(SO_REUSEPORT) failed");
    #endif

	if (bind(sockfd, (struct sockaddr *)&servaddr, servlen) < 0) 
    { 
        perror("could not bind\n"); 
        exit(EXIT_FAILURE); 
    } 

	while (1) {
		if (recvfrom(sockfd, recbuf, sizeof(recbuf), 0, (struct sockaddr *)&servaddr, &servlen) < 0) {
			printf("Could not receive information from peers\n");
			exit(EXIT_FAILURE);
		}
		strcpy(rec[0],strtok(recbuf,"\n"));
		strcpy(rec[1],strtok(NULL,"\n"));
		strcpy(rec[2],strtok(NULL,"\n"));

		// Check nature of message
		if (strcmp(rec[0], "Response") == 0) { // if its a response message
		    // IF pinger receives a 
		    if ((pseq1 < atoi(rec[2]) && atoi(rec[1]) == atoi(uargs->args[1])) 
		        || (pseq2 < atoi(rec[2]) && atoi(rec[1]) == atoi(uargs->args[2]))) {
		        continue;
	        }
		    printf("A ping response message was received from Peer %s\n", rec[1]);
		    // if first successor
		    if (atoi(rec[1]) == ntohs(succ1addr.sin_port) - 50000) {
		        lastrec1 = atoi(rec[2]);
		    } else if (atoi(rec[1]) == ntohs(succ2addr.sin_port) - 50000) {
		        lastrec2 = atoi(rec[2]);
		    }
		    // Assume successor 1 or successor 2 is dead if they do not respond 
		    // to 10 ping messages 
		    if (pseq1 - lastrec1 == 10) {
		    	strcpy(ureq, "u");
	            strcat(ureq, "\n");
	            strcat(ureq, uargs->args[0]);
	            strcat(ureq, "\n");
		    	strcat(ureq, "2\n");
		    	strcat(ureq, uargs->args[1]);
		    	strcat(ureq, "\n");
		        printf("Peer %d is no longer alive\n", ntohs(succ1addr.sin_port)-50000);
		        succ1addr = succ2addr;
		        printf("My first successor is now peer %d.\n", ntohs(succ1addr.sin_port)-50000 ); 
		        // Send a TCP message to new first successor to find new second successor 
		        if ((update = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		            perror("Couldnt open socket\n");
		            exit(EXIT_FAILURE);
		        } if (connect(update, (struct sockaddr *)&succ1addr, sizeof(succ1addr)) < 0) {
                    perror("Could not connect\n");
                    exit(EXIT_FAILURE);
                } if (send(update, ureq, sizeof(ureq), 0) < 0) {
                    perror("Could not send file request\n");
                    exit(EXIT_FAILURE);
                } 
                close(update);
                pseq1 = 0;
                sleep(1);
		    } if (pseq2 - lastrec2 == 10) {
		    	strcpy(ureq, "u");
	            strcat(ureq, "\n");
	            strcat(ureq, uargs->args[0]);
	            strcat(ureq, "\n");
		    	strcat(ureq, "1\n");
		    	strcat(ureq, uargs->args[2]);
		    	strcat(ureq, "\n");
	            printf("Peer %d is no longer alive\n", ntohs(succ2addr.sin_port)-50000);	 
		        printf("My first successor is now peer %d.\n", ntohs(succ1addr.sin_port)-50000);   
	            // Send a TCP message to first successor to find new second successor 
	            if ((update = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		            perror("Couldnt open socket\n");
		            exit(EXIT_FAILURE);
		        } if (connect(update, (struct sockaddr *)&succ1addr, sizeof(succ1addr)) < 0) {
                    perror("Could not connect\n");
                    exit(EXIT_FAILURE);
                } if (send(update, ureq, sizeof(ureq), 0) < 0) {
                    perror("Could not send file request\n");
                    exit(EXIT_FAILURE);
                } 
                close(update);
                pseq2 = 0;
                sleep(1);
		    }
		    
		} else if (strcmp(rec[0], "Request") == 0) { // if its a request message
		    printf("A ping request message was received from Peer %s\n", rec[1]);  
		    // Format the ping request using well-defined syntax (syntax is "Request|Response\nPeerNo\n")
		    strcpy(sendbuf, resp);
		    strcat(sendbuf, uargs->args[0]); 
		    strcat(sendbuf, "\n");
		    strcat(sendbuf, rec[2]);
		    strcat(sendbuf, "\n");
		    // Initialise socket destination for ping response
		    to.sin_family = AF_INET;
	        to.sin_addr.s_addr = INADDR_ANY;
	        to.sin_port = htons(50000 + atoi(rec[1]));
	        // Record predecessor addresses
	        if (i == 0) {
	            pred1 = to;
	        } else if (to.sin_port != pred1.sin_port) {
	            pred2 = to;
	        }
	        i += 1;    
		    if (sendto(sockfd, sendbuf, sizeof(sendbuf), 0, (struct sockaddr *)&to, tolen) < 0) {
    	        printf("Could not send response to peer\n");
		        exit(EXIT_FAILURE);
		    }
		}
	}
	close(sockfd);


}

// Send a ping to successor peers every 5 seconds 
void *pinger (void *arg2) {
    struct ping_args *pargs = (struct ping_args *)arg2;
	char sendbuf[1000];  
	char req[] = "Request\n", seq[10];
	clock_t before, difference;
	int sockfd, seconds = 0; 
	socklen_t succ1len = sizeof(succ1addr); 
	socklen_t succ2len = sizeof(succ2addr); 
	// start the timer
	before = clock();  
	while (seconds < 10) {
	    //if (pseq1 - lastrec1 == 10 || pseq2 - lastrec2 == 10) 
	    //sleep(1);
	    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
            printf("Could not create socket\n"); 
            
		// calculate the time interval 
		difference = (clock() - before);
		seconds =  difference/CLOCKS_PER_SEC;
		// Send ping once time interval of 10 seconds is reached
		if (seconds == 10) {
            strcpy(sendbuf,req); 
	        strcat(sendbuf,pargs->args[0]); 
	        strcat(sendbuf, "\n");
	        sprintf(seq, "%d", pseq1);
	        strcat(sendbuf, seq);
	        strcat(sendbuf, "\n");
			//send ping request to 1st successor peer
			if (sendto(sockfd, sendbuf, sizeof(sendbuf), 0, (struct sockaddr *)&succ1addr, succ1len) < 0) {
	       		perror("Could not send ping to successor 1\n");
	       		exit(EXIT_FAILURE);
			//send ping request to 2nd successor peer 
			} 
			pseq1 += 1;
			strcpy(sendbuf,req); 
	        strcat(sendbuf,pargs->args[0]); 
	        strcat(sendbuf, "\n");
	        sprintf(seq, "%d", pseq2);
	        strcat(sendbuf, seq);
	        strcat(sendbuf, "\n");
			sleep(5);
			if (sendto(sockfd, sendbuf, sizeof(sendbuf), 0, (struct sockaddr *)&succ2addr, succ2len) < 0) {
	        	perror("Could not send ping to successor 2\n");
	        	exit(EXIT_FAILURE);
        	}
        	pseq2 += 1;
			// Need to reset the difference in order to keep pinging continuous 
			before = clock();
			seconds = 0;
		}
	close(sockfd);	
    } 
    return 0;
}
// Requests file from successor given stdin prompt
void *filerequest (void *arg3) {
    struct file_args *fargs = (struct file_args*) arg3;
    int sockfd;
    char input[1000], req[2][1000], msg[1000], departure[1000];
    sleep(3);
    // Read a line from stdin 
    while (fgets(input,1000,stdin) != NULL) {
        // Break down elements of request (should have two elements representing 
        //"request" and filename)
        // Open a socket
        strcpy(req[0],strtok(input," "));
        if (!strcmp(req[0],"quit\n")) {
            if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                perror("Could not open socket");
                exit(EXIT_FAILURE);
            }
            strcpy(departure, "d");
            strcat(departure, fargs->args[0]);
            strcat(departure, "\n");
            strcat(departure, fargs->args[1]);
            strcat(departure, "\n");
            strcat(departure, fargs->args[2]);
            strcat(departure, "\n");
            if (connect(sockfd, (struct sockaddr *)&pred1, sizeof(pred1)) < 0) {
                perror("Could not connect\n");
                exit(EXIT_FAILURE);
            } 
            if(send(sockfd, departure, sizeof(departure), 0) < 0) {
                perror("Could not send file request\n");
                exit(EXIT_FAILURE);
            } 
            close(sockfd);
            if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                perror("Could not open socket");
                exit(EXIT_FAILURE);
            }
            if (connect(sockfd, (struct sockaddr *)&pred2, sizeof(pred2)) < 0) {
                perror("Could not connect\n");
                exit(EXIT_FAILURE);
            } if(send(sockfd, departure, sizeof(departure), 0) < 0) {
                perror("Could not send file request\n");
                exit(EXIT_FAILURE);
            }
            close(sockfd);
            terminate = 1;
            continue;
        }
        strcpy(req[1],strtok(NULL, " \n"));
        // Check that first element of request is in fact request
        if (strcmp(req[0], "request") != 0) {
            printf("Invalid file request. Please try again\n");
            continue;
        }
        // Check that second element of request is a number
        if (digits_only(req[1]) != 1) {
            printf("Invalid file request. Please try again\n");  
            continue;
        } 
        // Syntax for file request is "peerno(of requestor)\nfilename\npeerno(of sender)\n"
        strcpy(msg,fargs->args[0]);
        strcat(msg,"\n");
        strcat(msg,req[1]);
        strcat(msg,"\n");
        strcat(msg,fargs->args[0]);
        strcat(msg,"\n");
        //printf("msg is %s", msg);
        
        // Create a socket
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0))< 0) {
            printf("Could not create socket\n");  
            exit(EXIT_FAILURE);
        }
        // Connect to successor peer
        if (connect(sockfd, (struct sockaddr *)&succ1addr, sizeof(succ1addr)) < 0) {
            perror("Could not connect\n");
            exit(EXIT_FAILURE);
        }
        // Send file request
        if(send(sockfd, msg, sizeof(msg), 0) < 0) {
            perror("Could not send file request\n");
            exit(EXIT_FAILURE);
        }
        
        printf("File request message for %s has been sent to my successor.\n", req[1]);
        close(sockfd);
    }
    return 0;
}

// TCP Server that sends a file if requested
void *tserv (void *arg4) {
    struct tcp_args *targs = (struct tcp_args *) arg4;
    struct sockaddr_in cliaddr, requestor, term, update;
    int sockfd, commsock, filesock, succsock, ack1, ack2;
    socklen_t clilen = sizeof(cliaddr);
    char buf[1000];
    char req[4][1000], resp[10], succ2[1000];
    char found[1000], tack[100];
    socklen_t servlen = sizeof(servaddr);
    // Ensure no unwanted values are in any of the sockaddr_in structs
    memset(&cliaddr, 0, sizeof(cliaddr));
    memset(&requestor, 0, sizeof(requestor));
    memset(&term, 0, sizeof(term));
    ack1 = ack2 = 0;
    
    if ((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0) {
        printf("Could not open socket\n");
        exit(EXIT_FAILURE);
    }
        
    int reuse1 = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse1, sizeof(reuse1)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    #ifdef SO_REUSEPORT
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse1, sizeof(reuse1)) < 0) 
        perror("setsockopt(SO_REUSEPORT) failed");
    #endif
    
    if (bind(sockfd, (struct sockaddr *)&servaddr, servlen) < 0) 
    { 
        perror("could not bind\n"); 
        exit(EXIT_FAILURE); 
    }     
    
    while (1) {
        listen(sockfd, 10);
        if ((commsock = accept(sockfd, (struct sockaddr *)&cliaddr, &clilen)) < 0) {
            printf("Could not accept\n");
            exit(EXIT_FAILURE);
        }
        if (commsock < 0) {
            printf("Could not connect to file requesting peer\n");
            exit(EXIT_FAILURE);
        } if (recv(commsock, buf, sizeof(buf), 0) < 0) {
            printf("Could not read msg from file requesting peer\n");
            exit(EXIT_FAILURE);
        }
        strcpy(req[0],strtok(buf,"\n"));

        if (terminate == 1) {
            if (!strcmp(req[0], "ack1")) {
                ack1 = 1;
            }
            if (!strcmp(req[0], "ack2")) {
                ack2 = 1;
            }
            if (ack1 && ack2) {
                exit(EXIT_SUCCESS);
            }
            close(commsock);  
            continue;
        } if (!strcmp(req[0],"ru")) {
            strcpy(req[1],strtok(NULL,"\n"));
            strcat(resp, targs->args[1]);
            succ2addr.sin_family = AF_INET;
	        succ2addr.sin_addr.s_addr = INADDR_ANY;
	        succ2addr.sin_port = htons(50000 + atoi(req[1]));
            printf("My second successor is now peer %s.\n", req[1]); 
            close(commsock);
            continue;
        }
        strcpy(req[1],strtok(NULL,"\n"));
        strcpy(req[2],strtok(NULL,"\n"));
        if (req[0][0] == 'u') {
            strcpy(req[3],strtok(NULL,"\n"));
            strcpy(resp, "ru");
            strcat(resp, "\n");
            if (req[2][0] == '1') {  
                 if (ntohs(succ1addr.sin_port) - 50000 == atoi(req[3])) {
                    sprintf(succ2, "%d", ntohs(succ2addr.sin_port) - 50000);
                 } else {
                    sprintf(succ2, "%d", ntohs(succ1addr.sin_port) - 50000);
                 }       
            } else {
                 if (ntohs(succ1addr.sin_port) - 50000 == atoi(req[3])) {
                    sprintf(succ2, "%d", ntohs(succ2addr.sin_port) - 50000);
                 } else {
                    sprintf(succ2, "%d", ntohs(succ1addr.sin_port) - 50000);
                 }  
            }
            strcat(resp, succ2);
            strcat(resp, "\n");
            update.sin_family = AF_INET;
	        update.sin_addr.s_addr = INADDR_ANY;
            update.sin_port = htons(50000 + atoi(req[1]));
            if ((filesock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { 
		        printf("Could not connect to socket\n");
	            exit(EXIT_FAILURE);
	        }
		    if (connect(filesock, (struct sockaddr *)&update, sizeof(update)) < 0) {
		        perror("Could not connect to requestor\n");
	            exit(EXIT_FAILURE);
	        }
		    if (send(filesock, resp, sizeof(resp), 0) < 0) {
		        printf("Could not forward message to meh requestor\n");
                exit(EXIT_FAILURE);
            }
            close(filesock);
            close(commsock);
            continue;
        
        } 

        // Receiving message of succs graceful departure
        if (req[0][0] == 'd') {
            printf("Peer %s will depart from the network.\n",&req[0][1]);
            if (!strcmp(targs->args[1], &req[0][1])) {
                strcpy(tack, "ack1");
                succ1addr.sin_family = AF_INET;
	            succ1addr.sin_addr.s_addr = INADDR_ANY;
	            succ1addr.sin_port = htons(50000 + atoi(req[1]));
	    	    succ2addr.sin_family = AF_INET;
        	    succ2addr.sin_addr.s_addr = INADDR_ANY;
	            succ2addr.sin_port = htons(50000 + atoi(req[2])); 
	            printf("My first successor is now peer %s.\n", req[1]);
	            printf("My second successor is now peer %s.\n", req[2]);             
            } else if (!strcmp(targs->args[2], &req[0][1])) {
                strcpy(tack, "ack2");
              	succ2addr.sin_family = AF_INET;
        	    succ2addr.sin_addr.s_addr = INADDR_ANY;
	            succ2addr.sin_port = htons(50000 + atoi(req[1])); 
	            printf("My first successor is now peer %s.\n", targs->args[1]);
	            printf("My second successor is now peer %s.\n", req[1]); 
            }
            term.sin_family = AF_INET;
	        term.sin_addr.s_addr = INADDR_ANY;
	        term.sin_port = htons(50000 + atoi(&req[0][1]));
		    if ((filesock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
		        printf("Could not connect to socket\n");
		    if (connect(filesock, (struct sockaddr *) &term, sizeof(term)) < 0)
		        printf("Could not connect to requestor\n");
		    if (send(filesock, tack, sizeof(tack), 0) < 0)
		        printf("Could not forward message to meh requestor\n");
	        close(filesock);
	        close(commsock);
	        continue;
        }
        // Initial check if the peer is receiving a response for a file they requested
        // Syntax for file response is response\npeerno(of holder)\nfilename
        if (digits_only(req[0]) == 0) {
            printf("Received a response message from peer %s, which has the file %s.\n", req[1], req[2]);
            printf("We now start receiving the file .......\n");
            
            // Receive the file via UDP connection  
            filereceive(filesock, targs->args, targs->timer, atoi(req[1]));
            close(commsock);
            continue;
        }
		// If the peer contains the file (as per hash function)
		if (hasfile(atoi(req[1]), atoi(req[2]), atoi(targs->args[0]))) {
		
		// Send a response message notifying the peer that you have the file
		requestor.sin_family = AF_INET;
	    requestor.sin_addr.s_addr = INADDR_ANY;
	    requestor.sin_port = htons(50000 + atoi(req[0]));
	    
	    strcpy(found, "response\n");
		strcat(found, targs->args[0]); // peer no of of holder
		strcat(found,"\n");
		strcat(found, req[1]); // filename
		strcat(found, "\n");
		
		printf("File %s is here\n", req[1]);
		if ((filesock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
		    printf("Could not connect to socket\n");
		if (connect(filesock, (struct sockaddr *) &requestor, sizeof(requestor)) < 0)
		    printf("Could not connect to requestor\n");
		if (send(filesock, found, sizeof(found), 0) < 0)
		    printf("Could not forward message to meh requestor\n");
		printf("A response message, destined for peer %s, has been sent.\n",req[0]);
		close(filesock);
		printf("We now start sending the file .....\n");
   		// Send file over UDP with stop-wait mechanism
        filesend(filesock, req[1], atoi(req[0]), targs->args, targs->timer);
        
        close(commsock);
		} else {
		printf("File %s is not stored here.\n", req[1]);
		// Update file request
		strcpy(buf,req[0]);
        strcat(buf,"\n");
        strcat(buf,req[1]);
        strcat(buf,"\n");
        strcat(buf,targs->args[0]); // change peerno(of sender) section of request
        strcat(buf,"\n");
		
		// Send file request message to successor
		if ((succsock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		    printf("Unable to connect\n");
		    exit(EXIT_FAILURE);
	    }
		if (connect(succsock, (struct sockaddr *)&succ1addr, sizeof(succ1addr)) < 0) {
		    printf("Could not connect to successor \n");
		    exit(EXIT_FAILURE);
	    }
		if (send(succsock, buf, sizeof(buf), 0) < 0)
		    printf("Could not forward message to meh successor\n");
        printf("File request message has been forwarded to my successor.\n");
        close(succsock);
		}
		close(commsock); 
    }
    
}
 
int main (int argc, char **argv) {	
    // start the timer 
    clock_t timer = clock();
    pthread_t udp, tcp, ping, filereq;   
    struct udp_args uargs;
    struct ping_args pargs;
    struct file_args fargs;
    struct tcp_args targs;

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&succ1addr, 0, sizeof(succ1addr));
    memset(&succ2addr, 0, sizeof(succ2addr));
    memset(&pred1, 0, sizeof(pred1));
    memset(&pred2, 0, sizeof(pred2));
    
    servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(50000 + atoi(argv[1]));
	
	succ1addr.sin_family = AF_INET;
	succ1addr.sin_addr.s_addr = INADDR_ANY;
	succ1addr.sin_port = htons(50000 + atoi(argv[2]));
	
	succ2addr.sin_family = AF_INET;
	succ2addr.sin_addr.s_addr = INADDR_ANY;
	succ2addr.sin_port = htons(50000 + atoi(argv[3])); 
	
    strcpy(uargs.args[0],argv[1]);
    strcpy(uargs.args[1],argv[2]);
    strcpy(uargs.args[2],argv[3]);
    //uargs.servaddr = servaddr;
    //uargs.succ1addr = succ1addr;
    //uargs.succ2addr = succ2addr;
     
    strcpy(pargs.args[0],argv[1]);
    strcpy(pargs.args[1],argv[2]);
    strcpy(pargs.args[2],argv[3]);
    //pargs.succ1addr = succ1addr;
    //pargs.succ2addr = succ2addr;
    
    strcpy(fargs.args[0],argv[1]);     
    strcpy(fargs.args[1],argv[2]);
    strcpy(fargs.args[2],argv[3]);
    //fargs.succ1addr = succ1addr;
    
    strcpy(targs.args[0],argv[1]);
    strcpy(targs.args[1],argv[2]);
    strcpy(targs.args[2],argv[3]);
    strcpy(targs.args[3],argv[4]);
    strcpy(targs.args[4],argv[5]);
    targs.timer = timer;
    //targs.servaddr = servaddr;
    //targs.succ1addr = succ1addr;
    
    pthread_create(&ping, NULL, &pinger, (void*)&pargs);
    pthread_create(&udp, NULL, &userv, (void *)&uargs);
    pthread_create(&tcp, NULL, &tserv, (void *)&targs);
    pthread_create(&filereq, NULL, &filerequest, (void *)&fargs);
 
 
    pthread_exit(NULL);


	return 0;
}
