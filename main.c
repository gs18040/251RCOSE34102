#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>

#define TIME_MAX 1000
#define MAX_PROCESS 6
#define context_switch 0
typedef struct {
    unsigned int pid;
    unsigned int burst, s; // burst time, start(arrival) time
    unsigned int io_burst, io_s;
    unsigned int cpu_left, io_left;
    int r;
    int p; // priority
} Process;

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
int cmp_s(const void *a, const void *b) {
    Process *x = (Process*) a;
    Process *y = (Process*) b;
    if (x->s != y->s) return x->s - y->s;
    else return x->pid - y->pid;
}
int cmp_p(const void *a, const void *b) {
    Process *x = (Process*) a;
    Process *y = (Process*) b;
    if (x->p != y->p) return x->p - y->p;
    else return y->pid - x->pid;
}
// Queue DS
typedef struct {
    Process data[MAX_PROCESS + 1];
    int front;
    int back;
} Queue;

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

// Priority Queue DS
typedef struct {
    Process data[MAX_PROCESS + 1]; // 요소 배열
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

// 요소 삽입 (O(log n))
void enpq(PriorityQueue* pq, Process x) {
    // 새 요소를 끝에 추가
    pq->data[pq->size] = x;
    int i = pq->size;
    pq->size++;
    
    // 힙 속성 복구 (위로 올라가며 교환)
    while (i != 0 && pq->compare(&pq->data[parent(i)], &pq->data[i]) < 0) {
        swap(&pq->data[i], &pq->data[parent(i)]);
        i = parent(i);
    }
}

// 최우선 요소 제거 및 반환 (O(log n))
Process depq(PriorityQueue* pq) {
    if (pq->size == 0) {
        printf("Queue is empty!\n");
        exit(0);
    }

    Process root = pq->data[0];
    pq->data[0] = pq->data[pq->size-1];
    pq->size--;

    // 아래로 내리기(힙 속성 유지)
    int i = 0;
    while (1) {
        int l = left(i);
        int r = right(i);
        int largest = i;
        
        if (l < pq->size && pq->compare(&pq->data[l], &pq->data[i]) > 0) largest = l;
        if (r < pq->size && pq->compare(&pq->data[r], &pq->data[largest]) > 0) largest = r;
        if (largest == i) break;
        
        swap(&pq->data[i], &pq->data[largest]);
        i = largest;
    }
    return root;
}

// 최우선 요소 확인 (peek)
Process top(PriorityQueue* pq) {
    if (pq->size == 0) {
        printf("Queue is empty!\n");
        exit(0);
    }
    return pq->data[0];
}

// 큐가 비었는지 확인
int isEmpty_pq(PriorityQueue* pq) {
    return pq->size == 0;
}

Queue ioq[101];
Process arr[MAX_PROCESS + 1], arr2[MAX_PROCESS + 1];
int fcfs_ta[MAX_PROCESS + 1], fcfs_wa[MAX_PROCESS + 1];
int sjf_ta[MAX_PROCESS + 1], sjf_wa[MAX_PROCESS + 1];
int priority_ta[MAX_PROCESS + 1], priority_wa[MAX_PROCESS + 1];
int rr_ta[MAX_PROCESS + 1], rr_wa[MAX_PROCESS + 1];
int preemptive_priority_ta[MAX_PROCESS + 1], preemptive_priority_wa[MAX_PROCESS + 1];
int preemptive_sjf_ta[MAX_PROCESS + 1], preemptive_sjf_wa[MAX_PROCESS + 1];
int fcfs_chart[TIME_MAX];
int sjf_chart[TIME_MAX];
int priority_chart[TIME_MAX];
int rr_chart[TIME_MAX];
int preemptive_priority_chart[TIME_MAX];
int preemptive_sjf_chart[TIME_MAX];
int n = 5;
void initScheduler() {
    for (int i=1;i<=n;i++) arr2[i] = arr[i];
}
void display_Gantt(int *chart) {
    for (int i=0;i<=80;i++) {
        printf("%d ",chart[i]);
    }
    puts("");
}
void display_eval(int *ta, int *wa) {
    int sw = 0, st = 0;
    for (int i=1;i<=n;i++) {
        printf("Process %d's waiting time is %d and turnaround time is %d\n",i,wa[i],ta[i]);
        sw += wa[i];
        st += ta[i];
    }
    printf("Average waiting time: %lf\nAverage turnaround time: %lf\n",(double)sw/n, (double)st/n);
}
void fcfs(int *chart, int *ta, int *wa) {
    Queue* rq = initqueue();
    Queue* wq = initqueue();
    for (int i=1;i<=n;i++) {
        arr[i] = arr2[i];
    }

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
                ta[cur.pid] = T - cur.s + 1;
                wa[cur.pid] = ta[cur.pid] - cur.burst - cur.io_burst;
                continue;
            }
        }
        /*
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
        */
        if (!isEmpty(rq) && front(rq).s <= T && !running) { // no process was running, new process starts
            running = 1;
            cur = front(rq); dequeue(rq);
            chart[T] = cur.pid;
            cur.cpu_left--;
            if (cur.cpu_left == 0) {
                running = 0;
                idle_done = T + context_switch;
                ta[cur.pid] = T - cur.s + 1;
                wa[cur.pid] = ta[cur.pid] - cur.burst - cur.io_burst;
                continue;
            }
        }
    }
    display_Gantt(chart);
    display_eval(ta, wa);
}
void sjf(int *chart, int *ta, int *wa) {
    PriorityQueue* rq = initpq(cmp_burst);
    Queue* wq = initqueue();
    for (int i=1;i<=n;i++) {
        arr[i] = arr2[i];
    }

    Process cur;
    bool running = 0;
    int idle_done = 0;
    int i = 1;
    qsort(arr+1, n, sizeof(Process), cmp_s);
    for (int T=0;T<=TIME_MAX;T++) {
        while (i <= n && arr[i].s == T) {
            enpq(rq, arr[i]);
            i++;
        }
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
                ta[cur.pid] = T - cur.s + 1;
                wa[cur.pid] = ta[cur.pid] - cur.burst - cur.io_burst;
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
        if (!isEmpty_pq(rq) && !running) { // no process was running, new process starts
            running = 1;
            cur = top(rq); depq(rq);
            // printf("%d %d %d %d\n",rq->size,T,cur.pid,cur.burst);
            chart[T] = cur.pid;
            cur.cpu_left--;
            if (cur.cpu_left == 0) {
                running = 0;
                idle_done = T + context_switch;
                ta[cur.pid] = T - cur.s + 1;
                wa[cur.pid] = ta[cur.pid] - cur.burst - cur.io_burst;
                continue;
            }
        }
    }
    display_Gantt(chart);
    display_eval(ta, wa);
}
void priority(int *chart, int *ta, int *wa) {
    PriorityQueue* rq = initpq(cmp_p);
    Queue* wq = initqueue();
    for (int i=1;i<=n;i++) {
        arr[i] = arr2[i];
    }

    Process cur;
    bool running = 0;
    int idle_done = 0;
    int i = 1;
    qsort(arr+1, n, sizeof(Process), cmp_s);
    for (int T=0;T<=TIME_MAX;T++) {
        while (i <= n && arr[i].s == T) {
            enpq(rq, arr[i]);
            i++;
        }
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
                ta[cur.pid] = T - cur.s + 1;
                wa[cur.pid] = ta[cur.pid] - cur.burst - cur.io_burst;
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
        if (!isEmpty_pq(rq) && !running) { // no process was running, new process starts
            running = 1;
            cur = top(rq); depq(rq);
            chart[T] = cur.pid;
            cur.cpu_left--;
            if (cur.cpu_left == 0) {
                running = 0;
                idle_done = T + context_switch;
                ta[cur.pid] = T - cur.s + 1;
                wa[cur.pid] = ta[cur.pid] - cur.burst - cur.io_burst;
                continue;
            }
        }
    }
    display_Gantt(chart);
    display_eval(ta, wa);
}
void rr(int *chart, int *ta, int *wa, int tq) {
    Queue* rq = initqueue();
    Queue* wq = initqueue();
    for (int i=1;i<=n;i++) {
        arr[i] = arr2[i];
    }

    Process cur;
    bool running = 0;
    int idle_done = 0;
    int i = 1;
    qsort(arr+1, n, sizeof(Process), cmp_s);
    int t = tq;
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
                t--;
                chart[T] = cur.pid;
            }
            if (cur.cpu_left == 0) {
                running = 0;
                idle_done = T + context_switch;
                ta[cur.pid] = T - cur.s + 1;
                wa[cur.pid] = ta[cur.pid] - cur.burst - cur.io_burst;
                t = tq;
                continue;
            }
            else if (t == 0) { // time quantum over, but the process isn't over
                running = 0;
                idle_done = T + context_switch;
                enqueue(rq, cur);
                t = tq;
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
        }
        */
        if (!isEmpty(rq) && front(rq).s <= T && !running) { // no process was running, new process starts
            running = 1;
            cur = front(rq); dequeue(rq);
            chart[T] = cur.pid;
            cur.cpu_left--;
            t--;
            if (cur.cpu_left == 0) {
                running = 0;
                idle_done = T + context_switch;
                ta[cur.pid] = T - cur.s + 1;
                wa[cur.pid] = ta[cur.pid] - cur.burst - cur.io_burst;
                t = tq;
                continue;
            }
            else if (t == 0) { // time quantum over, but the process isn't over
                running = 0;
                idle_done = T + context_switch;
                enqueue(rq, cur);
                t = tq;
                continue;
            }
        }
    }
    display_Gantt(chart);
    display_eval(ta, wa);
}
void preemptive_sjf(int *chart, int *ta, int *wa) {
    PriorityQueue* rq = initpq(cmp_left);
    Queue* wq = initqueue();
    for (int i=1;i<=n;i++) {
        arr[i] = arr2[i];
    }

    Process cur;
    bool running = 0;
    int idle_done = 0;
    int i = 1;
    qsort(arr+1, n, sizeof(Process), cmp_s);
    for (int T=0;T<=TIME_MAX;T++) {
        while (i <= n && arr[i].s == T) {
            enpq(rq, arr[i]);
            i++;
        }
        /*
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
        */
        if (!isEmpty_pq(rq)) { // select process with shortest remaining time
            running = 1;
            if (top(rq).pid == cur.pid) { // no context switch
                chart[T] = cur.pid;
                cur.cpu_left--;
                depq(rq);
            }
            else {
                cur = top(rq);
                depq(rq);
                idle_done = T + context_switch;
                if (T == idle_done) {
                    chart[T] = cur.pid;
                    cur.cpu_left--;
                }
            }
            if (cur.cpu_left == 0) {
                running = 0;
                idle_done = T + context_switch;
                ta[cur.pid] = T - cur.s + 1;
                wa[cur.pid] = ta[cur.pid] - cur.burst - cur.io_burst;
                continue;
            }
            else {
                cur.r = 1;
                enpq(rq, cur);
            }
        }
    }
    display_Gantt(chart);
    display_eval(ta, wa);
}
void preemptive_priority(int *chart, int *ta, int *wa) {
    PriorityQueue* rq = initpq(cmp_p);
    Queue* wq = initqueue();
    for (int i=1;i<=n;i++) {
        arr[i] = arr2[i];
    }

    Process cur;
    bool running = 0;
    int idle_done = 0;
    int i = 1;
    for (int T=0;T<=TIME_MAX;T++) {
        while (i <= n && arr[i].s == T) {
            enpq(rq, arr[i]);
            i++;
        }
        /*
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
        */
        if (!isEmpty_pq(rq)) { // select process with highest priority
            running = 1;
            if (top(rq).pid == cur.pid) { // no context switch
                chart[T] = cur.pid;
                cur.cpu_left--;
            }
            else {
                cur = top(rq);
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
                depq(rq);
                ta[cur.pid] = T - cur.s + 1;
                wa[cur.pid] = ta[cur.pid] - cur.burst - cur.io_burst;
                continue;
            }
        }
    }
    display_Gantt(chart);
    display_eval(ta, wa);
}
int main()
{
    srand(time(NULL));
    int choice;
    printf("Input 1 for random process creation, 2 for manual process creation, 3 for test\n");
    printf("Choice: ");
    scanf("%d",&choice);
    if (choice == 1) {
        n = rand() % MAX_PROCESS;
        printf("%d\n",n);
        for (int i=1;i<=n;i++) {
            int b = 1 + (rand() % 10); // burst
            int a = 1 + (rand() % 10); // arrival
            int p = 1 + (rand() % 10);
            arr[i] = (Process){i, b, a, 0, 0, b, 0, 0, p};
            printf("%d %d %d %d\n",arr[i].pid,arr[i].s,arr[i].burst,arr[i].p);
        }
    }
    else if (choice == 2) {
        printf("Input number of processes: ");
        scanf("%d",&n);
        printf("Input data: ");
        for (int i=1;i<=n;i++) {
            arr[i].pid = i;
            arr[i].r = 0;
            scanf("%d %d %d %d %d %d %d",&arr[i].burst, &arr[i].s, &arr[i].io_burst, &arr[i].io_s, &arr[i].cpu_left, &arr[i].io_left, &arr[i].p);
        }
    }
    else {
        n = 5;
        arr[1] = (Process){1, 2, 0, 0, TIME_MAX, 2, 0, 0, 2};
        arr[2] = (Process){2, 1, 0, 0, TIME_MAX, 1, 0, 0, 1};
        arr[3] = (Process){3, 8, 0, 0, TIME_MAX, 8, 0, 0, 4};
        arr[4] = (Process){4, 4, 0, 0, TIME_MAX, 4, 0, 0, 2};
        arr[5] = (Process){5, 5, 0, 0, TIME_MAX, 5, 0, 0, 3};
    }
    initScheduler();
    fcfs(fcfs_chart, fcfs_ta, fcfs_wa);
    puts("");
    sjf(sjf_chart, sjf_ta, sjf_wa);
    puts("");
    priority(priority_chart, priority_ta, priority_wa);
    puts("");
    rr(rr_chart, rr_ta, rr_wa, 2);
    puts("");
    preemptive_sjf(preemptive_sjf_chart, preemptive_sjf_ta, preemptive_sjf_wa);
    puts("");
    preemptive_priority(preemptive_priority_chart, preemptive_priority_ta, preemptive_priority_wa);
    puts("");
}
