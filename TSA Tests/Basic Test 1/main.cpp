//
//  main.cpp
//  UFOTestFiles
//
//  Created by aser on 7/24/19.
//  Copyright © 2019 aser. All rights reserved.
//

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <iostream>

#include "Test2.h"
#include "Test3.h"

pthread_mutex_t l;
long long startNum = 2;
int *a = (int *)malloc(sizeof(int));
int y = 0;
Test3 test3;
void deallocate(void *z) {
    free(z);
}

void *productcalculator(void* arg)
{
    long long *limit_ptr = (long long*) arg;
    long long limit = *limit_ptr;
    for (long long i = 1; i <= limit+2; i++){
        startNum *= i;
    }
    pthread_mutex_lock(&l);
        if (y == 0)
            *a = 1; //POSSIBILITY OF UAF
    pthread_mutex_unlock(&l);

    printf("%llu", startNum);
    pthread_exit(NULL);

}

void *sumcalculator(void* arg){
    long long *limit_ptr2 = (long long*) arg;
    long long limit = *limit_ptr2;
    for (long long i = 1; i <= limit+2; i++){
        startNum += i;
    }
    pthread_mutex_lock(&l);
        deallocate(a);
    pthread_mutex_unlock(&l);
    pthread_mutex_lock(&l);
        y++;
    pthread_mutex_unlock(&l);
    printf("%llu\n", startNum);
    Test3::printSomethingElse(); //SHARED METHOD IN SHARED FILE
    pthread_exit(NULL);
}

void *factorialCalculator(void* arg){
    long long num = 0;
    long long *limit_ptr3 = (long long*) arg;
    long long limit = *limit_ptr3;
    for (long long i = limit+2; i >= 0; i--){
        num += i*(i-1);
    }
    Test3::printSomethingElse(); //SHARED METHOD IN SHARED FILE
    pthread_exit(NULL);
}

void printSomething(){
    std::cout << "Unshared Method\n";
}

int main(int argc, const char* argv[]){
    // insert code here...

    long long limit = (long long)atoi(argv[1]);

    //Thread ID:
    pthread_t tid,tid2,tid3;

    //Create attributes
    pthread_attr_t attr,attr2,attr3;
    pthread_attr_init(&attr);

    pthread_create(&tid, NULL, productcalculator, &limit);
    pthread_create(&tid2, NULL, sumcalculator, &limit);
    pthread_create(&tid3, NULL, factorialCalculator, &limit);
    //wait until threads are done

    pthread_join(tid,NULL);
    pthread_join(tid2, NULL);
    pthread_join(tid3, NULL);
    printf("%llu\n", startNum);

    int x = 5;
    Test2::increase(x);     //UNSHARED FILE
    printSomething();       //UNSHARED METHOD
    Test3::doNothing();     //UNSHARED METHOD in SHARED FILE
    Test3::increase(a);    //SHARED VARIABLE and UNSHARED VARIABLE IN METHOD TEST

    return 0;

}