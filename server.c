#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include "common.h"

#define MIN_SIZE 2
#define MAX_SIZE 10

int **puzzle; 
int n;
int m;
char message[MESSAGE_LIMIT];
char response[MESSAGE_LIMIT];

// Print out an error message and exit.
static void fail( char const *message ) {
  fprintf( stderr, "%s\n", message );
  exit( 1 );
}

// Flag for telling the server to stop running because of a sigint.
// This is safer than trying to print in the signal handler.
static volatile int running = 1;

// Registers a signal handler so that when the user kills the server, the server catches the signal
void sigint_handler(int sig) {
    running = 0;
}

// Rotates the given 2x2 matrix provided by r and c clockwise
void rotate_clockwise(int r, int c) {
    if (r < 0 || r > n - 2 || c < 0 || c > m - 2) {
        snprintf(response, MESSAGE_LIMIT, "error");
        return;
    }
    int temp = puzzle[r][c];
    puzzle[r][c] = puzzle[r+1][c];
    puzzle[r+1][c] = puzzle[r+1][c+1];
    puzzle[r+1][c+1] = puzzle[r][c+1];
    puzzle[r][c+1] = temp;
    snprintf(response, MESSAGE_LIMIT, "OK");
}

// Rotates the given 2x2 matrix provided by r and c counterclockwise
void rotate_counter_clockwise(int r, int c) {
    if (r < 0 || r > n - 2 || c < 0 || c > m - 2) {
        snprintf(response, MESSAGE_LIMIT, "error");
        return;
    }
    int temp = puzzle[r][c];
    puzzle[r][c] = puzzle[r][c+1];
    puzzle[r][c+1] = puzzle[r+1][c+1];
    puzzle[r+1][c+1] = puzzle[r+1][c];
    puzzle[r+1][c] = temp;
    snprintf(response, MESSAGE_LIMIT, "OK");
}

// Prints the current state of the puzzle with the correct formatting
void print_puzzle_state(char *response) {
    int offset = 0;
    memset(response, 0, MESSAGE_LIMIT);
    int written = snprintf(response + offset, MESSAGE_LIMIT - offset, "+");
    if (written < 0 || written >= MESSAGE_LIMIT - offset) return;
    offset += written;

    for (int j = 0; j < m; j++) {
        written = snprintf(response + offset, MESSAGE_LIMIT - offset, "---+");
        if (written < 0 || written >= MESSAGE_LIMIT - offset) return;
        offset += written;
    }
    written = snprintf(response + offset, MESSAGE_LIMIT - offset, "\n");
    if (written < 0 || written >= MESSAGE_LIMIT - offset) return;
    offset += written;

    for (int i = 0; i < n; i++) {
        written = snprintf(response + offset, MESSAGE_LIMIT - offset, "|");
        if (written < 0 || written >= MESSAGE_LIMIT - offset) return;
        offset += written;

        for (int j = 0; j < m; j++) {
            written = snprintf(response + offset, MESSAGE_LIMIT - offset, "%3d|", puzzle[i][j]);
            if (written < 0 || written >= MESSAGE_LIMIT - offset) return;
            offset += written;
        }
        written = snprintf(response + offset, MESSAGE_LIMIT - offset, "\n");
        if (written < 0 || written >= MESSAGE_LIMIT - offset) return;
        offset += written;

        written = snprintf(response + offset, MESSAGE_LIMIT - offset, "+");
        if (written < 0 || written >= MESSAGE_LIMIT - offset) return;
        offset += written;

        for (int j = 0; j < m; j++) {
            written = snprintf(response + offset, MESSAGE_LIMIT - offset, "---+");
            if (written < 0 || written >= MESSAGE_LIMIT - offset) return;
            offset += written;
        }
        written = snprintf(response + offset, MESSAGE_LIMIT - offset, "\n");
        if (written < 0 || written >= MESSAGE_LIMIT - offset) return;
        offset += written;
    }
}

// Function seperate from print_puzzle _state to be called by the server
// Does not need to take parameters like its brother function due to there being no communication needed between the client and server
void print_puzzle_state_output() {
    printf("+");
    for (int j = 0; j < m; j++) {
        printf("---+");
    }
    printf("\n");

    for (int i = 0; i < n; i++) {
        printf("|");
        for (int j = 0; j < m; j++) {
            printf("%3d|", puzzle[i][j]);
        }
        printf("\n");
        printf("+");
        for (int j = 0; j < m; j++) {
            printf("---+");
        }
        printf("\n");
    }
}

