#include "listener_socket.h"
#include "connection.h"
#include "response.h"
#include "request.h"
#include "iowrapper.h"
#include "protocol.h"
#include "queue.h"
#include "rwlock.h"

#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//-----------------------------------------------------------------------------------------------------------------
//                                                   STRUCTS
//-----------------------------------------------------------------------------------------------------------------

struct slot {
    char* uri;
    rwlock_t* lock;
    int num_workers;
};

struct thread_arguments {
    int tid;
    int num_threads;
};

typedef struct slot slot_t;
typedef struct thread_arguments thread_arguments_t;

//-----------------------------------------------------------------------------------------------------------------
//                                              GLOBAL VARIABLES
//-----------------------------------------------------------------------------------------------------------------

queue_t* queue_global;
pthread_mutex_t mutex_global;
slot_t** worker_slots;
char** hash_global;

//-----------------------------------------------------------------------------------------------------------------
//                                        HELPER FUNCTIONS DECLARATIONS
//-----------------------------------------------------------------------------------------------------------------

void handle_connection(conn_t*, rwlock_t*, const Response_t*, bool* );

void handle_get(conn_t*, rwlock_t* );
void handle_put(conn_t*, rwlock_t*, bool* );
void handle_unsupported(conn_t* );
void handle_bad_request(conn_t*/*, const Response_t**/ );

bool check_validity (conn_t*, bool* );

char* get_threads(int, char** );
int check_args(int, char**, char*, int );
size_t get_port(char**, int );

void audit_log(conn_t*, uint16_t, char* );

void* worker_exec(void* );

slot_t** worker_slot_init (int );

slot_t* slot_create(char* );
void slot_leave(int );

void print_worker_slots(slot_t**, int );

//-----------------------------------------------------------------------------------------------------------------
//                                                    MAIN
//-----------------------------------------------------------------------------------------------------------------

int main (int argc, char** argv) {

    // process command line args

    char* threads_str = get_threads(argc, argv);
    int port = (int) get_port(argv, optind);
    int threads = check_args(argc, argv, threads_str, 4);

    // initialize listener socket
    Listener_Socket_t *sock = ls_new(port);
    if (!sock) {
      fprintf(stderr, "cannot open socket");
      exit(1);
    }

    // initialize global concurrent queue
    queue_global = queue_new(threads);

    //initialize mutex
    pthread_mutex_init(&mutex_global, NULL);

    // initialize slots for worker threads
    worker_slots = worker_slot_init(threads);

    // initialize hashtable
    hash_global = (char** )calloc(threads*2, sizeof(char* ));

    // create threads
    pthread_t thread_arr[threads];
    thread_arguments_t** args_arr = malloc(sizeof(thread_arguments_t* ) * threads);

    for (int i = 0; i < threads; i++) {
        thread_arguments_t* args = (thread_arguments_t* )malloc(sizeof(thread_arguments_t));
        args->tid = i;
        args->num_threads = threads;
        args_arr[i] = args;

        pthread_create(&thread_arr[i], NULL, worker_exec, args_arr[i]);
    }

    // listen
    while (1) {
        // accept new connection
        int connfd = ls_accept(sock);

        // push conn to the queue
        queue_push(queue_global, &connfd);

    }
    ls_delete(&sock);

    return 1;
}


//-----------------------------------------------------------------------------------------------------------------
//                                     HELPER FUNCTIONS IMPLEMENTATIONS
//-----------------------------------------------------------------------------------------------------------------

void* worker_exec(void* args) {

    thread_arguments_t* props = (thread_arguments_t* ) args;

    while (1) {
        // get connfd sent as void*
        void* c;
        queue_pop(queue_global, &c);
        int connfd = *(int *) c;

        // create new conneciton and parse it
        conn_t* conn = conn_new(connfd);
        const Response_t *res = conn_parse(conn);

        // only enter if statement if request is valid format
        if (res == NULL) {

            // get important request attributes
            char* uri = conn_get_uri(conn);

            bool existed = false; 
            bool valid = check_validity(conn, &existed);

            // initialize state variables and lock to be used
            int current_slot = -1;
            bool active_uri = false;
            rwlock_t* current_lock = rwlock_new(N_WAY, 1);

            if (valid) {
                // loop through worker_slots array
                for (int i = 0; i < props->num_threads; i++) {

                    // if uri is in an active slot we must wait
                    if (strcmp(worker_slots[i]->uri, uri) == 0 && strcmp(worker_slots[i]->uri, "\0") != 0) {

                        // acquire mutex
                        pthread_mutex_lock(&mutex_global);
                        //set state variables and current lock for operations
                        current_slot = i;
                        current_lock = worker_slots[current_slot]->lock;
                        worker_slots[current_slot]->num_workers += 1;
                        active_uri = true;
                        // release mutex
                        pthread_mutex_unlock(&mutex_global);

                        break;
                    }
                }

                // if the uri is not currently being accessed we can use a new slot
                if (active_uri == false) {
                    // acquire mutex

                    // iterate worker_slots
                    for (int i = 0; i < props->num_threads; i++) {

                        // check if the slot is open will have empty uri
                        if (strcmp(worker_slots[i]->uri, "\0") == 0) { 

                            // acquire mutex
                            pthread_mutex_lock(&mutex_global);
                            // create a new slot in the worker_slots array
                            worker_slots[i] = slot_create(uri);
                            worker_slots[i]->num_workers = 1;

                            // set current lock and current slot, break out of loop once finished
                            current_slot = i;
                            current_lock = worker_slots[i]->lock;
                            // release mutex
                            pthread_mutex_unlock(&mutex_global);
                            break;
                        }
                    }
                }
            }
            // handle connection based on if method is GET, PUT, or unsupported using the current lock
            // acquire the mutex
            handle_connection(conn, current_lock, res, &existed);
            if (valid) {
                slot_leave(current_slot);
            }
        }
        else {
            // handle bad request
            handle_bad_request(conn/*, res*/);
        }
        // close connection socket
        close(connfd);
    }

    return args;
}

