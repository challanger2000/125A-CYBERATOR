#include "support/GridWitness.hpp"
#include <cstdlib>
#include <iostream>
using namespace cyberator::test;
void check(bool b,const char* m){if(!b){std::cerr<<m<<'\n';std::exit(1);}}
int main(){
    const auto a=frozenGrid(8000,60,0,2000,81,.5f);
    check(a[0]==0&&a[40]==.5f&&a[80]==0&&a[1040]==.5f,"exact integer grid and Hann peaks");
    const auto b=frozenGrid(8000,60,-.03125,2000,81,.5f);
    check(b[289]<.5f&&b[290]==.5f&&b[1290]==.5f,"negative musical origin shifts grains 250 samples");
    check(a!=b,"absolute position distinction");
    const auto c=frozenGrid(8000,60,0,2000,81,.5f,1);
    check(c[40]==0&&c[1040]==.5f,"initial disallowed tick removed");
    bool rejected=false;try{(void)frozenGrid(8000,60,0,2000,1001,.5f);}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"overlapping grains not misrepresented");
    std::cout<<"Independent frozen grid witness PASS\n";
}
