#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>

#define TIME_MAX 100000
#define IO_MAX 3
#define MAX_PROCESS 30
#define MAX_DEVICE 3
#define MAX_EVENT 150
#define MLFQ_N 3
#define MAX_WAIT 10
#define context_switch 0
#define min(a,b) a<b?a:b

typedef struct {
    int pid; // pid
    int burst, s; // burst time, start(arrival) time
    int io_left, io_burst_sum; // remaining io time, total io burst time
    int cpu_left; // remaining cpu time
    int r; // recent
    int p; // priority, higher number = higher priority, priority > 0
    int mlfq_i, q_in; // level of queue, time that the process got in queue (for aging)
} Process;

typedef struct {
    int idx, io_burst, io_s, io_d; // index, io burst time, io arrival, io device
} IO_event;

IO_event events[MAX_EVENT + 1];
int io_n_list[MAX_PROCESS + 1];
int io_i[MAX_PROCESS + 1];
IO_event* event_list[MAX_PROCESS + 1][MAX_EVENT + 1];
Process arr[MAX_PROCESS + 1], arr2[MAX_PROCESS + 1];
int fcfs_ta[MAX_PROCESS + 1], fcfs_wa[MAX_PROCESS + 1];
int sjf_ta[MAX_PROCESS + 1], sjf_wa[MAX_PROCESS + 1];
int priority_ta[MAX_PROCESS + 1], priority_wa[MAX_PROCESS + 1];
int rr_ta[MAX_PROCESS + 1], rr_wa[MAX_PROCESS + 1];
int preemptive_priority_ta[MAX_PROCESS + 1], preemptive_priority_wa[MAX_PROCESS + 1];
int preemptive_sjf_ta[MAX_PROCESS + 1], preemptive_sjf_wa[MAX_PROCESS + 1];
int lottery_ta[MAX_PROCESS + 1], lottery_wa[MAX_PROCESS + 1];
int mlfq_ta[MAX_PROCESS + 1], mlfq_wa[MAX_PROCESS + 1];
int mlfq_aging_ta[MAX_PROCESS + 1], mlfq_aging_wa[MAX_PROCESS + 1];
int hrn_ta[MAX_PROCESS + 1], hrn_wa[MAX_PROCESS + 1];
int fcfs_chart[TIME_MAX + 1];
int sjf_chart[TIME_MAX + 1];
int priority_chart[TIME_MAX + 1];
int rr_chart[TIME_MAX + 1];
int preemptive_priority_chart[TIME_MAX + 1];
int preemptive_sjf_chart[TIME_MAX + 1];
int lottery_chart[TIME_MAX + 1];
int mlfq_chart[TIME_MAX + 1];
int mlfq_aging_chart[TIME_MAX + 1];
int hrn_chart[TIME_MAX + 1];
int n = 5, io_n = 0;
int T = 0;

// compare functions
typedef int (*CompareFunc)(const void*, const void*); 

int cmp_burst(const void *a, const void *b) {
    Process *x = (Process*) a;
    Process *y = (Process*) b;
    if (x->burst != y->burst) return y->burst - x->burst;
    else return x->r - y->r;
}
int cmp_left(const void *a, const void *b) {
    Process *x = (Process*) a;
    Process *y = (Process*) b;
    if (x->cpu_left != y->cpu_left) return y->cpu_left - x->cpu_left;
    else return x->r - y->r;
}
int cmp_hrn(const void *a, const void *b) {
    Process *x = (Process*) a;
    Process *y = (Process*) b;
    int x_next_burst, x_wait, y_next_burst, y_wait;
    if (io_n_list[x->pid] == 0) x_next_burst = x->burst;
    else if (io_i[x->pid] == 0) x_next_burst = event_list[x->pid][0]->io_s;
    else if (io_i[x->pid] == io_n_list[x->pid]) x_next_burst = x->burst - event_list[x->pid][io_n_list[x->pid]-1]->io_s;
    else x_next_burst = event_list[x->pid][io_i[x->pid]]->io_s - event_list[x->pid][io_i[x->pid] - 1]->io_s;

    if (io_n_list[y->pid] == 0) y_next_burst = y->burst;
    else if (io_i[y->pid] == 0) y_next_burst = event_list[y->pid][0]->io_s;
    else if (io_i[y->pid] == io_n_list[y->pid]) y_next_burst = y->burst - event_list[y->pid][io_n_list[y->pid]-1]->io_s;
    else y_next_burst = event_list[y->pid][io_i[y->pid]]->io_s - event_list[y->pid][io_i[y->pid] - 1]->io_s;

    x_wait = T - x->q_in; y_wait = T - y->q_in;
    assert(x_next_burst >= 0 && y_next_burst >= 0 && x_wait >= 0 && y_wait >= 0);
    
    if (x_wait * y_next_burst != x_next_burst * y_wait) return x_wait * y_next_burst - x_next_burst * y_wait;
    else return x->r - y->r;
}
int cmp_s(const void *a, const void *b) {
    Process *x = (Process*) a;
    Process *y = (Process*) b;
    if (x->s != y->s) return x->s - y->s;
    else if (x->r != y->r) return x->r - y->r;
    else return x->pid - y->pid;
}
int cmp_p(const void *a, const void *b) {
    Process *x = (Process*) a;
    Process *y = (Process*) b;
    if (x->p != y->p) return x->p - y->p;
    else return y->pid - x->pid;
}
int cmp_io_s(const void *a, const void *b) {
    IO_event *x = *(IO_event**) a;
    IO_event *y = *(IO_event**) b;
    return x->io_s - y->io_s;
}
// Queue DS
typedef struct {
    Process* data[MAX_PROCESS + 1];
    int front;
    int back;
} Queue;

