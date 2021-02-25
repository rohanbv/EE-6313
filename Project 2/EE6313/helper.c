/******************************************************************************

Library for helping functions to Debug and Test

*******************************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

void printInBits(void* pNum,uint8_t size)
{
    bool mask = false;
    uint32_t binNum = 0;
    switch(size)
    {
        case 4 :
        printf("Num is uint32_t and Value: %d\n",*(uint32_t*)pNum);
        for(int i = 31; i >= 0; i--)
        {
            if(((*(uint32_t*)pNum) & (1<<i)))
            {
                printf("1");
            }
            else
            {
                printf("0");
            }
            if(i % 4 == 0)
            {
                printf(" ");
            }
        }
        printf("\n");
        break;

        case 1 :
        printf("Num is uint8_t and Value: %hu\n",*(uint8_t*)pNum);
        for(int i = 7; i >= 0; i--)
        {
            if(((*(uint8_t*)pNum) & (1<<i)))
            {
                printf("1");
            }
            else
            {
                printf("0");
            }
            if(i % 4 == 0)
            {
                printf(" ");
            }
        }
        printf("\n");
        break;

        case 8 :
        printf("Num is uint64_t and Value: %lu\n:",*(uint64_t*)pNum);
        for(int i = 63; i >= 0; i--)
        {
            if(((*(uint64_t*)pNum) & (1<<i)))
            {
                printf("1");
            }
            else
            {
                printf("0");
            }
            if(i % 4 == 0)
            {
                printf(" ");
            }
        }
        printf("\n");
        break;

        default:
        printf("Num is Junk\n");
    }
}

uint32_t extractBits(uint32_t Num,uint8_t Top,uint8_t Bottom)
{
    uint32_t ans = 0,botMask = 0;
    for(int i = 0; i <= Top ; i++)
    {
        botMask |= (1<<i);
    }
    ans = Num & (botMask);
    ans = ans >> Bottom;
   return ans;
}
