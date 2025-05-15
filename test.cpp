#include <bits/stdc++.h>

#define TIME_MAX 100000
#define MAX_PROCESS 1000000
#define clk 1
#define context_switch 0
using namespace std;
struct Process {
    unsigned int pid;
    unsigned int burst, s; // burst time, start(arrival) time
    unsigned int io_burst, io_s;
    unsigned int cpu_left, io_left;
    int p; // priority
};
queue<Process> wq, rq;
queue<Process> ioq[101];
Process arr[MAX_PROCESS];
int fcfs_chart[TIME_MAX];
unsigned int T;
int n;
void init() {
    arr[1] = {1, 2, 0, 0, TIME_MAX, 2, 0, 2};
    arr[2] = {2, 1, 0, 0, TIME_MAX, 1, 0, 1};
    arr[3] = {3, 8, 0, 0, TIME_MAX, 8, 0, 4};
    arr[4] = {4, 4, 0, 0, TIME_MAX, 4, 0, 2};
    arr[5] = {5, 5, 0, 0, TIME_MAX, 5, 0, 3};
    for (int i=1;i<=5;i++) rq.push(arr[i]);
}
void display_Gantt() {
    for (int i=0;i<=200;i+=clk) {
        printf("%d ",fcfs_chart[i]);
    }
}
void fcfs() {
    Process cur;
    bool running = 0;
    double idle_done = 0;
    for (T=0;T<=TIME_MAX;T+=clk) {
        if (running) {
            if (cur.io_s == T) { // I/O start
                running = 0;
                wq.push(cur);
                idle_done = T + context_switch;
                fcfs_chart[T] = 0;
            }
            else {
                cur.cpu_left -= clk;
                fcfs_chart[T] = cur.pid;
            }
            if (cur.cpu_left == 0) {
                running = 0;
                idle_done = T + context_switch;
                printf("done: %u\n", T);
                continue;
            }
        }
        if (T == idle_done && !running) { // Context switch over
            if (!wq.empty() && wq.front().s <= T) {
                running = 1;
                idle_done = 0;
                rq.push(wq.front());
                wq.pop();
                cur = rq.front(); rq.pop();
                fcfs_chart[T] = cur.pid;
            }
        }
        if (!rq.empty() && rq.front().s <= T && !running) { // no process was running, new process starts
            running = 1;
            printf("%u\n",T);
            cur = rq.front(); rq.pop();
            fcfs_chart[T] = cur.pid;
            cur.cpu_left -= clk;
            if (cur.cpu_left == 0) {
                running = 0;
                idle_done = T + context_switch;
                printf("done: %u\n", T);
                continue;
            }
        }
    }
}
void sjf() {

}
void priority() {

}
void rr() {

}
void preemptive_sjf() {

}
void preemptive_priority() {

}
int main()
{
    init();
    fcfs();
    display_Gantt();
}