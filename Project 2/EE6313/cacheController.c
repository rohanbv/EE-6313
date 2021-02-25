/**
  *************************************************************************************************
  * @file    cacheController.c
  * @author  Rohan B.V
  * @version V1.0
  * @brief   cache profiling for a 32bit processor with 32bit data bus with different parameters
             such as Associativity, Burst Length and Write strategies
  **************************************************************************************************
*/

#include<stdio.h>
#include<stdint.h>
#include<stdbool.h>
#include<stdlib.h>
#include<math.h>
#include "helper.h"


//#define DEBUG
//#define LOG

#define CACHESIZE (128*1024)
#define BUSWIDTHINBYTES 4
#define BUSWIDTHINBITS 32
#define MAXIMUM_LINES (32*1024)
#define MAXIMUM_ASSOCIATIVITY  16
#define WTA 1
#define WTNA 2
#define WB 0
#define NO_HIT 0xFF
#define LEAST_RECENTLY_USED N-1

//Variables to hold cache status
uint32_t tagInCache[MAXIMUM_LINES][MAXIMUM_ASSOCIATIVITY];
bool     cacheLineValid[MAXIMUM_LINES][MAXIMUM_ASSOCIATIVITY];
uint8_t  cacheLRU[MAXIMUM_LINES][MAXIMUM_ASSOCIATIVITY];
bool     cacheLineDirty[MAXIMUM_LINES][MAXIMUM_ASSOCIATIVITY];


//Matrices
float a[256][256];
float z[256][256];
float pivot[256][256];
float b[256];
float x[256];
float p[256];

//performance Counters
typedef struct counters
{
    uint64_t readMemoryCount;
    uint64_t rlCount;
    uint64_t rlHitCount;
    uint64_t rlMissCount;
    uint64_t rlMissDirtyCount;
    uint64_t readDirtyWriteToMemCount;
    uint64_t readNotHitReadFromMemCount;
    uint64_t totalBytesRead;

    uint64_t wlCount;
    uint64_t writeMemoryCount;
    uint64_t writeToMemoryForWtaWtna;
    uint64_t wlMissCount;
    uint64_t wlHitCount;
    uint64_t wlMissDirtyCount;
    uint64_t writeMissReadFromMem;
    uint64_t cpuWriteToCacheBlock;
    uint64_t totalBytesWriten;
    uint64_t flushCount;
}__Counters;

__Counters perfCounter;

//Function to reset cache and zero performance counters
void resetCache()
{
     //Clear the profiling counters
     perfCounter.readMemoryCount = 0;
     perfCounter.rlCount = 0;
     perfCounter.rlHitCount = 0;
     perfCounter.rlMissCount = 0;
     perfCounter.rlMissDirtyCount = 0;
     perfCounter.readDirtyWriteToMemCount = 0;
     perfCounter.readNotHitReadFromMemCount = 0;
     perfCounter.totalBytesRead = 0;
     perfCounter.wlCount = 0;
     perfCounter.writeMemoryCount = 0;
     perfCounter.writeToMemoryForWtaWtna = 0;
     perfCounter.wlMissCount = 0;
     perfCounter.wlHitCount = 0;
     perfCounter.wlMissDirtyCount = 0;
     perfCounter.writeMissReadFromMem = 0;
     perfCounter.cpuWriteToCacheBlock = 0;
     perfCounter.totalBytesWriten = 0;
     perfCounter.flushCount = 0;
     //Clear the cache
     for(int i = 0;i < MAXIMUM_LINES;i++)
        for(int j = 0;j < MAXIMUM_ASSOCIATIVITY;j++)
        {
            cacheLineDirty[i][j] = false;
            cacheLineValid[i][j] = false;
            tagInCache[i][j] = 0;
            cacheLRU[i][j] = j;
        }
}

//Function to flush all the values in cache
void flushCache()
{
    for(uint32_t i = 0;i < MAXIMUM_LINES;i++)
    {
        for(uint32_t j = 0;j < MAXIMUM_ASSOCIATIVITY;j++)
        {
            if(cacheLineDirty[i][j])
            {
                perfCounter.flushCount++;
                cacheLineDirty[i][j] = false;
            }
        }
    }
}