Queue* ioq[MAX_DEVICE];

Queue* initqueue() {
    Queue *q = (Queue*)malloc(sizeof(Queue));
    q->front = 0;
    q->back = 0;
    return q;
}

bool isEmpty(Queue* q) {
    return q->front == q->back;
}

bool isFull(Queue* q) {
    return ((q->back + 1) % (MAX_PROCESS + 1)) == q->front;
}

void enqueue(Queue* q, Process* x) {
    if (isFull(q)) {
        printf("Queue is full!\n");
        exit(0);
    }
    q->data[q->back] = x;
    q->back = (q->back + 1) % (MAX_PROCESS + 1);
}

void dequeue(Queue* q) {
    if (isEmpty(q)) {
        printf("Queue is empty!\n");
        exit(0);
    }
    q->front = (q->front + 1) % (MAX_PROCESS + 1);
}

Process* front(Queue* q) {
    if (isEmpty(q)) {
        printf("Queue is empty!\n");
        exit(0);
    }
    return q->data[q->front];
}

int size(Queue* q) {
    return (q->back - q->front + (MAX_PROCESS + 1)) % (MAX_PROCESS + 1);
}

// Priority Queue DS
typedef struct {
    Process* data[MAX_PROCESS + 1]; // 요소 배열
    int size;          // 현재 요소 개수
    CompareFunc compare;
} PriorityQueue;

int parent(int i) { return (i-1)/2; }
int left(int i) { return 2*i + 1; }
int right(int i) { return 2*i + 2; }

void swap(Process* a, Process* b) {
    Process temp = *a;
    *a = *b;
    *b = temp;
}

// 초기화 함수
PriorityQueue* initpq(CompareFunc cmp) {
    PriorityQueue* pq = (PriorityQueue*)malloc(sizeof(PriorityQueue));
    pq->size = 0;
    pq->compare = cmp;
    return pq;
}

void enpq(PriorityQueue* pq, Process* x) {
    pq->data[pq->size] = x;
    int i = pq->size;
    pq->size++;
    
    while (i != 0 && pq->compare(pq->data[parent(i)], pq->data[i]) < 0) {
        swap(pq->data[i], pq->data[parent(i)]);
        i = parent(i);
    }
}

Process* depq(PriorityQueue* pq) {
    if (pq->size == 0) {
        printf("Queue is empty!\n");
        exit(0);
    }

    Process* root = pq->data[0];
    pq->data[0] = pq->data[pq->size-1];
    pq->size--;

    int i = 0;
    while (1) {
        int l = left(i);
        int r = right(i);
        int largest = i;
        
        if (l < pq->size && pq->compare(pq->data[l], pq->data[i]) > 0) largest = l;
        if (r < pq->size && pq->compare(pq->data[r], pq->data[largest]) > 0) largest = r;
        if (largest == i) break;
        
        swap(pq->data[i], pq->data[largest]);
        i = largest;
    }
    return root;
}

Process* top(PriorityQueue* pq) {
    if (pq->size == 0) {
        printf("Queue is empty!\n");
        exit(0);
    }
    return pq->data[0];
}

int isEmpty_pq(PriorityQueue* pq) {
    return pq->size == 0;
}

