#include <stdio.h>
#include <fstream>
#include <string.h>

#define DATA_SIZE 64

int main()
{
    char inputfile[64] = "input/test.txt";
    char outputfile[64] = "output/test.txt";
    std::ifstream fin(inputfile, std::ios::binary);
    std::ofstream fout(outputfile, std::ios::binary);
    char buf[DATA_SIZE];
    memset(buf, 0, DATA_SIZE);
    while (fin)
    {
        fin.read(buf, DATA_SIZE);
        printf("fin.gcount(): %d, strlen: %d\n", fin.gcount(), strlen(buf));
        fout.write(buf, fin.gcount());
        memset(buf, 0, DATA_SIZE);
    }
    fin.close();
}