//Function to get Line from given address
uint32_t getLine(uint32_t address,uint8_t burstLength,uint8_t N)
{
    #ifdef DEBUG
    printInBits((uint32_t)&address,sizeof(address));
    #endif // DEBUG
    uint8_t blockSize = burstLength * BUSWIDTHINBYTES;
    uint8_t b = log2(blockSize);
    uint32_t totalLines = CACHESIZE/(N*blockSize);
    uint8_t l = log2(totalLines);
    uint32_t line = address >> b;
    line = line & (totalLines-1);
    #ifdef DEBUG
    printInBits((uint32_t)&line,sizeof(line));
    printf("line: %d\n",l);
    #endif // DEBUG
    return line;
}

//Function to get tag from given address
uint32_t getTag(uint32_t address,uint8_t burstLength,uint8_t N)
{
    #ifdef DEBUG
    printInBits((uint32_t)&address,sizeof(address));
    #endif // DEBUG
    uint8_t blockSize = burstLength * BUSWIDTHINBYTES;
    uint8_t b = log2(blockSize);
    uint32_t totalLines = CACHESIZE/(N*blockSize);
    uint8_t l = log2(totalLines);
    uint8_t t = BUSWIDTHINBITS - l - b;
    uint32_t tag = address >> (l+b);
    uint32_t totalTags = pow(2,t);
    tag = tag & (totalTags-1);
    #ifdef DEBUG
    printInBits((uint32_t)&tag,sizeof(tag));
    printf("line: %d\n",l);
    printf("tag: %d\n",t);
    #endif // DEBUG
    return tag;
}

//Function to check if tag is already present in certain set in that line of cache and is valid
int8_t checkIfHit(uint32_t line,uint32_t tag,uint8_t N)
{
    uint8_t i;
    for(i = 0;i < N;i++)
    {
        if((tagInCache[line][i] == tag) && cacheLineValid[line][i] == true)
        {
            return i;
        }
    }
    return NO_HIT;
}

//Function to Update LRU table
void updateLRU(uint32_t line,uint8_t N,uint8_t hitAtSet)
{
    for(uint8_t i = 0;i < N;i++)
    {
        if(cacheLRU[line][i] < cacheLRU[line][hitAtSet])
            cacheLRU[line][i]++;
    }
    //Set the set that has been hit as the least recently used
    cacheLRU[line][hitAtSet] = 0;
}

//Function to update tag for cache miss for read
void setTag(uint32_t line,uint32_t tag,uint8_t set)
{
    tagInCache[line][set] = tag;
}

//Function to find the Least Recently Used Set
uint8_t getLRU(uint32_t line,uint8_t N)
{
    for(uint8_t i = 0;i < N;i++)
    {
        if(cacheLRU[line][i] == LEAST_RECENTLY_USED)
            return i;
    }
    return 0;
}
//Function to profile reads to the CPU from cache
void readLine(uint32_t line,uint32_t tag,uint8_t N)
{
    perfCounter.rlCount++;
    //check if hit in any of N sets for the Tag
    uint8_t hitAtSet,oldestSet;
    hitAtSet = checkIfHit(line,tag,N);
    bool hit = (hitAtSet != NO_HIT)? true : false;
    if(hit)
    {
        //return value in cache
        perfCounter.rlHitCount++;
        //update LRU
        updateLRU(line,N,hitAtSet);

    }
    else
    {
        perfCounter.rlMissCount++;
        //Find LRU Block and Replace with our block of data
        oldestSet = getLRU(line,N);
        //If dirty Write back to Memory
        if(cacheLineDirty[line][oldestSet])
        {
            perfCounter.rlMissDirtyCount++;
            //write block to Memory
            perfCounter.readDirtyWriteToMemCount++;
            //mark as invalid and not dirty
            cacheLineValid[line][oldestSet] = false;
            cacheLineDirty[line][oldestSet] = false;
        }
        //Read in New Block of Memory
        perfCounter.readNotHitReadFromMemCount++;
        //set block in the oldest set to valid
        cacheLineValid[line][oldestSet] = true;
        //Set Tag for new data
        setTag(line,tag,oldestSet);
        //Update LRU
        updateLRU(line,N,oldestSet);
    }
}