void initScheduler() {
    for (int i=0;i<MAX_DEVICE;i++) {
        ioq[i] = initqueue();
    }
    for (int i=0;i<io_n;i++) {
        event_list[events[i].idx][io_n_list[events[i].idx]] = &events[i];
        io_n_list[events[i].idx]++;
        arr[events[i].idx].io_burst_sum += events[i].io_burst;
    }
    for (int i=1;i<=n;i++) {
        qsort(event_list[i], io_n_list[i], sizeof(IO_event*), cmp_io_s);
    }
    for (int i=1;i<=n;i++) arr2[i] = arr[i];
}
void display_Gantt(int *chart, int over) {
    // return;
    int cnt[MAX_PROCESS + 1];
    for (int i=1;i<=MAX_PROCESS;i++) cnt[i] = 0;
    for (int i=0;i<=over;i++) {
        printf("%d ",chart[i]);
        if (chart[i] && chart[i] != chart[i+1]) {
            if (cnt[chart[i]] < io_n_list[chart[i]]) {
                printf("/ ");
            }
            cnt[chart[i]]++;
        }
    }
    puts("");
}
void display_eval(int *chart, int *ta, int *wa, int over) {
    int sw = 0, st = 0, srsp = 0;
    int rsp[MAX_PROCESS + 1];
    int run = 0, start = TIME_MAX;
    for (int i=1;i<=n;i++) {
        start = min(start, arr[i].s);
    }
    for (int i=over;i>=start;i--) {
        rsp[chart[i]] = i;
        if (chart[i]) run++;
    }

    for (int i=1;i<=n;i++) {
        printf("Process %d; waiting time: %d, turnaround time: %d, response time: %d\n",i,wa[i],ta[i],rsp[i]);
        sw += wa[i];
        st += ta[i];
        srsp += rsp[i];
    }
    assert(over >= start);
    printf("Average waiting time: %lf\nAverage turnaround time: %lf\nAverage response time: %lf\nCPU utilization: %lf\nIdle time: %d\n",(double)sw/n, (double)st/n,(double)srsp/n,(double)run/(over-start+1)*100.0,over-start+1-run);
}
void fcfs(int *chart, int *ta, int *wa) {
    Queue* rq = initqueue();
    for (int i=1;i<=n;i++) {
        arr[i] = arr2[i];
    }

    Process* cur = NULL;
    int idle_done = -1;
    int i = 1;
    bool done = 1;
    qsort(arr+1, n, sizeof(Process), cmp_s);
    for (T=0;T<=TIME_MAX;T++) {
        while (i <= n && arr[i].s == T) {
            arr[i].q_in = T;
            enqueue(rq, &arr[i]);
            i++;
        }
        if (!cur && !isEmpty(rq) && idle_done < T) {
            idle_done = -1;
            cur = front(rq);
            dequeue(rq);
        }
        while (cur && io_i[cur->pid] < io_n_list[cur->pid] && event_list[cur->pid][io_i[cur->pid]]->io_s == cur->burst - cur->cpu_left) {
            cur->io_left = event_list[cur->pid][io_i[cur->pid]]->io_burst;
            enqueue(ioq[event_list[cur->pid][io_i[cur->pid]]->io_d], cur);
            idle_done = T + context_switch;
            if (isEmpty(rq)) {
                cur = NULL;
                break;
            }
            else {
                cur = front(rq);
                dequeue(rq);
            }
        }
        if (cur && idle_done <= T) {
            idle_done = -1;
            cur->cpu_left--;
            chart[T] = cur->pid;
            if (cur->cpu_left == 0) {
                idle_done = T + context_switch;
                ta[cur->pid] = T - cur->s + 1;
                wa[cur->pid] = ta[cur->pid] - cur->burst - cur->io_burst_sum;
                cur = NULL;
            }
        }
        for (int i=0;i<MAX_DEVICE;i++) {
            // if (!isEmpty(ioq[i])) printf("%d %d %d %d\n",i,front(ioq[i])->pid,front(ioq[i])->io_left,T);
            if (!isEmpty(ioq[i])) {
                front(ioq[i])->io_left--;
                // printf("%d ",front(ioq[i])->pid);
            }
            if (!isEmpty(ioq[i]) && front(ioq[i])->io_left == 0) {
                enqueue(rq, front(ioq[i]));
                front(ioq[i])->q_in = T;
                io_i[front(ioq[i])->pid]++;
                dequeue(ioq[i]);
            }
        }
    }
    int over = TIME_MAX;
    for (int i=TIME_MAX;i>=0;i--) {
        if (chart[i]) {
            over = i;
            break;
        }
    }
    display_Gantt(chart, over);
    display_eval(chart, ta, wa, over);
}

void sjf(int *chart, int *ta, int *wa) {
    for (int i=0;i<MAX_DEVICE;i++) {
        assert(isEmpty(ioq[i]));
    }
    PriorityQueue* rq = initpq(cmp_left);
    for (int i=1;i<=n;i++) {
        arr[i] = arr2[i];
        io_i[i] = 0;
    }

    Process* cur = NULL;
    int idle_done = -1;
    int i = 1;
    qsort(arr+1, n, sizeof(Process), cmp_s);
    for (T=0;T<=TIME_MAX;T++) {
        while (i <= n && arr[i].s == T) {
            arr[i].q_in = T;
            enpq(rq, &arr[i]);
            i++;
        }
        if (!cur && !isEmpty_pq(rq) && idle_done < T) {
            idle_done = -1;
            cur = top(rq);
            depq(rq);
        }
        while (cur && io_i[cur->pid] < io_n_list[cur->pid] && event_list[cur->pid][io_i[cur->pid]]->io_s == cur->burst - cur->cpu_left) {
            cur->io_left = event_list[cur->pid][io_i[cur->pid]]->io_burst;
            enqueue(ioq[event_list[cur->pid][io_i[cur->pid]]->io_d], cur);
            idle_done = T + context_switch;
            if (isEmpty_pq(rq)) {
                cur = NULL;
                break;
            }
            else {
                cur = top(rq);
                depq(rq);
            }
        }
        if (cur && idle_done <= T) {
            idle_done = -1;
            cur->cpu_left--;
            chart[T] = cur->pid;
            if (cur->cpu_left == 0) {
                idle_done = T + context_switch;
                ta[cur->pid] = T - cur->s + 1;
                wa[cur->pid] = ta[cur->pid] - cur->burst - cur->io_burst_sum;
                cur = NULL;
            }
        }
        for (int i=0;i<MAX_DEVICE;i++) {
            if (!isEmpty(ioq[i])) front(ioq[i])->io_left--;
            if (!isEmpty(ioq[i]) && front(ioq[i])->io_left == 0) {
                io_i[front(ioq[i])->pid]++;
                front(ioq[i])->q_in = T;
                enpq(rq, front(ioq[i]));
                // printf("%d %d %d %d %d\n",i,T,front(ioq[i])->pid,io_i[front(ioq[i])->pid],io_n_list[front(ioq[i])->pid]);
                dequeue(ioq[i]);
            }
        }
    }
    int over = TIME_MAX;
    for (int i=TIME_MAX;i>=0;i--) {
        if (chart[i]) {
            over = i;
            break;
        }
    }
    display_Gantt(chart, over);
    display_eval(chart, ta, wa, over);
}

