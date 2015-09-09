/*
 * WatchDog
 * Authors: Ryan Smith, Brad Thompson, Theresa Brenier
 * Version: April, 2015 
 * Citations:
 * Fundamental server operations were borrowed from the following 2 sources:
 * 1 - http://www.prasannatech.net/2008/07/socket-programming-tutorial.html
 * 2 - http://www.binarii.com/files/papers/c_sockets.txt
 * This is mostly found in function server_thread between lines 320-355
 * and in function main between lines 498-503
 */

#include <sys/types.h>
#include <sys/socket.h>
#include "server_info.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>


char tempBuilder[1000];
double temps[3600];
int nextTempPointer;
int fd;
int quit_signal = 0;
int standby = 0;
pthread_mutex_t lock;
char cOrF = 'c';
char celciusCount = 0;
char standbyCount = 0;
bool standbyActive = false;
bool arduinoError = false;
bool tripped = false;

/*
 * Finds the highest temperature in the array of recorded temps and returns it.
 */
double findMax() {
      double max = -300.0;
      for (int i = 0; i < 3600; i++) {
            if (temps[i] > 200.0) continue;
            if (temps[i] > max) max = temps[i];
      }
      return max;
}

/*
 * Finds the lowest temperature in the array of recorded temps and returns it.
 */
double findMin() {
      double min = 500.0;
      for (int i = 0; i < 3600; i++) {
            if (temps[i] < -200.0) continue;
            if (temps[i] < min) min = temps[i];
      }
      return min;
}

/*
 * Calculates the average of all the recorded temperatures and returns it.
 */
double findAverage() {
      double sum = 0;
      int count = 0;
      for (int i = 0; i < 3600; i++) {
            if (temps[i] > 200.0 || temps[i] < -200.0) continue;
            sum += temps[i];
            count++;
      }
      if (count == 0) return -274.0;
      return sum / count;
}

/*
 * Packages a JSON containing max, min and average temperatures and returns it for sending.
 * Returns "No data available." when temperature array is empty of there has been a problem receiving data.
 * Returns "Arudino Error!!!" if the Arudino is currently in an error state (such as when it is disconnected after starting).
 */
char* packageAvgJSON(){
      char* latest_temp = (char*) malloc(1000);
      if (latest_temp == NULL) {
            printf("\nAn error occurred while allocating memory for your request. Please try again.\n");
            return NULL;
      }
      if (arduinoError){
            sprintf(latest_temp, "{\n\"name\":\"Arudino Error!!!\"\n}\n");
      }
      else {
            if (nextTempPointer == 0 ){
                  if (temps[3599] == -274.0){
                        sprintf(latest_temp, "{\n\"name\":\"No data available.\"\n}\n");
                        return latest_temp;
                  }
            }
            else {
                  if (temps[nextTempPointer - 1] == -274.0){
                        sprintf(latest_temp, "{\n\"name\":\"No data available.\"\n}\n");
                        return latest_temp;
                  }     
            }
            double max = findMax();
            double min = findMin();
            double average = findAverage();
            printf("%f\n%f\n%f\n\n", max, min, average);
            if (cOrF == 'F') {
                  max = max * 9 / 5 + 32;
                  min = min * 9 / 5 + 32;
                  average = average * 9 / 5 + 32;
            }
            sprintf(latest_temp, "{\n\"name\":\"H: %.1f L: %.1f AVG: %.1f\"\n}\n", max, min, average);
      }
      return latest_temp;
}

/*
 * Packages a JSON containing the most recent temperature reading and returns it for sending.
 * Returns "No data available." when temperature array is empty of there has been a problem receiving it.
 * Returns "Arudino Error!!!" if the Arudino is currently in an error state (such as when it is disconnected after starting).
 */
