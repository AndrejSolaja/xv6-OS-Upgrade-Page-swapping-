// Test that fork fails gracefully.
// Tiny executable so that the limit can be filling the proc table.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define N  1000

void
print(const char *s)
{
    write(1, s, strlen(s));
}

void f(int n){
    char* a = sbrk(4096 * 100);

    for(int i = 0; i < 5; i++){
        for(int j = 0; j < 100; j++){
            a[j * 4096] = 'a';
            char broj[] = {(n / 100) % 10 + '0', (n / 10) % 10 + '0',
                           n % 10 + '0', '\n'};
            print(broj);
        }
        //sleep(1);
    }
}

void
forktest(void)
{
    int n, pid;

    print("fork test\n");

    for(n=0; n<N; n++){
        pid = fork();
        if(pid < 0)
            break;
        if(pid == 0) {
            f(n);
            exit(0);
        }
    }

    print("ovde\n");

    if(n == N){
        print("fork claimed to work N times!\n");
        exit(1);
    }

    for(; n > 0; n--){
        if(wait(0) < 0){
            print("wait stopped early\n");
            exit(1);
        }
    }

    if(wait(0) != -1){
        print("wait got too many\n");
        exit(1);
    }

    print("fork test OK\n");
}

int
main(void)
{
    forktest();
    exit(0);
}