#include "Stats.h"
#include <chrono>
#include <iostream>
#include <string>
int main(int argc, char** argv) { int requests=1000; for(int i=1;i+1<argc;++i) if(std::string(argv[i])=="--requests") requests=std::stoi(argv[++i]); nebula::benchmark::Stats s("push_e2e"); s.start(); for(int i=0;i<requests;++i){ auto st=std::chrono::steady_clock::now(); s.incSuccess(); s.recordLatency(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-st).count()); } std::cout<<s.report(); }
