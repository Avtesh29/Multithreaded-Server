# Multithreaded HTTP Server

This project is a multi-threaded HTTP server written in C. It extends off of basic HTTP Server by introducing thread-safe concurrency mechanisms to handle multiple simultaneous HTTP client requests in a consistent and linearizable way.

## 📌 Overview

The server uses a **dispatcher thread** to accept incoming client connections and a **thread pool** of worker threads to process those connections concurrently. It also produces a strict audit log, ensuring that the request-response behavior remains consistent with an atomic, serial execution model.

## 🚀 Features

- ✅ Thread pool with configurable number of worker threads
- ✅ Dispatcher/worker design pattern
- ✅ Support for HTTP `GET` and `PUT` methods
- ✅ File operations with reader-writer synchronization
- ✅ Detailed and atomic audit logging to `stderr`
- ✅ Minimal synchronization overhead for high throughput

---

## 🛠️ Compilation

> [!NOTE]  
> The library implementation for the header files in this respository is kept private, meaning the program will not compile. A public version is in production and will be added when available.

Use the provided `Makefile`:

```bash
make
```
This generates an executable named httpserver. More make commands are available in the Makefile

## ▶️ Run the Server

```bash
./httpserver [-t threads] <port>
```
- -t threads: (optional) Number of worker threads (defaults to 4 if not specified)
- <port>: Required port number for the server to listen on

## ⚙️ Example

```bash
./httpserver -t 8 1234
```
This command launches the server on port 1234 with 8 worker threads handling requests concurrently.

## 📄 Audit Log Format

Each processed HTTP request is logged to stderr in the following comma-separated format:

```php-template
<Operation>,<URI>,<Status-Code>,<RequestID>
```
Where:
- Operation: The HTTP method (GET or PUT)
- URI: The path of the file (e.g., /file.txt)
- Status-Code: The HTTP status code returned (e.g., 200, 404)
- RequestID: The value of the optional Request-Id header (or 0 if not provided)

## 📌 Example Output

Here's an example audit log for a sequence of client requests:
```bash
GET,/a.txt,200,1
GET,/b.txt,404,2
PUT,/b.txt,201,3
GET,/b.txt,200,0
```
These entries are recorded in the order the server processed the requests, forming a consistent and total linearization of all client interactions.