//Function to profile the reads from the  pointed memory add of Size sizeInBytes with different cache placement parameters
void readMemory(void* add,int sizeInBytes,uint8_t burstLength,uint8_t N)
{
    perfCounter.readMemoryCount++;
    uint32_t address = (uint32_t)add;
    int oldline = -1;
    uint32_t line,tag;
    for(int i = 0;i < sizeInBytes; i++)
    {
        line = getLine(address,burstLength,N);
        tag  = getTag(address,burstLength,N);
        if(line != oldline)
        {
            oldline = line;
            readLine(line,tag,N);
        }
        address++;
    }
    perfCounter.totalBytesRead = perfCounter.totalBytesRead + sizeInBytes;
}

void writeLine(uint32_t line,uint32_t tag,uint8_t N,uint8_t writeStrategy)
{
    perfCounter.wlCount++;
    uint8_t hitAtSet,oldestSet;
    hitAtSet = checkIfHit(line,tag,N);
    bool hit = (hitAtSet != NO_HIT)? true : false;

    //Always writing to Memory if WTA or WTNA
    if((writeStrategy == WTA) ||(writeStrategy == WTNA))
    {
        //write to Memory
        perfCounter.writeToMemoryForWtaWtna++;
    }

    //If its a miss and write through allocate or write back
    if(((writeStrategy == WB) || (writeStrategy == WTA)) && !hit)
    {
        perfCounter.wlMissCount++;
        //Find oldest block to replace
        oldestSet = getLRU(line,N);
        if(cacheLineDirty[line][oldestSet])
        {
            //if dirty write block to memory
            perfCounter.wlMissDirtyCount++;
            cacheLineValid[line][oldestSet] = false;
            cacheLineDirty[line][oldestSet] = false;
        }
        //read in new block of memory
        perfCounter.writeMissReadFromMem++;
        //set block in the oldest set to valid
        cacheLineValid[line][oldestSet] = true;
        //Set Tag for new data
        setTag(line,tag,oldestSet);
        //Update LRU
        updateLRU(line,N,oldestSet);
        //CPU write to cache Block
        perfCounter.cpuWriteToCacheBlock++;
        if(writeStrategy == WB)
            cacheLineDirty[line][oldestSet] = true;
    }

    //If its a hit
    if(hit)
    {
        perfCounter.wlHitCount++;
        updateLRU(line,N,hitAtSet);
        //CPU write to cache Block
        perfCounter.cpuWriteToCacheBlock++;
        if(writeStrategy == WB)
            cacheLineDirty[line][hitAtSet] = true;
    }
}
void writeMemory(void* add,int sizeInBytes,uint8_t burstLength,uint8_t N,uint8_t writeStrategy)
{
    perfCounter.writeMemoryCount++;
    uint32_t address = (uint32_t)add;
    int oldline = -1;
    for(int i = 0;i < sizeInBytes;i++)
    {
        uint32_t line = getLine(address,burstLength,N);
        uint32_t tag  = getTag(address,burstLength,N);
        if(line != oldline)
        {
            oldline = line;
            writeLine(line,tag,N,writeStrategy);
        }
        address++;
    }
    perfCounter.totalBytesWriten = perfCounter.totalBytesWriten + sizeInBytes;
}



