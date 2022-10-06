//
// Created by ufo on 8/20/19.
//
#include "Test3.h"
#include <iostream>

int x=5;
void Test3::printSomethingElse(){
    x++;
    std::cout <<"Shared Method\n";
}

void Test3::doNothing(){

}

void Test3::increase(int* a){
    a++;
    int q = 0;
    q++;
}