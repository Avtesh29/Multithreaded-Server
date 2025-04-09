# Multithreaded HTTP Server

This project is a multi-threaded HTTP server written in C. It extends off of basic HTTP Server by introducing thread-safe concurrency mechanisms to handle multiple simultaneous HTTP client requests in a consistent and linearizable way.

---

## 📌 Overview

The server uses a **dispatcher thread** to accept incoming client connections and a **thread pool** of worker threads to process those connections concurrently. It also produces a strict audit log, ensuring that the request-response behavior remains consistent with an atomic, serial execution model.

---

## 💡 Technologies and Concepts

**Technologies**: `C`, `POSIX Threads` (`pthreads`), `GCC`/`Clang`, `Makefile`, `Standard I/O`, `Socket Programming`, `Command-Line Argument Parsing` (`getopt`)

**Concepts**: `Multithreading`, `Thread Pools`, `Producer-Consumer Model`, `Thread-Safe Queues`, `Reader-Writer Locks`, `Mutexes`, `Condition Variables`, `Semaphores`, `Synchronization`, `Atomicity`, `Linearizability`, `Audit Logging`

---

## 🚀 Features

- Thread pool with configurable number of worker threads
- Dispatcher/worker design pattern
- Support for HTTP `GET` and `PUT` methods
- File operations with reader-writer synchronization
- Detailed and atomic audit logging to `stderr`
- Minimal synchronization overhead for high throughput

---

## 🧠  How It Works

1. **Startup**:
   - The server starts by parsing command-line arguments (`port` and optional `-t threads`).
   - Initializes the thread pool (default 4 workers if not specified).
   - Initializes the shared queue and synchronization mechanisms.

2. **Dispatcher Loop**:
   - Waits for incoming client connections via `accept()`.
   - For each new connection, pushes the socket file descriptor into the thread-safe queue.

3. **Worker Threads**:
   - Each worker blocks on the queue until a connection is available.
   - Once dequeued, the worker parses the request (GET or PUT), performs file operations, and sends a response back to the client.
   - It then logs the operation to `stderr` using the specified audit log format.

4. **Audit Log**:
   - Ensures a total and coherent order of requests, even with multiple threads handling clients concurrently.
   - Log entries reflect the order of actual processing, which must be consistent with real-time arrival order in cases where requests do not overlap.

5. **Graceful Termination**:
   - Ensures no memory leaks and minimal resource usage.

This design balances concurrency and consistency, simulating single-threaded correctness with the performance benefits of parallelism.

---

## 🏗️ Implementation

`queue.c` and `rwlock.c` contain the majority of synchorinzation techniques used in this project. 

#### 🧩 Key Components

- **Thread Pool (Worker Threads)**: The thread pool for worker threads is implemented with an array of slots. These slots allow for each worker thread to have a lock based on the URI they are handling (Threads processing the same URI will use the same lock).

- **Thread-Safe Queue**: The Queue used is implemented as a bounded buffer (circular) which utilizes semaphores to ensure order, maintain empty/full logic, and avoid contention.

- **Reader-Writer Locks**: `rwlock.c` and `rwlock.h` contain the implementation of a Reader-Writer Lock with 3 different priorities:
  - `READER` → Readers are always allowed to proceed before writers when there is contention for the lock.
  - `WRITER` → Writers are always given priority over readers during contention, preventing writer starvation.
  - `N-WAY` → Allows up to `n` readers between writers, balancing access and ensuring fairness while preventing both reader and writer starvation.

---

## 🛠️ Compilation

> [!NOTE]  
> The library implementation for the header files in this respository is kept private, meaning the program will not compile. A public version is in production and will be added when available.
>
> Recently added: `rwlock.c`  `queue.c`

Use the provided `Makefile`:

```bash
make
```
This generates an executable named httpserver. More `make` commands are available in the Makefile.

---

## ▶️ Run the Server

```bash
./httpserver [-t threads] <port>
```
- -t threads: (optional) Number of worker threads (defaults to 4 if not specified).
- <port>: Required port number for the server to listen on.

---

## ⚙️ Example

```bash
./httpserver -t 8 1234
```
This command launches the server on port 1234 with 8 worker threads handling requests concurrently.

---

## 📄 Audit Log Format

Each processed HTTP request is logged to stderr in the following comma-separated format:

```php-template
<Operation>,<URI>,<Status-Code>,<RequestID>
```
Where:
- Operation: The HTTP method (GET or PUT).
- URI: The path of the file (e.g., /file.txt).
- Status-Code: The HTTP status code returned (e.g., 200, 404).
- RequestID: The value of the optional Request-Id header (or 0 if not provided).

---

## 📌 Example Output

Here's an example audit log for a sequence of client requests:
```bash
GET,/a.txt,200,1
GET,/b.txt,404,2
PUT,/b.txt,201,3
GET,/b.txt,200,0
```
These entries are recorded in the order the server processed the requests, forming a consistent and total linearization of all client interactions.

---