//-----------------------------------------------------------------------------------------------------------------
slot_t** worker_slot_init (int num_threads){

    // allocate memory for the array of slots
    slot_t** worker_slots = (slot_t** )calloc(num_threads, sizeof(slot_t* ));

    // iterate through each worker slot
    for (int i = 0; i < num_threads; i++) {
        // create new slots
        slot_t* new_slot = slot_create("\0");
        worker_slots[i] = new_slot;
    }

    return worker_slots;
}

//-----------------------------------------------------------------------------------------------------------------
slot_t* slot_create(char* uri) {

    // allocate memory
    slot_t* new_slot = (slot_t *)malloc(sizeof(slot_t));
    new_slot->uri = (char* )malloc(strlen(uri) + 1);

    // set values
    strcpy(new_slot->uri, uri);
    new_slot->lock = rwlock_new(N_WAY, 1);
    new_slot->num_workers = 0;

    return new_slot;
}

//-----------------------------------------------------------------------------------------------------------------
void slot_leave(int index) {

    // for readability
    int n = index;


    // leave the slot located at index n of the worker_slots array (if it active)
    if (worker_slots[n]->num_workers > 0) {

        // acquire the mutex
        pthread_mutex_lock(&mutex_global);
        worker_slots[n]->num_workers -= 1;

        // if the slot is empty we must reset it
        if (worker_slots[n]->num_workers == 0) {

            rwlock_delete(&(worker_slots[n]->lock));
            worker_slots[n]->lock = NULL;

            strcpy(worker_slots[n]->uri, "\0");
        }

        // release the mutex
        pthread_mutex_unlock(&mutex_global);
    }
}

//-----------------------------------------------------------------------------------------------------------------
void audit_log(conn_t* conn, uint16_t response_code, char* request_type) {

    // get appropriate fields
    char* method = request_type;
    char* uri = conn_get_uri(conn);
    uint16_t code = response_code;
    char* id = conn_get_header(conn, "Request-Id");

    if (id == NULL) {
        id = "0";
    }

    // output appropriate audit log
    fprintf(stderr, "%s,/%s,%hu,%s\n", method, uri, code, id);
}

//-----------------------------------------------------------------------------------------------------------------
char* get_threads(int argc, char** argv) {

    // function gets threads argument
    int opt;
    char* threads_str = NULL;

    // check number of arguments
    if (argc < 2 || argc > 4) {
        fprintf(stderr, "wrong arguments: %s threads port_num\n", argv[0]);
        fprintf(stderr, "usage: %s [-t threads] <port>\n", argv[0]);
        exit(1);
    }

    // get opt to check -t flag
    while ((opt = getopt(argc, argv, "t:")) != -1) {
        switch(opt) {
            case 't':
                threads_str = optarg;
                break;
            default:
                fprintf(stderr, "usage: %s [-t threads] <port>\n", argv[0]);
                exit(1);
        }
    }

    // NULL or threads string
    return threads_str;
}

//-----------------------------------------------------------------------------------------------------------------
size_t get_port(char** argv, int optind) {

    // check validity of port number
    char *endptr = NULL;
    size_t port = (size_t) strtoull(argv[optind], &endptr, 10);
    if (endptr && *endptr != '\0') {
        fprintf(stderr, "invalid port number: %s", argv[1]);
        exit(1);
    }

    return port;
}

//-----------------------------------------------------------------------------------------------------------------
int check_args(int argc, char** argv, char* threads_str, int threads) {

    // function checks valid arguments, returns number of threads

    // change thread count if specified
    if (threads_str != NULL) {
        threads = atoi(threads_str);
    }

    // check number of arguments
    if (argc < 2 || argc > 4) {
        fprintf(stderr, "wrong arguments: %s threads port_num\n", argv[0]);
        fprintf(stderr, "usage: %s [-t threads] <port>\n", argv[0]);
        exit(1);
    }

    return threads;
}

