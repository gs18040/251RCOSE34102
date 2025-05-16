#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#define TIME_MAX 1000
#define MAX_PROCESS 6
#define context_switch 0
typedef struct {
    unsigned int pid;
    unsigned int burst, s; // burst time, start(arrival) time
    unsigned int io_burst, io_s;
    unsigned int cpu_left, io_left;
    int p; // priority
} Process;

int cmp_burst(const void *a, const void *b) {
    Process *x = (Process*) a;
    Process *y = (Process*) b;
    return x->burst - y->burst;
}
int cmp_s(const void *a, const void *b) {
    Process *x = (Process*) a;
    Process *y = (Process*) b;
    if (x->s != y->s) return x->s - y->s;
    else return x->burst - y->burst;
}
// Queue DS
typedef struct {
    Process data[MAX_PROCESS];
    int front;
    int back;
} Queue;

void initQueue(Queue* q) {
    q->front = 0;
    q->back = 0;
}

bool isEmpty(Queue* q) {
    return q->front == q->back;
}

bool isFull(Queue* q) {
    return ((q->back + 1) % MAX_PROCESS) == q->front;
}

// 큐에 데이터 삽입 (enqueue)
void enqueue(Queue* q, Process x) {
    if (isFull(q)) {
        printf("Queue is full!\n");
        exit(0);
    }
    q->data[q->back] = x;
    q->back = (q->back + 1) % MAX_PROCESS;
}

// 큐에서 데이터 제거 (dequeue)
void dequeue(Queue* q) {
    if (isEmpty(q)) {
        printf("Queue is empty!\n");
        exit(0);
    }
    q->front = (q->front + 1) % MAX_PROCESS;
}

// 큐의 맨 앞 값 확인 (peek)
Process front(Queue* q) {
    if (isEmpty(q)) {
        printf("Queue is empty!\n");
        exit(0);
    }
    return q->data[q->front];
}

