#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
using namespace std;

const string STORAGE1 = "STORAGE1";
const string STORAGE2 = "STORAGE2";
const string STORAGE3 = "STORAGE3";
const string MAIL1 = "MAIL1";
const string MAIL2 = "MAIL2";
const string USER = "USER";

void cleanLog(string serverTablet){
  string rmRecover = "log/recoverLog" + serverTablet + ".txt";
  string rmKeyValue = "log/keyValuesLog" + serverTablet + ".txt";
  ofstream tempRecover;
  ofstream tempKeyValue;
  remove(rmRecover.c_str());
  remove(rmKeyValue.c_str());
  tempRecover.open(rmRecover, ios::app);
  tempKeyValue.open(rmKeyValue, ios::app);
  tempKeyValue.close();
  tempRecover.close();
}

int main(int argc, char *argv[]){
  cleanLog(STORAGE1);
  cleanLog(STORAGE2);
  cleanLog(STORAGE3);
  cleanLog(MAIL1);
  cleanLog(MAIL2);
  cleanLog(USER);
}
