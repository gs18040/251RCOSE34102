#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>

#define TIME_MAX 100
#define IO_MAX 3
#define MAX_PROCESS 10
#define MAX_DEVICE 5
#define MAX_EVENT 10
#define context_switch 0

typedef struct {
    int pid; // pid
    int burst, s; // burst time, start(arrival) time
    int io_left, io_burst_sum;
    int cpu_left;
    int r;
    int p; // priority
} Process;

typedef struct {
    int idx, io_burst, io_s, io_d;
} IO_event;

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
void enqueue(Queue* q, Process* x) {
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
Process* front(Queue* q) {
    if (isEmpty(q)) {
        printf("Queue is empty!\n");
        exit(0);
    }
    return q->data[q->front];
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

// 요소 삽입 (O(log n))
void enpq(PriorityQueue* pq, Process* x) {
    // 새 요소를 끝에 추가
    pq->data[pq->size] = x;
    int i = pq->size;
    pq->size++;
    
    // 힙 속성 복구 (위로 올라가며 교환)
    while (i != 0 && pq->compare(pq->data[parent(i)], pq->data[i]) < 0) {
        swap(pq->data[i], pq->data[parent(i)]);
        i = parent(i);
    }
}

// 최우선 요소 제거 및 반환 (O(log n))
Process* depq(PriorityQueue* pq) {
    if (pq->size == 0) {
        printf("Queue is empty!\n");
        exit(0);
    }

    Process* root = pq->data[0];
    pq->data[0] = pq->data[pq->size-1];
    pq->size--;

    // 아래로 내리기(힙 속성 유지)
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

// 최우선 요소 확인 (peek)
Process* top(PriorityQueue* pq) {
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
Queue* ioq[MAX_DEVICE];
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
int fcfs_chart[TIME_MAX];
int sjf_chart[TIME_MAX];
int priority_chart[TIME_MAX];
int rr_chart[TIME_MAX];
int preemptive_priority_chart[TIME_MAX];
int preemptive_sjf_chart[TIME_MAX];
int n = 5, io_n = 0;
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
    puts("A");
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
    for (int i=1;i<=n;i++) {
        arr[i] = arr2[i];
    }

    Process* cur = NULL;
    bool running = 0;
    int idle_done = -1;
    int i = 1;
    qsort(arr+1, n, sizeof(Process), cmp_s);
    for (int T=0;T<=TIME_MAX;T++) {
        while (i <= n && arr[i].s == T) {
            enqueue(rq, &arr[i]);
            i++;
        }
        if (!cur && !isEmpty(rq)) {
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
        if (cur) {
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
            if (!isEmpty(ioq[i])) printf("%d %d %d %d\n",i,front(ioq[i])->pid,front(ioq[i])->io_left,T);
            if (!isEmpty(ioq[i])) front(ioq[i])->io_left--;
            if (!isEmpty(ioq[i]) && front(ioq[i])->io_left == 0) {
                enqueue(rq, front(ioq[i]));
                io_i[front(ioq[i])->pid]++;
                dequeue(ioq[i]);
            }
        }
    }
    display_Gantt(chart);
    display_eval(ta, wa);
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
    bool running = 0;
    int idle_done = 0;
    int i = 1;
    qsort(arr+1, n, sizeof(Process), cmp_s);
    for (int T=0;T<=TIME_MAX;T++) {
        while (i <= n && arr[i].s == T) {
            enpq(rq, &arr[i]);
            i++;
        }
        if (!cur && !isEmpty_pq(rq)) {
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
        if (cur) {
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
                enpq(rq, front(ioq[i]));
                // printf("%d %d %d %d %d\n",i,T,front(ioq[i])->pid,io_i[front(ioq[i])->pid],io_n_list[front(ioq[i])->pid]);
                dequeue(ioq[i]);
            }
        }
    }
    display_Gantt(chart);
    display_eval(ta, wa);
}
void priority(int *chart, int *ta, int *wa) {
    PriorityQueue* rq = initpq(cmp_p);
    for (int i=1;i<=n;i++) {
        arr[i] = arr2[i];
        io_i[i] = 0;
    }

    Process* cur = NULL;
    bool running = 0;
    int idle_done = 0;
    int i = 1;
    qsort(arr+1, n, sizeof(Process), cmp_s);
    for (int T=0;T<=TIME_MAX;T++) {
        while (i <= n && arr[i].s == T) {
            enpq(rq, &arr[i]);
            i++;
        }
        if (!cur && !isEmpty_pq(rq)) {
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
        if (cur) {
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
                enpq(rq, front(ioq[i]));
                // printf("%d %d %d %d %d\n",i,T,front(ioq[i])->pid,io_i[front(ioq[i])->pid],io_n_list[front(ioq[i])->pid]);
                dequeue(ioq[i]);
            }
        }
    }
    display_Gantt(chart);
    display_eval(ta, wa);
}
void rr(int *chart, int *ta, int *wa, int tq) {
    Queue* rq = initqueue();
    for (int i=1;i<=n;i++) {
        arr[i] = arr2[i];
        io_i[i] = 0;
    }

    Process* cur = NULL;
    bool running = 0;
    int idle_done = 0;
    int i = 1;
    qsort(arr+1, n, sizeof(Process), cmp_s);
    int t = tq;
    for (int T=0;T<=TIME_MAX;T++) {
        while (i <= n && arr[i].s == T) {
            enqueue(rq, &arr[i]);
            i++;
        }
        if (!cur && !isEmpty(rq)) {
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
        if (cur) {
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
    display_Gantt(chart);
    display_eval(ta, wa);
}
void preemptive_sjf(int *chart, int *ta, int *wa) {
    PriorityQueue* rq = initpq(cmp_left);
    for (int i=1;i<=n;i++) {
        arr[i] = arr2[i];
        io_i[i] = 0;
    }

    Process* cur = NULL;
    bool running = 0;
    int idle_done = 0;
    int i = 1;
    qsort(arr+1, n, sizeof(Process), cmp_s);
    for (int T=0;T<=TIME_MAX;T++) {
        while (i <= n && arr[i].s == T) {
            enpq(rq, &arr[i]);
            i++;
        }
        if (!cur && !isEmpty_pq(rq)) {
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
        if (cur) {
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
                enpq(rq, front(ioq[i]));
                // printf("%d %d %d %d %d\n",i,T,front(ioq[i])->pid,io_i[front(ioq[i])->pid],io_n_list[front(ioq[i])->pid]);
                dequeue(ioq[i]);
            }
        }
    }
    display_Gantt(chart);
    display_eval(ta, wa);
}
void preemptive_priority(int *chart, int *ta, int *wa) {
    PriorityQueue* rq = initpq(cmp_p);
    for (int i=1;i<=n;i++) {
        arr[i] = arr2[i];
        io_i[i] = 0;
    }

    Process* cur = NULL;
    bool running = 0;
    int idle_done = 0;
    int i = 1;
    qsort(arr+1, n, sizeof(Process), cmp_s);
    for (int T=0;T<=TIME_MAX;T++) {
        while (i <= n && arr[i].s == T) {
            enpq(rq, &arr[i]);
            i++;
        }
        if (!cur && !isEmpty_pq(rq)) {
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
        if (cur) {
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
                enpq(rq, front(ioq[i]));
                // printf("%d %d %d %d %d\n",i,T,front(ioq[i])->pid,io_i[front(ioq[i])->pid],io_n_list[front(ioq[i])->pid]);
                dequeue(ioq[i]);
            }
        }
    }
    display_Gantt(chart);
    display_eval(ta, wa);
}
void lottery(int *chart, int *ta, int *wa) {

}
void sjf_aging(int *chart, int *ta, int *wa) {
    
}
void mlfq(int *chart, int *ta, int *wa) {
    
}
int main()
{
    srand(time(NULL));
    int choice;
    printf("Input 1 for random process creation, 2 for manual process creation, 3 for test\n");
    printf("Choice: ");
    scanf("%d",&choice);
    if (choice == 1) {
        n = 1 + (rand() % MAX_PROCESS);
        io_n = rand() % MAX_EVENT;
        io_n = 0;
        printf("%d %d\n",n,io_n);
        for (int i=1;i<=n;i++) {
            int b = 1 + (rand() % 10); // burst
            int a = 1 + (rand() % 10); // arrival
            int p = 1 + (rand() % 10);
            arr[i] = (Process){i, b, a, -1, 0, b, 0, p};
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
        printf("Input process data: \n");
        for (int i=1;i<=n;i++) {
            arr[i].pid = i;
            arr[i].r = 0;
            scanf("%d %d %d %d",&arr[i].burst, &arr[i].s, &arr[i].cpu_left, &arr[i].p);
        }
        printf("Input I/O data: \n");
        for (int i=0;i<io_n;i++) {
            scanf("%d %d %d %d",&events[i].idx,&events[i].io_burst,&events[i].io_s,&events[i].io_d);
        }
    }
    else {
        // Test
        // pid, burst, arrival, io_end, io_burst_sum, cpu_left, r, p
        // pid, burst, arrival, device
        n = 5;
        io_n = 1;
        events[0] = (IO_event){1, 2, 1, 0};
        arr[1] = (Process){1, 2, 0, -1, 0, 2, 0, 2};
        arr[2] = (Process){2, 1, 0, -1, 0, 1, 0, 1};
        arr[3] = (Process){3, 8, 0, -1, 0, 8, 0, 4};
        arr[4] = (Process){4, 4, 0, -1, 0, 4, 0, 2};
        arr[5] = (Process){5, 5, 0, -1, 0, 5, 0, 3};
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
