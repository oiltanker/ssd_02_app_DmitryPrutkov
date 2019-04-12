#include <iostream>

#include "client.h"
#include "service.h"

using namespace std;

int main(int argc, char** argv) {
    //vector<Image> images = getInterestings();
    //for(Image& img: images) cout << img << "\n\n";

    int code = runService();
    cout << "\nService code: " << code << endl;

    return 0;
}