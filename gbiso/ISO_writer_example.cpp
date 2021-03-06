//
// Created by vladkanash on 10/26/15.
//

#include "ISOWriter.h"

int main(int argc, char* argv[]) {

    if (argc < 3)
    {
        printf("Too few arguments! (need 3 or more)\n");
        return 1;
    }

    //List of input folders/files
    ISOWriter writer(argv, argc-1);

    //Name of the output .iso file
    writer.write_iso(argv[argc-1]);
    return 0;
}