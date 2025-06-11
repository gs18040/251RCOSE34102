#include <bits/stdc++.h>

using namespace std;

int main()
{
    freopen("case.txt", "w", stdout);
    printf("2\n4\n150\n8\n20 10 1\n50 0 1\n50 0 1\n50 0 1\n");
    for (int i=0;i<50;i++) {
        printf("2 1 %d 0\n3 1 %d 1\n4 1 %d 2\n",i,i,i);
    }
}