void choldc(float a[256][256], int n, float p[256],uint8_t BL,uint8_t N,uint8_t writeStrategy)
{
	int i,j,k;
	float sum;

	writeMemory(&i,sizeof(i),BL,N,writeStrategy);
	readMemory(&n,sizeof(n),BL,N);
	for (i=0;i<n;i++)
	{
	    readMemory(&i,sizeof(i),BL,N);
	    writeMemory(&j,sizeof(j),BL,N,writeStrategy);
	    readMemory(&n,sizeof(n),BL,N);
		for (j=i;j<=n;j++)
		{
		    readMemory(&i,sizeof(i),BL,N);
		    readMemory(&j,sizeof(j),BL,N);
		    readMemory(&a[i][j],sizeof(a[i][j]),BL,N);
		    writeMemory(&sum,sizeof(sum),BL,N,writeStrategy);
		    writeMemory(&k,sizeof(k),BL,N,writeStrategy);
			for (sum=a[i][j],k=i-1;k>=1;k--)
			{
                readMemory(&sum,sizeof(sum),BL,N);
                readMemory(&i,sizeof(i),BL,N);
                readMemory(&j,sizeof(j),BL,N);
                readMemory(&k,sizeof(k),BL,N);
                readMemory(&a[i][k],sizeof(a[i][k]),BL,N);
                readMemory(&a[j][k],sizeof(a[j][k]),BL,N);
                writeMemory(&sum,sizeof(sum),BL,N,writeStrategy);
                sum -= a[i][k]*a[j][k];
                readMemory(&k,sizeof(k),BL,N);
                writeMemory(&k,sizeof(k),BL,N,writeStrategy);
			}
			readMemory(&j,sizeof(j),BL,N);
			readMemory(&i,sizeof(i),BL,N);
				if (i == j)
				{
				    readMemory(&sum,sizeof(sum),BL,N);
					if (sum <= 0.0)
                    {
                        printf("Error\n");
                    }
                    readMemory(&sum,sizeof(sum),BL,N);
                    readMemory(&i,sizeof(i),BL,N);
                    writeMemory(&p[i],sizeof(p[i]),BL,N,writeStrategy);
					p[i]=sqrt(sum);
				}
				else
				{
				    readMemory(&sum,sizeof(sum),BL,N);
				    readMemory(&i,sizeof(sum),BL,N);
				    readMemory(&p[i],sizeof(p[i]),BL,N);
				    readMemory(&j,sizeof(j),BL,N);
				    writeMemory(&a[j][i],sizeof(a[j][i]),BL,N,writeStrategy);
					a[j][i]=sum/p[i];
				}
				readMemory(&j,sizeof(j),BL,N);
				readMemory(&n,sizeof(n),BL,N);
				writeMemory(&j,sizeof(j),BL,N,writeStrategy);
		}
		readMemory(&i,sizeof(i),BL,N);
		writeMemory(&i,sizeof(i),BL,N,writeStrategy);
		readMemory(&n,sizeof(n),BL,N);
	}
}

void cholsl(float a[256][256], int n, float p[256], float b[256], float x[256],uint8_t BL,uint8_t N,uint8_t writeStrategy)
{
	int i,k;
	float sum;

	readMemory(&n,sizeof(n),BL,N);
	writeMemory(&i,sizeof(i),BL,N,writeStrategy);
	for (i=0;i<n;i++)
	{
	    readMemory(&i,sizeof(i),BL,N);
	    readMemory(&b[i],sizeof(b[i]),BL,N);
	    writeMemory(&sum,sizeof(sum),BL,N,writeStrategy);
	    readMemory(&i,sizeof(i),BL,N);
	    writeMemory(&k,sizeof(k),BL,N,writeStrategy);
		for (sum=b[i],k=i-1;k>=1;k--)
			{
                readMemory(&sum,sizeof(sum),BL,N);
                readMemory(&i,sizeof(i),BL,N);
                readMemory(&k,sizeof(k),BL,N);
                readMemory(&x[k],sizeof(x[k]),BL,N);
                readMemory(&a[i][k],sizeof(a[i][k]),BL,N);
                writeMemory(&sum,sizeof(sum),BL,N,writeStrategy);
                sum -= a[i][k]*x[k];
                readMemory(&k,sizeof(k),BL,N);
                writeMemory(&k,sizeof(k),BL,N,writeStrategy);
			}
        readMemory(&i,sizeof(i),BL,N);
        readMemory(&p[i],sizeof(p[i]),BL,N);
        readMemory(&sum,sizeof(sum),BL,N);
        writeMemory(&x[i],sizeof(x[i]),BL,N,writeStrategy);
		x[i]=sum/p[i];
		readMemory(&i,sizeof(i),BL,N);
		readMemory(&n,sizeof(n),BL,N);
		writeMemory(&i,sizeof(i),BL,N,writeStrategy);
	}
    readMemory(&n,sizeof(n),BL,N);
	writeMemory(&i,sizeof(i),BL,N,writeStrategy);
	for (i=n;i>=1;i--)
	{
	    readMemory(&i,sizeof(i),BL,N);
	    readMemory(&x[i],sizeof(x[i]),BL,N);
	    writeMemory(&sum,sizeof(sum),BL,N,writeStrategy);
	    readMemory(&i,sizeof(i),BL,N);
        readMemory(&n,sizeof(n),BL,N);
        writeMemory(&k,sizeof(k),BL,N,writeStrategy);
		for (sum=x[i],k=i+1;k<=n;k++)
		{
		    readMemory(&k,sizeof(k),BL,N);
		    readMemory(&x[k],sizeof(x[k]),BL,N);
		    readMemory(&i,sizeof(i),BL,N);
		    readMemory(&a[k][i],sizeof(a[k][i]),BL,N);
		    readMemory(&sum,sizeof(sum),BL,N);
		    writeMemory(&sum,sizeof(sum),BL,N,writeStrategy);
            sum -= a[k][i]*x[k];
            readMemory(&k,sizeof(k),BL,N);
            readMemory(&n,sizeof(n),BL,N);
		    writeMemory(&k,sizeof(k),BL,N,writeStrategy);
		}
		readMemory(&i,sizeof(i),BL,N);
		readMemory(&p[i],sizeof(p[i]),BL,N);
		readMemory(&sum,sizeof(sum),BL,N);
		writeMemory(&x[i],sizeof(x[i]),BL,N,writeStrategy);
		x[i]=sum/p[i];
		readMemory(&i,sizeof(i),BL,N);
		writeMemory(&i,sizeof(i),BL,N,writeStrategy);
	}
}