char* packageTempJSON(){
      char* latest_temp = (char*) malloc(1000);
      if (latest_temp == NULL) {
            printf("\nAn error occurred while allocating memory for your request. Please try again.\n");
            return NULL;
      }
      if (arduinoError){
            sprintf(latest_temp, "{\n\"name\":\"Arudino Error!!!\"\n}\n");
      }
      else if (nextTempPointer == 0) {
            if (temps[3599] == -274.0){
                  sprintf(latest_temp, "{\n\"name\":\"No data available.\"\n}\n");

            }
            else{
                  double convert = temps[3599];
                  if (cOrF == 'F') {
                        convert = temps[3599] * 9 / 5 + 32;
                  }
                  sprintf(latest_temp, "{\n\"name\":\"%.1f %c\"\n}\n", convert, cOrF);
            }
      }
      else {
            if (temps[nextTempPointer - 1] == -274.0){
                  //sprintf(latest_temp, "{\n\"name\":\"No data available.\"\n}\n");
                  sprintf(latest_temp, "{\n\"name\": \"No data available.\"}\n");
            }
            else{
                  double convert = temps[nextTempPointer - 1];
                  if (cOrF == 'F') {
                        convert = temps[nextTempPointer - 1] * 9 / 5 + 32;
                  }
                  sprintf(latest_temp, "{\n\"name\":\"%.1f %c\"\n}\n", convert, cOrF);
            }
      }
      return latest_temp;
}

/*
 * Sends the most recent temperature reading to the Pebble in JSON format.
 */
void mostRecentTemp(int fd2){
      pthread_mutex_lock(&lock);
      char* latest_temp = packageTempJSON();
      pthread_mutex_unlock(&lock);
      //send the package JSON if it's not NULL
      if (!(latest_temp == NULL)){
            int completion_value = send(fd2, latest_temp, strlen(latest_temp), 0);
            if (completion_value < 0){
                  printf("Server failed to send message.");
            }
            free(latest_temp);
      }
}

/*
 * Tells the Arduino to change the display from c to F or vice versa. 
 * Sends the most recent temperature reading in the new style to the Pebble in JSON format.
 * (Does not change the format of temperatures being transmitted Arduino->Server, just processes differently for display on both sides.)
 */
void changeSign(int fd2){
      if (celciusCount % 3 == 0){
            write(fd, "f", 1);
            if (cOrF == 'c')
                  cOrF = 'F';
            else 
                  cOrF = 'c';
            //send temp in new format
            pthread_mutex_lock(&lock);
            char* latest_temp = packageTempJSON();
            pthread_mutex_unlock(&lock);
            //send packaged JSON if not NULL
            if (!(latest_temp == NULL)){
                  int completion_value = send(fd2, latest_temp, strlen(latest_temp), 0);
                  if (completion_value < 0){
                        printf("Server failed to send message.");
                  }
                  free(latest_temp);
            }
      }
      celciusCount += 1;
}

/*
 *Tells the Arduino to go into or come out of standby mode.
 */
void toggleStandby(int fd2){
      //*Note - UP button on Pebble sends request 3 times - mod processes once per click
      if (standbyCount % 3 == 0){
            //tell the Arduino to enter/leave standby
            write(fd, "s", 1);  
            //malloc space for a string and check that space was available
            char* message = (char*) malloc(1000);
            if (message == NULL) {
                  printf("\nAn error occurred while allocating memory for your request. Please try again.\n");
                  return;
            }
            //toggle standbyActive and fill string with appropriate message
            standbyActive = !standbyActive;
            if (standbyActive){
                  sprintf(message, "{\n\"name\":\"Standby engaged.\"\n}\n");
            }
            else {
                  sprintf(message, "{\n\"name\":\"Standby disengaged.\"\n}\n");    
            } 
            //send message to Pebble and check that it sent
            int completion_value = send(fd2, message, strlen(message), 0);
            if (completion_value < 0){
                  printf("Server failed to send message.");
            }
      }
      standbyCount++;
}

/*
 * Sends the max, min and average temperature readings to the Pebble in JSON format.
 */
void highLowAverage(int fd2){
      pthread_mutex_lock(&lock);
      char* latest_temp = packageAvgJSON();
      pthread_mutex_unlock(&lock);
      //send packaged JSON if not NULL
      if (!(latest_temp == NULL)){
            int completion_value = send(fd2, latest_temp, strlen(latest_temp), 0);
            if (completion_value < 0){
                  printf("Server failed to send message.");
            }
            free(latest_temp);
      }
}