// Checks if the values in the puzzle input file are valid. Returns true if they are and false otherwise
// Allocates memory for different variables, iterating through and checking if the specific instance is valid or invalid. Returns accordingly
bool validate_input(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        return false;
    }
    if (fscanf(file, "%d %d", &n, &m) != 2 || n < MIN_SIZE || n > MAX_SIZE || m < MIN_SIZE || m > MAX_SIZE) {
        fclose(file);
        return false;
    }
    int total_tiles = n * m;
    int *tiles = malloc(total_tiles * sizeof(int));
    if (!tiles) {
        fclose(file);
        return false;
    }
    int i = 0;
    while (i < total_tiles && fscanf(file, "%d", &tiles[i]) == 1) {
        if (tiles[i] < 1 || tiles[i] > total_tiles) {
            free(tiles);
            fclose(file);
            return false;
        }
        i++;
    }
    if (i != total_tiles) {
        free(tiles);
        fclose(file);
        return false;
    }
    bool *viewed = calloc(total_tiles, sizeof(bool));
    if (!viewed) {
        free(tiles);
        fclose(file);
        return false;
    }
    for (i = 0; i < total_tiles; i++) {
        if (viewed[tiles[i] - 1]) {
            free(viewed);
            free(tiles);
            fclose(file);
            return false;
        }
        viewed[tiles[i] - 1] = true;
    }
    puzzle = malloc(n * sizeof(int *));
    if (!puzzle) {
        free(viewed);
        free(tiles);
        fclose(file);
        return false;
    }
    for (i = 0; i < n; i++) {
        puzzle[i] = malloc(m * sizeof(int));
        if (!puzzle[i]) {
            for (int k = 0; k < i; k++) {
                free(puzzle[k]);
            }
            free(puzzle);
            free(viewed);
            free(tiles);
            fclose(file);
            return false;
        }
    }
    int idx = 0;
    for (int row = 0; row < n; row++) {
        for (int col = 0; col < m; col++) {
            puzzle[row][col] = tiles[idx++];
        }
    }
    free(viewed);
    free(tiles);
    fclose(file);
    return true;
}

// Iterates throught the puzzle values and determines if the puzzle is solved by ensuring the value preceding each observed is less than
// Returns false if not solved and true otherwise
bool is_puzzle_solved() {
    int correct = 1;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < m; j++) {
            if (puzzle[i][j] != correct++) {
                return false;
            }
        }
    }
    return true;
}

// Critical function called in main that determines which command was sent from the clientQueue to the serverQueue
// Calls functions based on the specific command. Prints error if invalid command
void process_command(const char *command) {
    char cmd[100];
    int r;
    int c;
    if (sscanf(command, "%s %d %d", cmd, &r, &c) == 3) {
        if (strcmp(cmd, "clockwise") == 0) {
            rotate_clockwise(r, c);
            if (is_puzzle_solved()) {
                snprintf(response, MESSAGE_LIMIT, "solved");
            }
        } else if (strcmp(cmd, "counter") == 0) {
            rotate_counter_clockwise(r, c);
            if (is_puzzle_solved()) {
                snprintf(response, MESSAGE_LIMIT, "solved");
            }
        } else {
            snprintf(response, MESSAGE_LIMIT, "error");
        }
    } else if (strcmp(command, "show") == 0) {
        print_puzzle_state(response);
    } else {
        snprintf(response, MESSAGE_LIMIT, "error");
    }
}

// Main function that does error checking, sets up signal handler, opens/closes/unlinks queues, and receives the message sent by the clientQueue, processing the command accordingly
int main( int argc, char *argv[] ) {
    // Ensure correct number of arguments
    if (argc != 2) {
        fail("usage: server PUZZLE-FILE");
    }

    const char *filename = argv[1];
    if (!validate_input(filename)) {
        fprintf(stderr, "Invalid input file: %s\n", filename);
        return 1;
    }

    // Set up signal handler. Referred to StackOverflow and manual pages for information on how to handle the signal
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = sigint_handler;
    sigaction(SIGINT, &action, NULL);

    // Remove both queues, in case, last time, this program terminated
    // abnormally with some queued messages still queued.
    mq_unlink( SERVER_QUEUE );
    mq_unlink( CLIENT_QUEUE );

    // Prepare structure indicating maximum queue and message sizes.
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 1;
    attr.mq_msgsize = MESSAGE_LIMIT;

  // Make both the server and client message queues.
    mqd_t serverQueue = mq_open( SERVER_QUEUE, O_RDONLY | O_CREAT, 0600, &attr );
    mqd_t clientQueue = mq_open( CLIENT_QUEUE, O_WRONLY | O_CREAT, 0600, &attr );
    if ( serverQueue == -1 || clientQueue == -1 )
        fail( "Can't create the needed message queues" );

    // Repeatedly read and process client messages.
    while (running) {
        ssize_t bytes_received = mq_receive(serverQueue, message, MESSAGE_LIMIT, NULL);
        if (bytes_received == -1) {
            if (errno == EINTR) {
                continue;
            }
            mq_close(serverQueue);
            mq_close(clientQueue);
            fail("Failed to receive message from client");
        } else if (bytes_received < MESSAGE_LIMIT) {
            message[bytes_received] = '\0';
        }
        process_command(message);

        if (mq_send(clientQueue, response, strlen(response) + 1, 0) == -1) {
            mq_close(serverQueue);
            mq_close(clientQueue);
            fail("Failed to send response to client");
        }
    }

    printf("\n");
    print_puzzle_state_output();

    // Close our two message queues (and delete them).
    mq_close(clientQueue);
    mq_close(serverQueue);
    mq_unlink(SERVER_QUEUE);
    mq_unlink(CLIENT_QUEUE);

    return 0;
}