void createMatrixToBeDecomposed()
{
    for(int i = 0;i < 256;i++)
    {
        for(int j = 0;j <256;j++)
        {

            if(i == j)
            {
                a[i][j] = 1;
            }
        }
    }
    for(int i = 0;i < 256;i++)
    {
        pivot[i] = 1;
        b[i] = 1;
    }
}

void updateCSV(FILE *fp,uint8_t BL,uint8_t N,uint8_t writeStrategy)
{
    fprintf(fp,"%d,",BL);
    fprintf(fp,"%d,",N);
    fprintf(fp,"%d,",writeStrategy);
    fprintf(fp,"%lu,",perfCounter.readMemoryCount);
    fprintf(fp,"%lu,",perfCounter.rlCount);
    fprintf(fp,"%lu,",perfCounter.rlHitCount);
    fprintf(fp,"%lu,",perfCounter.rlMissCount);
    fprintf(fp,"%lu,",perfCounter.rlMissDirtyCount);
    fprintf(fp,"%lu, ,",perfCounter.totalBytesRead);
    fprintf(fp,"%lu,",perfCounter.writeMemoryCount);
    fprintf(fp,"%lu,",perfCounter.wlCount);
    fprintf(fp,"%lu,",perfCounter.wlMissCount);
    fprintf(fp,"%lu,",perfCounter.wlHitCount);
    fprintf(fp,"%lu,",perfCounter.wlMissDirtyCount);
    fprintf(fp,"%lu,",perfCounter.writeToMemoryForWtaWtna);
    fprintf(fp,"%lu, ,",perfCounter.totalBytesWriten);
    fprintf(fp,"%lu,\n",perfCounter.flushCount);
}


int main()
{
    FILE *fp;
    fp = fopen("testperfCounters.csv","w+");
    fprintf(fp,"BL,N,WS,rdMemoryCount,rlCount,rlHitCount,rlMissCount,rlMissDirtyCount,Total Bytes Read, ,writeMemoryCount,wlCount,wlMissCount,wlHitCount,wlMissDirtyCount,writeThroughCount,Total bytes written,,Flush Cache Count\n");
    for(uint8_t N = 1;N <= 16;N = N*2)
    {
        for(uint8_t BL = 1;BL <= 8;BL = BL*2)
          {
            for(uint8_t writeStrategy = 0;writeStrategy < 3;writeStrategy++)
            {
                resetCache();
                createMatrixToBeDecomposed();
                choldc(a,256,p,BL,N,writeStrategy);
                cholsl(a,256,p,b,x,BL,N,writeStrategy);
                flushCache();
#ifdef LOG
                printf("Running: %d  ",writeStrategy);
                printf("%d  ",BL);
                printf("%d  \n",N);
                updateCSV(fp,BL,N,writeStrategy);
#endif // LOG
            }
        }
    }
    return 0;
}