/*
 * Checks to see if the motion sensor has been trip. Sends message to Pebble in JSON format indicating T/F.
 */
void checkTripped(int fd2){  
      char* message = (char*) malloc(1000);
      if (message == NULL) {
            printf("\nAn error occurred while allocating memory for your request. Please try again.\n");
            return;
      }
      if (tripped){
            sprintf(message, "{\n\"name\":\"tripped\"\n}\n");    
      }
      else {
            sprintf(message, "{\n\"name\":\"nottripped\"\n}\n"); 
      }
      int completion_value = send(fd2, message, strlen(message), 0);
      if (completion_value < 0){
            printf("Server failed to send message.");
      }
      free(message);
}

/*
 * Asks the Arduino to display the warning message on the 7-Seg.
 */
void requestMessage(int fd2){
      write(fd, "m", 1);
      char* message = (char*) malloc(1000);
      if (message == NULL) {
            printf("\nAn error occurred while allocating memory for your request. Please try again.\n");
            return;
      }
      sprintf(message, "{\n\"name\":\"Message Sent\"\n}\n"); 
      int completion_value = send(fd2, message, strlen(message), 0);
      if (completion_value < 0){
            printf("Server failed to send message.");
      }
      free(message);
}

/*
 * Sets var tripped back to false in both the server and the Arduino.
 */
void resetAlarm(int fd2){
      tripped = false;
      write(fd, "r", 1);
      char* message = (char*) malloc(1000);
      if (message == NULL) {
            printf("\nAn error occurred while allocating memory for your request. Please try again.\n");
            return;
      }
      sprintf(message, "{\n\"name\":\"Alarm Reset\"\n}\n"); 
      int completion_value = send(fd2, message, strlen(message), 0);
      if (completion_value < 0){
            printf("Server failed to send message.");
      }
      free(message);
}

/*
Configures server and loops, processing connections 1 by 1 until quit_signal is received
*/
void* server_thread(void* p){
//Server config implementation
      //get info regarding server config details from Main method
      server_info* info = (server_info*)p;
      int PORT_NUMBER = info->port_num;
      // structs to represent the server and client
      struct sockaddr_in server_addr,client_addr;
      int sock; // socket descriptor
      // 1. socket: creates a socket descriptor that you later use to make other system calls
      if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
      perror("Socket");
      exit(1);
      }
      int temp;
      if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&temp,sizeof(int)) == -1) {
      perror("Setsockopt");
      exit(1);
      }
      // configure the server
      server_addr.sin_port = htons(PORT_NUMBER); // specify port number
      server_addr.sin_family = AF_INET;
      server_addr.sin_addr.s_addr = INADDR_ANY;
      bzero(&(server_addr.sin_zero),8);
      // 2. bind: use the socket and associate it with the port number
      if (bind(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
      perror("Unable to bind");
      exit(1);
      }
      // 3. listen: indicates that we want to listen to the port to which we bound; second arg is number of allowed connections
      if (listen(sock, 1) == -1) {
      perror("Listen");
      exit(1);
      }
      // once you get here, the server is set up and about to start listening
      printf("\nServer configured to listen on port %d\n", PORT_NUMBER);
      fflush(stdout);

//Request Processing
      //loops, waiting for requests until quit_signal is activated
      while (quit_signal == 0){
            // 4. accept: wait here until we get a connection on that port
            int sin_size = sizeof(struct sockaddr_in);
            int fd2 = accept(sock, (struct sockaddr *)&client_addr,(socklen_t *)&sin_size);
            printf("Server got a connection from (%s, %d)\n", inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));

            // buffer to read data into
            char request[1024];
            // 5. recv: read incoming request message into buffer
            int bytes_received = recv(fd2,request,1024,0);
            request[bytes_received] = '\0';
            printf("%s\n", request);
            //this is a request for the most recent temperature 
            switch (request[5]){
                  case 'a':
                        changeSign(fd2);
                        break;
                  case 'b':
                        mostRecentTemp(fd2);
                        break;
                  case 'd':
                        highLowAverage(fd2); 
                        break;
                  case 'm':
                        requestMessage(fd2);
                        break;
                  case 'r':
                        resetAlarm(fd2);
                        break;
                  case 's':
                        toggleStandby(fd2);
                        break;
                  case 't':
                        checkTripped(fd2);
                        printf("%s\n\n", "in t");
                        break;
            }  
            close(fd2);
      }
      // 7. close: close the socket connection
      close(sock);
      printf("Server closed connection\n");
      return 0;
}