void hrn(int *chart, int *ta, int *wa) {
    PriorityQueue* rq = initpq(cmp_hrn);
    for (int i=1;i<=n;i++) {
        arr[i] = arr2[i];
        io_i[i] = 0;
    }

    Process* cur = NULL;
    int idle_done = -1;
    int i = 1;
    qsort(arr+1, n, sizeof(Process), cmp_s);
    for (T=0;T<=TIME_MAX;T++) {
        while (i <= n && arr[i].s == T) {
            arr[i].q_in = T;
            enpq(rq, &arr[i]);
            i++;
        }
        if (!cur && !isEmpty_pq(rq) && idle_done < T) {
            idle_done = -1;
            cur = top(rq);
            depq(rq);
        }
        while (cur && io_i[cur->pid] < io_n_list[cur->pid] && event_list[cur->pid][io_i[cur->pid]]->io_s == cur->burst - cur->cpu_left) {
            cur->io_left = event_list[cur->pid][io_i[cur->pid]]->io_burst;
            enqueue(ioq[event_list[cur->pid][io_i[cur->pid]]->io_d], cur);
            idle_done = T + context_switch;
            if (isEmpty_pq(rq)) {
                cur = NULL;
                break;
            }
            else {
                cur = top(rq);
                depq(rq);
            }
        }
        if (cur && idle_done <= T) {
            idle_done = -1;
            cur->cpu_left--;
            chart[T] = cur->pid;
            if (cur->cpu_left == 0) {
                idle_done = T + context_switch;
                ta[cur->pid] = T - cur->s + 1;
                wa[cur->pid] = ta[cur->pid] - cur->burst - cur->io_burst_sum;
                cur = NULL;
            }
        }
        for (int i=0;i<MAX_DEVICE;i++) {
            if (!isEmpty(ioq[i])) front(ioq[i])->io_left--;
            if (!isEmpty(ioq[i]) && front(ioq[i])->io_left == 0) {
                io_i[front(ioq[i])->pid]++;
                front(ioq[i])->q_in = T;
                enpq(rq, front(ioq[i]));
                // printf("%d %d %d %d %d\n",i,T,front(ioq[i])->pid,io_i[front(ioq[i])->pid],io_n_list[front(ioq[i])->pid]);
                dequeue(ioq[i]);
            }
        }
    }
    int over = TIME_MAX;
    for (int i=TIME_MAX;i>=0;i--) {
        if (chart[i]) {
            over = i;
            break;
        }
    }
    display_Gantt(chart, over);
    display_eval(chart, ta, wa, over);
}

