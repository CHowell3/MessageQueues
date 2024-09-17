#include <stdlib.h>
#include <stdio.h>
#include <mqueue.h>
#include <string.h>
#include <errno.h>
#include "common.h"

// Function that is called to output a stderr error message
void fail(const char *message) {
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
}

// Main function that checks error conditions opens message queues, parses arguments, and sends message to server
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fail("Usage: ./client clockwise|counter r c /show]");
    }

    // Open the server's message queue
    mqd_t serverQueue = mq_open(SERVER_QUEUE, O_WRONLY);
    if (serverQueue == -1) {
        fail("Unable to open server queue");
    }

    // Open the client's message queue to receive responses
    mqd_t clientQueue = mq_open(CLIENT_QUEUE, O_RDONLY);
    if (clientQueue == -1) {
        fail("Unable to open client queue");
    }

    // Buffer to send messages and receive responses
    char message[MESSAGE_LIMIT];
    memset(message, 0, sizeof(message));
    int commandType = 0;

    // Parse command-line arguments
    if (strcmp(argv[1], "clockwise") == 0 && argc == 4) {
        snprintf(message, MESSAGE_LIMIT, "clockwise %s %s", argv[2], argv[3]);
        commandType = 1;
    } 
    else if (strcmp(argv[1], "counter") == 0 && argc == 4) {
        snprintf(message, MESSAGE_LIMIT, "counter %s %s", argv[2], argv[3]);
        commandType = 1;
    } 
    else if (strcmp(argv[1], "show") == 0 && argc == 2) {
        strcpy(message, "show");
        commandType = 2;
    } 
    else {
        fail("error");
        exit(EXIT_FAILURE);
    }

    // Send message to the server
    if (mq_send(serverQueue, message, strlen(message) + 1, 0) == -1) {
        fail("Failed to send message to server");
    }

    char response[MESSAGE_LIMIT];
    ssize_t bytes_received = mq_receive(clientQueue, response, MESSAGE_LIMIT, NULL);
    if (bytes_received == -1) {
        fail("Failed to receive response from server");
    }

    // Ensure null-termination
    if (bytes_received >= MESSAGE_LIMIT) {
        response[MESSAGE_LIMIT - 1] = '\0';
    } else {
        response[bytes_received] = '\0';
    }

    // Handle server's response
    if (commandType == 1) {
        // For rotate commands
        if (strcmp(response, "OK") == 0 || strcmp(response, "solved") == 0) {
            printf("%s\n", response);
        } else {
            // If the response is "error", print it and exit unsuccessfully
            printf("error\n");
            exit(EXIT_FAILURE);
        }
    } else if (commandType == 2) {
        // For show command, print the puzzle state as is
        printf("%s", response);
    }

    // Close message queues
    mq_close(serverQueue);
    mq_close(clientQueue);

    return 0;
}