Queue *wq, *rq;
Queue ioq[101];
Process arr[MAX_PROCESS], arr2[MAX_PROCESS];
int fcfs_chart[TIME_MAX];
int sjf_chart[TIME_MAX];
int priority_chart[TIME_MAX];
int rr_chart[TIME_MAX];
int preemptive_priority_chart[TIME_MAX];
int preemptive_sjf_chart[TIME_MAX];
int n = 5;
void CreateProcess() {
    rq = malloc(sizeof(Queue));
    wq = malloc(sizeof(Queue));
    arr[1] = (Process){1, 2, 0, 0, TIME_MAX, 2, 0, 2};
    arr[2] = (Process){2, 1, 0, 0, TIME_MAX, 1, 0, 1};
    arr[3] = (Process){3, 8, 0, 0, TIME_MAX, 8, 0, 4};
    arr[4] = (Process){4, 4, 0, 0, TIME_MAX, 4, 0, 2};
    arr[5] = (Process){5, 5, 0, 0, TIME_MAX, 5, 0, 3};
    for (int i=1;i<=n;i++) arr2[i] = arr[i];
}
void init() {
    initQueue(rq);
    initQueue(wq);
    for (int i=1;i<=n;i++) {
        arr[i] = arr2[i];
    }
}
void display_Gantt(int *chart) {
    for (int i=0;i<=200;i++) {
        printf("%d ",chart[i]);
    }
}
void fcfs(int *chart) {
    init();
    Process cur;
    bool running = 0;
    int idle_done = 0;
    qsort(arr+1, n, sizeof(Process), cmp_s);
    for (int i=1;i<=n;i++) enqueue(rq, arr[i]);
    for (int T=0;T<=TIME_MAX;T++) {
        if (running) {
            if (cur.io_s == T) { // I/O start
                running = 0;
                enqueue(wq, cur);
                idle_done = T + context_switch;
                chart[T] = 0;
            }
            else {
                cur.cpu_left--;
                chart[T] = cur.pid;
            }
            if (cur.cpu_left == 0) {
                running = 0;
                idle_done = T + context_switch;
                // printf("done: %d\n", T);
                continue;
            }
        }
        if (T == idle_done && !running) { // Context switch over
            if (!isEmpty(wq) && front(wq).s <= T) {
                running = 1;
                idle_done = 0;
                enqueue(rq, front(wq));
                dequeue(wq);
                cur = front(rq); dequeue(rq);
                chart[T] = cur.pid;
            }
        }
        if (!isEmpty(rq) && front(rq).s <= T && !running) { // no process was running, new process starts
            running = 1;
            cur = front(rq); dequeue(rq);
            chart[T] = cur.pid;
            cur.cpu_left--;
            if (cur.cpu_left == 0) {
                running = 0;
                idle_done = T + context_switch;
                // printf("done: %d\n", T);
                continue;
            }
        }
    }
    display_Gantt(chart);
}
void sjf(int *chart) {
    init();
    Process cur;
    bool running = 0;
    int idle_done = 0;
    qsort(arr+1, n, sizeof(Process), cmp_burst);
    for (int i=1;i<=n;i++) enqueue(rq, arr[i]);
    for (int T=0;T<=TIME_MAX;T++) {
        if (running) {
            if (cur.io_s == T) { // I/O start
                running = 0;
                enqueue(wq, cur);
                idle_done = T + context_switch;
                chart[T] = 0;
            }
            else {
                cur.cpu_left--;
                chart[T] = cur.pid;
            }
            if (cur.cpu_left == 0) {
                running = 0;
                idle_done = T + context_switch;
                // printf("done: %d\n", T);
                continue;
            }
        }
        /*
        if (T == idle_done && !running) { // Context switch over
            if (isEmpty(wq) && front(wq).s <= T) {
                running = 1;
                idle_done = 0;
                enqueue(rq, front(wq));
                dequeue(wq);
                cur = front(rq); dequeue(rq);
                chart[T] = cur.pid;
            }
        }*/
        if (!isEmpty(rq) && front(rq).s <= T && !running) { // no process was running, new process starts
            running = 1;
            cur = front(rq); dequeue(rq);
            chart[T] = cur.pid;
            cur.cpu_left--;
            if (cur.cpu_left == 0) {
                running = 0;
                idle_done = T + context_switch;
                continue;
            }
        }
    }
    display_Gantt(chart);
}
/*
void priority(int *chart) {
    init();
    Process cur;
    bool running = 0;
    int idle_done = 0;
    sort(arr+1, arr+1+n, [&] (Process x, Process y) {
        return x.p > y.p;
    });
    for (int i=1;i<=n;i++) enqueue(rq, arr[i]);
    for (int T=0;T<=TIME_MAX;T++) {
        if (running) {
            if (cur.io_s == T) { // I/O start
                running = 0;
                wq.enqueue(cur);
                idle_done = T + context_switch;
                chart[T] = 0;
            }
            else {
                cur.cpu_left--;
                chart[T] = cur.pid;
            }
            if (cur.cpu_left == 0) {
                running = 0;
                idle_done = T + context_switch;
                // printf("done: %d\n", T);
                continue;
            }
        }
        if (T == idle_done && !running) { // Context switch over
            if (isEmpty(wq) && front(wq).s <= T) {
                running = 1;
                idle_done = 0;
                rq.enqueue(front(wq));
                dequeue(wq);
                cur = front(rq); dequeue(rq);
                chart[T] = cur.pid;
            }
        }
        if (!isEmpty(rq) && front(rq).s <= T && !running) { // no process was running, new process starts
            running = 1;
            cur = front(rq); dequeue(rq);
            chart[T] = cur.pid;
            cur.cpu_left--;
            if (cur.cpu_left == 0) {
                running = 0;
                idle_done = T + context_switch;
                // printf("done: %d\n", T);
                continue;
            }
        }
    }
    display_Gantt(chart);
}
void rr(int *chart, int tq) {
    init();
    Process cur;
    bool running = 0;
    int idle_done = 0;
    int t = tq;
    sort(arr+1, arr+1+n, [&] (Process x, Process y) {
        return x.s < y.s;
    });
    for (int i=1;i<=n;i++) rq.enqueue(arr[i]);
    for (int T=0;T<=TIME_MAX;T++) {
        if (running) {
            if (cur.io_s == T) { // I/O start
                running = 0;
                wq.enqueue(cur);
                idle_done = T + context_switch;
                chart[T] = 0;
            }
            else {
                cur.cpu_left--;
                t--;
                chart[T] = cur.pid;
            }
            if (cur.cpu_left == 0) {
                running = 0;
                idle_done = T + context_switch;
                // printf("done: %d\n", T);
                t = tq;
                continue;
            }
            else if (t == 0) { // time quantum over, but the process isn't over
                running = 0;
                idle_done = T + context_switch;
                rq.enqueue(cur);
                t = tq;
                continue;
            }
        }
        if (T == idle_done && !running) { // Context switch over
            if (isEmpty(wq) && front(wq).s <= T) {
                running = 1;
                idle_done = 0;
                rq.enqueue(front(wq));
                dequeue(wq);
                cur = front(rq); dequeue(rq);
                chart[T] = cur.pid;
            }
        }
        if (!isEmpty(rq) && front(rq).s <= T && !running) { // no process was running, new process starts
            running = 1;
            cur = front(rq); dequeue(rq);
            chart[T] = cur.pid;
            cur.cpu_left--;
            t--;
            if (cur.cpu_left == 0) {
                running = 0;
                idle_done = T + context_switch;
                // printf("done: %d\n", T);
                t = tq;
                continue;
            }
            else if (t == 0) { // time quantum over, but the process isn't over
                running = 0;
                idle_done = T + context_switch;
                rq.enqueue(cur);
                t = tq;
                continue;
            }
        }
    }
    display_Gantt(chart);
}
bool cmp(Process x, Process y) {
    return x.burst < y.burst;
}
void preemptive_sjf(int *chart) {
    init();
    Process cur;
    int cur_i = 1;
    bool running = 0;
    int idle_done = 0;
    sort(arr+1, arr+1+n, [&] (Process x, Process y) {
        return x.s < y.s;
    });
    priority_queue<pii, vector<pii>, greater<pii>> pq;
    int i = 1;
    for (int T=0;T<=TIME_MAX;T++) {
        while (i <= n && arr[i].s == T) {
            pq.enqueue({arr[i].cpu_left, i});
            i++;
        }
        if (T == idle_done && !running) { // Context switch over
            if (isEmpty(wq) && front(wq).s <= T) {
                running = 1;
                idle_done = 0;
                pq.enqueue({front(wq).cpu_left, cur_i});
                dequeue(wq);
                cur = arr[pq.top().snd]; pq.pop();
                chart[T] = cur.pid;
            }
        }
        if (!pq.empty()) { // select process with shortest remaining time
            running = 1;
            if (pq.top().snd == cur_i) { // no context switch
                chart[T] = cur.pid;
                cur.cpu_left--;
                pq.pop();
            }
            else {
                cur = arr[pq.top().snd];
                cur_i = pq.top().snd;
                pq.pop();
                idle_done = T + context_switch;
                if (T == idle_done) {
                    chart[T] = cur.pid;
                    cur.cpu_left--;
                }
            }
            if (cur.cpu_left == 0) {
                running = 0;
                idle_done = T + context_switch;
                // printf("done: %d\n", T);
                continue;
            }
            else {
                pq.enqueue({cur.cpu_left, cur_i});
            }
        }
    }
    display_Gantt(chart);
}
void preemptive_priority(int *chart) {
    init();
    Process cur;
    int cur_i = 1;
    bool running = 0;
    int idle_done = 0;
    sort(arr+1, arr+1+n, [&] (Process x, Process y) {
        return x.s < y.s;
    });
    priority_queue<pii> pq;
    int i = 1;
    for (int T=0;T<=TIME_MAX;T++) {
        while (i <= n && arr[i].s == T) {
            pq.enqueue({arr[i].p, i});
            i++;
        }
        if (T == idle_done && !running) { // Context switch over
            if (isEmpty(wq) && front(wq).s <= T) {
                running = 1;
                idle_done = 0;
                pq.enqueue({front(wq).cpu_left, cur_i});
                dequeue(wq);
                cur = arr[pq.top().snd]; pq.pop();
                chart[T] = cur.pid;
            }
        }
        if (!pq.empty()) { // select process with highest priority
            running = 1;
            if (pq.top().snd == cur_i) { // no context switch
                chart[T] = cur.pid;
                cur.cpu_left--;
            }
            else {
                cur = arr[pq.top().snd];
                cur_i = pq.top().snd;
                idle_done = T + context_switch;
                if (T == idle_done) {
                    chart[T] = cur.pid;
                    cur.cpu_left--;
                }
            }
            if (cur.cpu_left == 0) {
                running = 0;
                idle_done = T + context_switch;
                // printf("done: %d\n", T);
                continue;
            }
        }
    }
    display_Gantt(chart);
}
*/
int main()
{
    CreateProcess();
    fcfs(fcfs_chart);
    puts("");
    sjf(sjf_chart);
    puts("");
    /*
    priority(priority_chart);
    puts("");
    rr(rr_chart, 2);
    puts("");
    preemptive_sjf(preemptive_sjf_chart);
    puts("");
    preemptive_priority(preemptive_priority_chart);
    */
}