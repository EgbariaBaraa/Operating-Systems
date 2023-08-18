#ifndef __REQUEST_H__


typedef struct timeval Timeval;

typedef struct info {
    int requests_number;
    int static_requests;
    int dynamic_requests;
} *Info;

void requestHandle(int fd, Timeval arrival_time,
Timeval dispatch_interval, int handler_thread_id, int* handler_thread_req_count, int* handler_thread_static_req_count,
                   int* handler_thread_dynamic_req_count);

#endif