void priority(int *chart, int *ta, int *wa) {
    PriorityQueue* rq = initpq(cmp_p);
    for (int i=1;i<=n;i++) {
        arr[i] = arr2[i];
        io_i[i] = 0;
    }

    Process* cur = NULL;
    int idle_done = -1;
    int i = 1;
    qsort(arr+1, n, sizeof(Process), cmp_s);
    for (T=0;T<=TIME_MAX;T++) {
        while (i <= n && arr[i].s == T) {
            arr[i].q_in = T;
            enpq(rq, &arr[i]);
            i++;
        }
        if (!cur && !isEmpty_pq(rq) && idle_done < T) {
            idle_done = -1;
            cur = top(rq);
            depq(rq);
        }
        while (cur && io_i[cur->pid] < io_n_list[cur->pid] && event_list[cur->pid][io_i[cur->pid]]->io_s == cur->burst - cur->cpu_left) {
            cur->io_left = event_list[cur->pid][io_i[cur->pid]]->io_burst;
            enqueue(ioq[event_list[cur->pid][io_i[cur->pid]]->io_d], cur);
            idle_done = T + context_switch;
            if (isEmpty_pq(rq)) {
                cur = NULL;
                break;
            }
            else {
                cur = top(rq);
                depq(rq);
            }
        }
        if (cur && idle_done <= T) {
            idle_done = -1;
            cur->cpu_left--;
            chart[T] = cur->pid;
            if (cur->cpu_left == 0) {
                idle_done = T + context_switch;
                ta[cur->pid] = T - cur->s + 1;
                wa[cur->pid] = ta[cur->pid] - cur->burst - cur->io_burst_sum;
                cur = NULL;
            }
        }
        for (int i=0;i<MAX_DEVICE;i++) {
            if (!isEmpty(ioq[i])) front(ioq[i])->io_left--;
            if (!isEmpty(ioq[i]) && front(ioq[i])->io_left == 0) {
                front(ioq[i])->q_in = T;
                io_i[front(ioq[i])->pid]++;
                enpq(rq, front(ioq[i]));
                // printf("%d %d %d %d %d\n",i,T,front(ioq[i])->pid,io_i[front(ioq[i])->pid],io_n_list[front(ioq[i])->pid]);
                dequeue(ioq[i]);
            }
        }
    }
    int over = TIME_MAX;
    for (int i=TIME_MAX;i>=0;i--) {
        if (chart[i]) {
            over = i;
            break;
        }
    }
    display_Gantt(chart, over);
    display_eval(chart, ta, wa, over);
}
void rr(int *chart, int *ta, int *wa, int tq) {
    Queue* rq = initqueue();
    for (int i=1;i<=n;i++) {
        arr[i] = arr2[i];
        io_i[i] = 0;
    }

    Process* cur = NULL;
    int idle_done = -1;
    int i = 1;
    qsort(arr+1, n, sizeof(Process), cmp_s);
    int t = tq;
    for (T=0;T<=TIME_MAX;T++) {
        while (i <= n && arr[i].s == T) {
            arr[i].q_in = T;
            enqueue(rq, &arr[i]);
            i++;
        }
        if (!cur && !isEmpty(rq) && idle_done < T) {
            idle_done = -1;
            cur = front(rq);
            dequeue(rq);
        }
        while (cur && io_i[cur->pid] < io_n_list[cur->pid] && event_list[cur->pid][io_i[cur->pid]]->io_s == cur->burst - cur->cpu_left) {
            cur->io_left = event_list[cur->pid][io_i[cur->pid]]->io_burst;
            enqueue(ioq[event_list[cur->pid][io_i[cur->pid]]->io_d], cur);
            idle_done = T + context_switch;
            if (isEmpty(rq)) {
                cur = NULL;
                break;
            }
            else {
                cur = front(rq);
                dequeue(rq);
            }
        }
        if (cur && idle_done <= T) {
            cur->cpu_left--;
            t--;
            chart[T] = cur->pid;
            if (cur->cpu_left == 0) {
                idle_done = T + context_switch;
                ta[cur->pid] = T - cur->s + 1;
                wa[cur->pid] = ta[cur->pid] - cur->burst - cur->io_burst_sum;
                t = tq;
                cur = NULL;
            }
            else if (t == 0) { // time quantum over, but the process isn't over
                idle_done = T + context_switch;
                cur->r = 1;
                cur->q_in = T;
                enqueue(rq, cur);
                t = tq;
                cur = NULL;
            }
        }
        for (int i=0;i<MAX_DEVICE;i++) {
            if (!isEmpty(ioq[i])) front(ioq[i])->io_left--;
            if (!isEmpty(ioq[i]) && front(ioq[i])->io_left == 0) {
                enqueue(rq, front(ioq[i]));
                io_i[front(ioq[i])->pid]++;
                dequeue(ioq[i]);
            }
        }
    }
    int over = TIME_MAX;
    for (int i=TIME_MAX;i>=0;i--) {
        if (chart[i]) {
            over = i;
            break;
        }
    }
    display_Gantt(chart, over);
    display_eval(chart, ta, wa, over);
}
void preemptive_sjf(int *chart, int *ta, int *wa) {
    PriorityQueue* rq = initpq(cmp_left);
    for (int i=1;i<=n;i++) {
        arr[i] = arr2[i];
        io_i[i] = 0;
    }

    Process* cur = NULL;
    int idle_done = -1;
    int i = 1;
    qsort(arr+1, n, sizeof(Process), cmp_s);
    for (T=0;T<=TIME_MAX;T++) {
        while (i <= n && arr[i].s == T) {
            arr[i].q_in = T;
            enpq(rq, &arr[i]);
            i++;
        }
        if (!cur && !isEmpty_pq(rq) && idle_done < T) {
            idle_done = -1;
            cur = top(rq);
            depq(rq);
        }
        while (cur && io_i[cur->pid] < io_n_list[cur->pid] && event_list[cur->pid][io_i[cur->pid]]->io_s == cur->burst - cur->cpu_left) {
            cur->io_left = event_list[cur->pid][io_i[cur->pid]]->io_burst;
            enqueue(ioq[event_list[cur->pid][io_i[cur->pid]]->io_d], cur);
            idle_done = T + context_switch;
            if (isEmpty_pq(rq)) {
                cur = NULL;
                break;
            }
            else {
                cur = top(rq);
                depq(rq);
            }
        }
        if (cur && idle_done <= T) {
            chart[T] = cur->pid;
            cur->cpu_left--;
            if (cur->cpu_left == 0) {
                idle_done = T + context_switch;
                ta[cur->pid] = T - cur->s + 1;
                wa[cur->pid] = ta[cur->pid] - cur->burst - cur->io_burst_sum;
                cur = NULL;
            }
            else {
                cur->r = 1;
                enpq(rq, cur);
                cur = NULL;
            }
        }
        for (int i=0;i<MAX_DEVICE;i++) {
            if (!isEmpty(ioq[i])) front(ioq[i])->io_left--;
            if (!isEmpty(ioq[i]) && front(ioq[i])->io_left == 0) {
                io_i[front(ioq[i])->pid]++;
                front(ioq[i])->q_in = T;
                enpq(rq, front(ioq[i]));
                // printf("%d %d %d %d %d\n",i,T,front(ioq[i])->pid,io_i[front(ioq[i])->pid],io_n_list[front(ioq[i])->pid]);
                dequeue(ioq[i]);
            }
        }
    }
    int over = TIME_MAX;
    for (int i=TIME_MAX;i>=0;i--) {
        if (chart[i]) {
            over = i;
            break;
        }
    }
    display_Gantt(chart, over);
    display_eval(chart, ta, wa, over);
}
void preemptive_priority(int *chart, int *ta, int *wa) {
    PriorityQueue* rq = initpq(cmp_p);
    for (int i=1;i<=n;i++) {
        arr[i] = arr2[i];
        io_i[i] = 0;
    }

    Process* cur = NULL;
    int idle_done = -1;
    int i = 1;
    qsort(arr+1, n, sizeof(Process), cmp_s);
    for (T=0;T<=TIME_MAX;T++) {
        while (i <= n && arr[i].s == T) {
            arr[i].q_in = T;
            enpq(rq, &arr[i]);
            i++;
        }
        if (!cur && !isEmpty_pq(rq) && idle_done < T) {
            idle_done = -1;
            cur = top(rq);
            depq(rq);
        }
        while (cur && io_i[cur->pid] < io_n_list[cur->pid] && event_list[cur->pid][io_i[cur->pid]]->io_s == cur->burst - cur->cpu_left) {
            cur->io_left = event_list[cur->pid][io_i[cur->pid]]->io_burst;
            enqueue(ioq[event_list[cur->pid][io_i[cur->pid]]->io_d], cur);
            idle_done = T + context_switch;
            if (isEmpty_pq(rq)) {
                cur = NULL;
                break;
            }
            else {
                cur = top(rq);
                depq(rq);
            }
        }
        if (cur && idle_done <= T) {
            chart[T] = cur->pid;
            cur->cpu_left--;
            if (cur->cpu_left == 0) {
                idle_done = T + context_switch;
                ta[cur->pid] = T - cur->s + 1;
                wa[cur->pid] = ta[cur->pid] - cur->burst - cur->io_burst_sum;
                cur = NULL;
            }
            else {
                cur->r = 1;
                enpq(rq, cur);
                cur = NULL;
            }
        }
        for (int i=0;i<MAX_DEVICE;i++) {
            if (!isEmpty(ioq[i])) front(ioq[i])->io_left--;
            if (!isEmpty(ioq[i]) && front(ioq[i])->io_left == 0) {
                io_i[front(ioq[i])->pid]++;
                front(ioq[i])->q_in = T;
                enpq(rq, front(ioq[i]));
                // printf("%d %d %d %d %d\n",i,T,front(ioq[i])->pid,io_i[front(ioq[i])->pid],io_n_list[front(ioq[i])->pid]);
                dequeue(ioq[i]);
            }
        }
    }
    int over = TIME_MAX;
    for (int i=TIME_MAX;i>=0;i--) {
        if (chart[i]) {
            over = i;
            break;
        }
    }
    display_Gantt(chart, over);
    display_eval(chart, ta, wa, over);
}
Process* gacha(Queue* rq) {
    if (isEmpty(rq)) return NULL;
    int sz = size(rq), s = 0;
    int cur = rq->front, priority_sum = 0;
    for (int i=0;i<sz;i++) {
        priority_sum += rq->data[cur]->p;
        cur = (cur + 1) % (MAX_PROCESS + 1);
    }
    cur = rq->front;
    int x = 1 + (rand() % priority_sum);
    for (int i=0;i<sz;i++) {
        s += rq->data[cur]->p;
        if (s >= x) {
            Process* ret = rq->data[cur];
            for (int j=i;j>0;j--) {
                int prev = (cur - 1 + (MAX_PROCESS + 1)) % (MAX_PROCESS + 1);
                rq->data[cur] = rq->data[prev];
                cur = prev;
            }
            rq->front = (rq->front + 1) % (MAX_PROCESS + 1);
            return ret;
        }
        cur = (cur + 1) % (MAX_PROCESS + 1);
    }
    return NULL;
}
void lottery(int *chart, int *ta, int *wa, int tq) {
    Queue* rq = initqueue();
    for (int i=1;i<=n;i++) {
        arr[i] = arr2[i];
        io_i[i] = 0;
    }

    Process* cur = NULL;
    int idle_done = -1;
    int i = 1;
    qsort(arr+1, n, sizeof(Process), cmp_s);
    int t = tq;
    for (T=0;T<=TIME_MAX;T++) {
        while (i <= n && arr[i].s == T) {
            arr[i].q_in = T;
            enqueue(rq, &arr[i]);
            i++;
        }
        if (!cur && !isEmpty(rq) && idle_done < T) {
            idle_done = -1;
            cur = gacha(rq);
        }
        while (cur && io_i[cur->pid] < io_n_list[cur->pid] && event_list[cur->pid][io_i[cur->pid]]->io_s == cur->burst - cur->cpu_left) {
            cur->io_left = event_list[cur->pid][io_i[cur->pid]]->io_burst;
            enqueue(ioq[event_list[cur->pid][io_i[cur->pid]]->io_d], cur);
            idle_done = T + context_switch;
            if (isEmpty(rq)) {
                cur = NULL;
                break;
            }
            else {
                cur = gacha(rq);
            }
        }
        if (cur && idle_done <= T) {
            cur->cpu_left--;
            t--;
            chart[T] = cur->pid;
            if (cur->cpu_left == 0) {
                idle_done = T + context_switch;
                ta[cur->pid] = T - cur->s + 1;
                wa[cur->pid] = ta[cur->pid] - cur->burst - cur->io_burst_sum;
                t = tq;
                cur = NULL;
            }
            else if (t == 0) { // time quantum over, but the process isn't over
                idle_done = T + context_switch;
                cur->r = 1;
                enqueue(rq, cur);
                t = tq;
                cur = NULL;
            }
        }
        for (int i=0;i<MAX_DEVICE;i++) {
            if (!isEmpty(ioq[i])) front(ioq[i])->io_left--;
            if (!isEmpty(ioq[i]) && front(ioq[i])->io_left == 0) {
                enqueue(rq, front(ioq[i]));
                io_i[front(ioq[i])->pid]++;
                dequeue(ioq[i]);
            }
        }
    }
    int over = TIME_MAX;
    for (int i=TIME_MAX;i>=0;i--) {
        if (chart[i]) {
            over = i;
            break;
        }
    }
    display_Gantt(chart, over);
    display_eval(chart, ta, wa, over);
}
void mlfq(int *chart, int *ta, int *wa, int tq) {
    Queue* rq[MLFQ_N];
    for (int i=0;i<MLFQ_N;i++) {
        rq[i] = initqueue();
    }
    for (int i=1;i<=n;i++) {
        arr[i] = arr2[i];
        io_i[i] = 0;
    }

    int idle_done = -1;
    int i = 1;
    Process *cur = NULL;
    qsort(arr+1, n, sizeof(Process), cmp_s);
    int t = tq;
    for (T=0;T<=TIME_MAX;T++) {
        while (i <= n && arr[i].s == T) {
            arr[i].q_in = T;
            enqueue(rq[0], &arr[i]);
            i++;
        }
        if (!cur && idle_done < T) {
            idle_done = -1;
            for (int j=0;j<MLFQ_N;j++) {
                if (!cur && !isEmpty(rq[j])) {
                    cur = front(rq[j]);
                    dequeue(rq[j]);
                    t = tq << j;
                    break;
                }
            }
        }
        while (cur && io_i[cur->pid] < io_n_list[cur->pid] && event_list[cur->pid][io_i[cur->pid]]->io_s == cur->burst - cur->cpu_left) {
            cur->io_left = event_list[cur->pid][io_i[cur->pid]]->io_burst;
            enqueue(ioq[event_list[cur->pid][io_i[cur->pid]]->io_d], cur);
            idle_done = T + context_switch;
            cur = NULL;
            for (int j=0;j<MLFQ_N;j++) {
                if (!cur && !isEmpty(rq[j])) {
                    cur = front(rq[j]);
                    dequeue(rq[j]);
                    t = tq << j;
                    break;
                }
            }
            if (!cur) {
                break;
            }
        }
        if (cur && idle_done <= T) {
            cur->cpu_left--;
            t--;
            chart[T] = cur->pid;
            if (cur->cpu_left == 0) {
                idle_done = T + context_switch;
                ta[cur->pid] = T - cur->s + 1;
                wa[cur->pid] = ta[cur->pid] - cur->burst - cur->io_burst_sum;
                cur = NULL;
            }
            else if (t == 0) { // time quantum over, but the process isn't over
                idle_done = T + context_switch;
                cur->r = 1;
                cur->q_in = T;
                if (cur->mlfq_i < MLFQ_N - 1) cur->mlfq_i++; // move to less priority queue
                assert(cur->mlfq_i < MLFQ_N);
                enqueue(rq[cur->mlfq_i], cur);
                cur = NULL;
            }
        }
        for (int i=0;i<MAX_DEVICE;i++) {
            if (!isEmpty(ioq[i])) front(ioq[i])->io_left--;
            if (!isEmpty(ioq[i]) && front(ioq[i])->io_left == 0) {
                front(ioq[i])->q_in = T;
                enqueue(rq[front(ioq[i])->mlfq_i], front(ioq[i])); // process with I/O has remains in the same level of mlfq
                io_i[front(ioq[i])->pid]++;
                dequeue(ioq[i]);
            }
        }
    }
    int over = TIME_MAX;
    for (int i=TIME_MAX;i>=0;i--) {
        if (chart[i]) {
            over = i;
            break;
        }
    }
    display_Gantt(chart, over);
    display_eval(chart, ta, wa, over);
}
void mlfq_aging(int *chart, int *ta, int *wa, int tq) {
    Queue* rq[MLFQ_N];
    for (int i=0;i<MLFQ_N;i++) {
        rq[i] = initqueue();
    }
    for (int i=1;i<=n;i++) {
        arr[i] = arr2[i];
        io_i[i] = 0;
    }

    int idle_done = -1;
    int i = 1;
    Process *cur = NULL;
    qsort(arr+1, n, sizeof(Process), cmp_s);
    int t = tq;
    for (T=0;T<=TIME_MAX;T++) {
        while (i <= n && arr[i].s == T) {
            arr[i].q_in = T;
            enqueue(rq[0], &arr[i]);
            i++;
        }
        if (!cur && idle_done < T) {
            idle_done = -1;
            for (int j=0;j<MLFQ_N;j++) {
                if (!cur && !isEmpty(rq[j])) {
                    cur = front(rq[j]);
                    dequeue(rq[j]);
                    t = tq << j;
                    break;
                }
            }
        }
        while (cur && io_i[cur->pid] < io_n_list[cur->pid] && event_list[cur->pid][io_i[cur->pid]]->io_s == cur->burst - cur->cpu_left) {
            cur->io_left = event_list[cur->pid][io_i[cur->pid]]->io_burst;
            enqueue(ioq[event_list[cur->pid][io_i[cur->pid]]->io_d], cur);
            idle_done = T + context_switch;
            cur = NULL;
            for (int j=0;j<MLFQ_N;j++) {
                if (!cur && !isEmpty(rq[j])) {
                    cur = front(rq[j]);
                    dequeue(rq[j]);
                    t = tq << j;
                    break;
                }
            }
            if (!cur) {
                break;
            }
        }
        if (cur && idle_done <= T) {
            idle_done = -1;
            cur->cpu_left--;
            t--;
            chart[T] = cur->pid;
            if (cur->cpu_left == 0) {
                idle_done = T + context_switch;
                ta[cur->pid] = T - cur->s + 1;
                wa[cur->pid] = ta[cur->pid] - cur->burst - cur->io_burst_sum;
                cur = NULL;
            }
            else if (t == 0) { // time quantum over, but the process isn't over
                idle_done = T + context_switch;
                cur->r = 1;
                if (cur->mlfq_i < MLFQ_N - 1) cur->mlfq_i++; // move to less priority queue
                cur->q_in = T;
                assert(cur->mlfq_i < MLFQ_N);
                enqueue(rq[cur->mlfq_i], cur);
                cur = NULL;
            }
        }
        for (int i=0;i<MAX_DEVICE;i++) {
            if (!isEmpty(ioq[i])) front(ioq[i])->io_left--;
            if (!isEmpty(ioq[i]) && front(ioq[i])->io_left == 0) {
                front(ioq[i])->q_in = T;
                enqueue(rq[front(ioq[i])->mlfq_i], front(ioq[i])); // process with I/O has remains in the same level of mlfq
                io_i[front(ioq[i])->pid]++;
                dequeue(ioq[i]);
            }
        }
        for (int j=1;j<MLFQ_N;j++) { // Level 0 queue never ages
            while (!isEmpty(rq[j]) && T - front(rq[j])->q_in == MAX_WAIT) {
                front(rq[j])->mlfq_i--;
                front(rq[j])->q_in = T;
                enqueue(rq[front(rq[j])->mlfq_i], front(rq[j]));
                dequeue(rq[j]);
            }
        }
    }
    int over = TIME_MAX;
    for (int i=TIME_MAX;i>=0;i--) {
        if (chart[i]) {
            over = i;
            break;
        }
    }
    display_Gantt(chart, over);
    display_eval(chart, ta, wa, over);
}
int main()
{
    // freopen("aging.txt", "r", stdin);
    srand(time(NULL));
    int choice, tq = 2;
    printf("Input 1 for random process creation, 2 for manual process creation, 3 for test\n");
    printf("Choice: ");
    scanf("%d",&choice);
    if (choice == 1) {
        n = 1 + (rand() % MAX_PROCESS);
        io_n = rand() % MAX_EVENT;
        io_n = 5;
        printf("number of process: %d\nnumber of events: %d\n",n,io_n);
        for (int i=1;i<=n;i++) {
            int b = 1 + (rand() % 20); // burst
            int a = 1 + (rand() % 20); // arrival
            int p = 1 + (rand() % 20);
            arr[i] = (Process){i, b, a, -1, 0, b, 0, p, 0, 0};
            printf("Process %d : %d (arrival), %d (burst), %d (priority)\n",arr[i].pid,arr[i].s,arr[i].burst,arr[i].p);
        }
        for (int i=0;i<io_n;i++) {
            int j = 1 + (rand() % n);
            int a = rand() % arr[j].burst; // arrival
            int b = 1 + (rand() % 10); // burst
            int d = rand() % MAX_DEVICE;
            events[i] = (IO_event){j, b, a, d};
            printf("%d %d %d %d\n",events[i].idx,events[i].io_burst,events[i].io_s,events[i].io_d);
        }
    }
    else if (choice == 2) {
        printf("Input number of processes: ");
        scanf("%d",&n);
        printf("Input number of I/O events: ");
        scanf("%d",&io_n);
        printf("Input time quantum: ");
        scanf("%d",&tq);
        printf("Input process data: (burst, arrival, priority)\n");
        for (int i=1;i<=n;i++) {
            arr[i].pid = i;
            arr[i].r = 0;
            scanf("%d %d %d",&arr[i].burst, &arr[i].s, &arr[i].p);
            arr[i].cpu_left = arr[i].burst;
        }
        printf("Input I/O data: (pid, I/O burst, I/O event arrival, I/O device)\n");
        for (int i=0;i<io_n;i++) {
            scanf("%d %d %d %d",&events[i].idx,&events[i].io_burst,&events[i].io_s,&events[i].io_d);
        }
    }
    else {
        // Test
        // pid, burst, arrival, io_end, io_burst_sum, cpu_left, r, p, mlfq_i, q_in
        // pid, burst, arrival, device
        n = 5;
        io_n = 1;
        events[0] = (IO_event){1, 2, 1, 0};
        arr[1] = (Process){1, 2, 0, -1, 0, 2, 0, 2, 0, 0};
        arr[2] = (Process){2, 1, 0, -1, 0, 1, 0, 1, 0, 0};
        arr[3] = (Process){3, 8, 0, -1, 0, 8, 0, 4, 0, 0};
        arr[4] = (Process){4, 4, 0, -1, 0, 4, 0, 2, 0, 0};
        arr[5] = (Process){5, 5, 0, -1, 0, 5, 0, 3, 0, 0};
    }
    initScheduler();
    puts("FCFS Scheduler");
    fcfs(fcfs_chart, fcfs_ta, fcfs_wa);
    puts("");
    puts("SJF Scheduler");
    sjf(sjf_chart, sjf_ta, sjf_wa);
    puts("");
    puts("Priority Scheduler");
    priority(priority_chart, priority_ta, priority_wa);
    puts("");
    puts("RR Scheduler (default time quantum is 2)");
    rr(rr_chart, rr_ta, rr_wa, tq);
    puts("");
    puts("Preemptive SJF Scheduler");
    preemptive_sjf(preemptive_sjf_chart, preemptive_sjf_ta, preemptive_sjf_wa);
    puts("");
    puts("Preemptive Priority Scheduler");
    preemptive_priority(preemptive_priority_chart, preemptive_priority_ta, preemptive_priority_wa);
    puts("");
    puts("Highest Response Ratio Next Scheduler");
    hrn(hrn_chart, hrn_ta, hrn_wa);
    puts("");
    puts("Lottery Scheduler (default time quantum is 2)");
    lottery(lottery_chart, lottery_ta, lottery_wa, tq);
    puts("");
    puts("MLFQ Scheduler (default time quantum is 2)");
    mlfq(mlfq_chart, mlfq_ta, mlfq_wa, tq);
    puts("");
    puts("MLFQ Scheduler with aging (default time quantum is 2)");
    mlfq_aging(mlfq_aging_chart, mlfq_aging_ta, mlfq_aging_wa, tq);
    puts("");
}