//-----------------------------------------------------------------------------------------------------------------
bool check_validity (conn_t* conn, bool* existed) {
    // init
    char *uri = conn_get_uri(conn);
    const Request_t *req = conn_get_request(conn);
    int fd = 0;
    bool check = true;

    // check if file exists.  
    bool ex = (access(uri, F_OK)) == 0;
    (*existed) = ex;

    if (req == &REQUEST_PUT) {

        // open the file
        fd = open(uri, O_CREAT | O_TRUNC | O_WRONLY, 0600);

        // handle invalid PUT
        if (fd < 0) {
            check = false;
        }
        else {
            check = true;
        }
    }
    else if (req == &REQUEST_GET) {

        // open the file
        fd = open(uri, O_RDWR, 0600);

        // invalid GET
        if (fd < 0) {
            check = false;
        }
        //valid GET
        else {
            check = true;
        }
    }
    else {
        check = false;
    }

    close(fd);
    return check;
}

//-----------------------------------------------------------------------------------------------------------------
void handle_connection(conn_t* conn, rwlock_t* lock, const Response_t* res, bool* existed) {

    if (res != NULL) {
        conn_send_response(conn, res);
    } else {
        const Request_t *req = conn_get_request(conn);
        if (req == &REQUEST_PUT) {
            writer_lock(lock);
            handle_put(conn, lock, existed);
            writer_unlock(lock);
        }
        else if (req == &REQUEST_GET) {
            reader_lock(lock);
            handle_get(conn, lock);
            reader_unlock(lock);
        }
        else if (req == &REQUEST_UNSUPPORTED) {
            handle_unsupported(conn);
        }
    }

    conn_delete(&conn);
}

//-----------------------------------------------------------------------------------------------------------------
void handle_get(conn_t* conn, rwlock_t* lock) {
    // // init
    const Response_t* res = NULL;
    char *uri = conn_get_uri(conn);

    // check if file exists.  
    bool existed = (access(uri, F_OK)) == 0;

    // open the file
    int fd = open(uri, O_RDWR, 0600);

    // handle invalid GET
    if (fd < 0) {
        if (existed || errno == EISDIR) {
            res = &RESPONSE_FORBIDDEN;
        }
        else if (errno == ENOENT) {
            res = &RESPONSE_NOT_FOUND;
        }
        else {
            res = &RESPONSE_INTERNAL_SERVER_ERROR;
        }
    }
    
    // handle valid GET

    // stat struct used to calculate file size  
    struct stat fstat;
    stat(uri, &fstat);

    reader_lock(lock)
    res = conn_send_file(conn, fd, fstat.st_size);

    if (res == NULL && existed) {
        res = &RESPONSE_OK;
    }
    audit_log(conn, response_get_code(res), "GET");
    reader_unlock(lock);
    
    close(fd);
}

//-----------------------------------------------------------------------------------------------------------------
void handle_put(conn_t* conn, rwlock_t* lock, bool* existed) {

    //init
    const Response_t* res = NULL;
    char *uri = conn_get_uri(conn);

    // open the file
    int fd = open(uri, O_TRUNC | O_WRONLY, 0600);

    // handle invalid PUT
    if (fd < 0) {
        // fprintf(stderr, "%s: %d", uri, errno);
        if (errno == EACCES || errno == EISDIR || errno == ENOENT) {
            res = &RESPONSE_FORBIDDEN;
        }
        else {
            res = &RESPONSE_INTERNAL_SERVER_ERROR;
        }
    }
    
    // handle valid PUT
    writer_lock(lock);
    res = conn_recv_file(conn, fd);

    if (res == NULL && (*existed)) {
        res = &RESPONSE_OK;
    }
    else if (res == NULL && !(*existed)) {
        res = &RESPONSE_CREATED;
    }

    conn_send_response(conn, res);
    audit_log(conn, response_get_code(res), "PUT");
    writer_unlock(lock);

    close(fd);
}

//-----------------------------------------------------------------------------------------------------------------
void handle_unsupported(conn_t* conn) {

    // simply send unsupported response
    conn_send_response(conn, &RESPONSE_NOT_IMPLEMENTED);
}   

//-----------------------------------------------------------------------------------------------------------------
void handle_bad_request(conn_t* conn/*, const Response_t* res*/) {

    // simply send bad request response
    conn_send_response(conn, &RESPONSE_BAD_REQUEST);
}   

//-----------------------------------------------------------------------------------------------------------------
void print_worker_slots(slot_t** ws, int num_threads) {

    for (int i = 0; i < num_threads; i ++) {
        printf("Slot %d: URI: %s, Waiting: %d\n", i, ws[i]->uri, ws[i]->num_workers);
    }
}   

//-----------------------------------------------------------------------------------------------------------------