/*
Waits for input from user. If input is Q or q, updates quit_signal.
*/
void* input_thread (void* p){
      //reads input into char array to avoid errors if extra keys are hit accidentally
      char user_input[100];
      while (user_input[0] != 'q' && user_input[0] != 'Q'){
            scanf("%s", user_input);      
      }
      quit_signal = 1;
      return NULL;
}

/*
 *Reads temp from Arduino and saves values in array.
 */
void* storeData(void* p) {
      //setup new array of temps
      for (int i = 0; i < 3600; i++) {
            pthread_mutex_lock(&lock);
            temps[i] = -274.0;
            pthread_mutex_unlock(&lock);
      }
      //for tracking where to insert next
      pthread_mutex_lock(&lock);
      nextTempPointer = 0;
      pthread_mutex_unlock(&lock);
      
      char string[1000];
      while(quit_signal == 0){
            char buf[1000];
            int bytes_read = read(fd, buf, 1000);
            if (bytes_read == -1)
                  arduinoError = true;
            else
                  arduinoError = false;
            int i;
            for(i = 0; i < bytes_read; i++){
                  if (buf[i] == '\n'){
                        char null = '\0';
                        //appends null character to complete working string
                        strcat(string, &null);
                        //checks to see if the word received is "tripped" notifying us of motion sensor
                        if (strncmp(string, "tripped", 7) == 0){
                              tripped = true;
                              printf("%s\n\n", "trip noticed");
                        }
                        //if not, adds the temp value into the array
                        else { 
                              pthread_mutex_lock(&lock);
                              temps[nextTempPointer] = atof(string);
                              nextTempPointer += 1;
                              pthread_mutex_unlock(&lock);
                        }
                        //clear working string to begin again
                        string[0] = null;
                        continue;   
                  }
                  //simply adds individual character to string while '\n' has not been encountered
                  char this_char = buf[i];
                  strncat(string, &this_char, 1);     
            }
        }
        return NULL;
}

/*
 * Processes args, starts threads and executes program.
 */
int main(int argc, char *argv[])
{
//setting up server	
      // check the number of arguments
	if (argc != 2){
		printf("\nPlease enter the proper number of arguments when executing.\n");
		exit(0);
	}
      //package the arguments into a server_info struct and pass to server thread
      server_info* start_info = (server_info*)malloc(sizeof(server_info));
      if (start_info == NULL)
            printf("\nAn error occurred while allocating memory for your request. Please try again.\n");
      start_info->port_num = atoi(argv[1]);

//setting up arudino connection
      pthread_mutex_init(&lock, NULL);

      //establish connection w/Arduino
      fd = open("/dev/cu.usbmodem1451", O_RDWR);
      if(fd == -1){ // couldn't open
        printf("Couldn't establish a connection with Arduino.\n");
        return 0;
      }

      //configure connection
      struct termios options;
      tcgetattr(fd, &options);
      cfsetispeed(&options, 9600); //how fast to receive
      cfsetospeed(&options, 9600); //how fast to send
      tcsetattr(fd, TCSANOW, &options);


//create and join threads
      pthread_t thread1, thread2, thread3;
      pthread_create(&thread1, NULL, &server_thread, (void*)start_info);
      pthread_create(&thread2, NULL, &input_thread, NULL);
      //create threads and attach to functions
      pthread_create(&thread3, NULL, &storeData, NULL);

      pthread_join(thread1, NULL);
      pthread_join(thread2, NULL);
      pthread_join(thread3, NULL);

//TERMINATION
      //free the server_info package once server has terminated execution
      free(start_info);
      //close
      close(fd);
      return 1;

}

