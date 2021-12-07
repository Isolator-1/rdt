/**
 * @file test_io.cpp
 * @author zqs
 * @brief 测试缓存大小对于文件读写的影响，不计open和close的时间
 * @version 0.1
 * @date 2021-12-07
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include <stdio.h>
#include <fstream>
#include <string.h>
#include <chrono>

using namespace std::chrono;

int main()
{
    system_clock::time_point beg;
    system_clock::time_point end;
    char inputfile[64] = "input/3.jpg";
    char outputfile[64] = "output/3.jpg";
    int size;
    printf("%-12s%-12s%-12s%-12s\n", "bufSize(B)", "total(us)", "times", "average(us)");
    for(size = 64; size <= 262144; size*=2){
        std::ifstream fin(inputfile, std::ios::binary);
        std::ofstream fout(outputfile, std::ios::binary);
        char* buf = new char[size];
        memset(buf, 0, size);
        beg = system_clock::now();
        int times = 0;
        while (fin)
        {
            fin.read(buf, size);
            fout.write(buf, fin.gcount());
            times++;
        }
        end = system_clock::now();
        auto duration = duration_cast<microseconds>(end - beg);
        double costTime = duration.count();
        printf("%-12d%-12.2f%-12d%-12.2f\n", size, costTime, times, costTime/times);
        fin.close();
        fout.close();
        delete[] buf;
    }
}