#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // for getopt()
//#include <math.h>   

#define BYTES_PER_WORD 4    // 1 word = 4 Byte
//#define DEBUG

/*
 * Cache structures
 */
int time = 0;

typedef struct
{
    int age;    // LRU 
    int valid;
    int modified;   // dirty
    uint32_t tag;
} cline;

typedef struct
{ 
    cline *lines;
} cset;

typedef struct
{
    int s; // index bits
    int E; // way
    int b; // block offset bits 
    cset *sets; // tag + valid
} cache;

int index_bit(int n){   // index bit 비트수
    int cnt = 0;

    while(n) {
        cnt++;
        n = n >> 1;
    }

    return cnt-1;
}

/***************************************************************/
/*                                                             */
/* Procedure : build_cache                                     */
/*                                                             */
/* Purpose   : Initialize cache structure                      */
/*                                                             */
/* Parameters:                                                 */
/*     int S: The set of cache    16                           */
/*     int E: The associativity way of cache    8              */
/*     int b: The blocksize of cache       8                   */
/*                                                             */
/***************************************************************/
cache build_cache(int S, int E, int b) //캐시구조에 메모리를 할당- 선언된 캐시 구조와 인수 활용
{
	/* Implement this function */

    cache result_cache;
    result_cache.s = 4;    // index bits
    result_cache.E = E;    // way
    result_cache.b = 3;    // block offset bits

    result_cache.sets = malloc(sizeof(cset) * S);   // set array

    for (int i = 0; i < S; i++) {

        result_cache.sets[i].lines = malloc(sizeof(cline) * E);   // line array

        for (int j = 0; j < E; j++) {
            result_cache.sets[i].lines[j].age = 0;
            //printf("sets[%d].lines[%d].age: %d\n",i,j, result_cache.sets[i].lines[j].age);
            result_cache.sets[i].lines[j].valid = 0;
            result_cache.sets[i].lines[j].modified = 0;
            result_cache.sets[i].lines[j].tag = 0;
        }
    }
    return result_cache;

}

/***************************************************************/
/*                                                             */
/* Procedure : access_cache                                    */
/*                                                             */
/* Purpose   : Update cache stat and content                   */
/*                                                             */
/* Parameters:                                                 */
/*     cache *L: An accessed cache                             */
/*     int op: Read/Write operation                            */
/*     uint32_t addr: The address of memory access             */
/*     int *hit: The number of cache hit                       */
/*     int *miss: The number of cache miss                     */
/*     int *wb: The number of write-back                       */
/*                                                             */
/***************************************************************/
void access_cache(cache *L, char *op, uint32_t addr, int *read, int *write, int *hit, int *miss, int *wb)    // 파일을 읽고 파일의 주소를 캐시 구조에 적용
{
	/* Implement this function */
    int i = 0, j = 0, count = 0 ;
    int max_age = 0, max_waynum = 0;

    int block_offset = addr & 0b00000000000000000000000000000111;
    int index = (addr & 0b00000000000000000000000001111000) >> 3;
    int tag = (addr & 0b11111111111111111111111110000000) >> 7;
    //printf("blockoffset: %d\n", block_offset); printf("index: %d\n", index); printf("tag: %d\n", tag);

    if (strcmp(op, "R") == 0) {
        
        *read = *read + 1;

        for (j = 0; j < 8; j++) {   // 다 old 올림
            L->sets[index].lines[j].age ++;
            //printf("sets[%d].age[%d]: %d\n",index, j, L->sets[index].lines[j].age);
        }

        for (i = 0; i < 8; i++) {
            
            if ((L->sets[index].lines[i].tag == tag) & (L->sets[index].lines[i].valid == 1)) {  // tag 같음, valid = 1 -> hit
                *hit = *hit + 1;
                L->sets[index].lines[i].tag = tag;
                L->sets[index].lines[i].valid = 1;
                L->sets[index].lines[i].age = 0;
                break;
            }
            if ((L->sets[index].lines[i].tag != tag) & (L->sets[index].lines[i].valid == 0)) {  // 안에 데이터 없음
                *miss = *miss + 1;
                L->sets[index].lines[i].tag = tag;
                L->sets[index].lines[i].valid = 1;
                L->sets[index].lines[i].age = 0;
                break;
            }
            if (L->sets[index].lines[i].tag != tag & L->sets[index].lines[i].valid == 1) {  // 다른 데이터있음
                count++;
            }
        }

        if (count == 8) {   // tag다른 값들이 8개 꽉 차있음
            *miss = *miss + 1;

            max_waynum = 0;
            for (int a = 0; a < 8; a++) {   // age 가장 높은거 선정
                if (L->sets[index].lines[a].age > max_age) {
                    max_age = L->sets[index].lines[a].age;
                    max_waynum = a;
                }
            }
            if (L->sets[index].lines[max_waynum].modified == 1) {   // 업뎃된 적 있음 -> write_back
                *wb = *wb + 1;
                L->sets[index].lines[max_waynum].tag = tag;  // cache tag 바꿈
                L->sets[index].lines[max_waynum].modified = 0;   // 1인지 0인지 물어봐야함
                L->sets[index].lines[max_waynum].valid = 1;
                L->sets[index].lines[max_waynum].age = 0;

            }
            else {  // 업뎃된 적 없음
                L->sets[index].lines[max_waynum].tag = tag;  // cache tag 바꿈
                L->sets[index].lines[max_waynum].modified = 0;   // 1인지 0인지 물어봐야함
                L->sets[index].lines[max_waynum].valid = 1;
                L->sets[index].lines[max_waynum].age = 0;
            }
        }

    }

    if (strcmp(op, "W") == 0) {

        *write = *write + 1;

        for (j = 0; j < 8; j++) {   // 다 old 올림
            L->sets[index].lines[j].age++;
            //printf("sets[%d].age[%d]: %d\n",index, j, L->sets[index].lines[j].age);
        }

        for (i = 0; i < 8; i++) {

            if (L->sets[index].lines[i].tag == tag & L->sets[index].lines[i].valid == 1) { // tag 같고, valid 1임 ->  hit
                *hit = *hit + 1;
                L->sets[index].lines[i].tag = tag;
                L->sets[index].lines[i].valid = 1;
                L->sets[index].lines[i].modified = 1;   // dirty = 1로
                L->sets[index].lines[i].age = 0;
                break;
            }
            else if (L->sets[index].lines[i].tag != tag & L->sets[index].lines[i].valid == 0) { //안에 데이터 없음 -> allocate, dirty = 1
                *miss = *miss + 1;
                L->sets[index].lines[i].tag = tag;
                L->sets[index].lines[i].valid = 1;
                L->sets[index].lines[i].modified = 1;
                L->sets[index].lines[i].age = 0;
                break;
            }
            else if (L->sets[index].lines[i].tag != tag & L->sets[index].lines[i].valid == 1) { // 다른 데이터 있음
                count++;
            }

        }

        if (count == 8) {   // tag다른 값들이 8개 꽉 차있음
            *miss = *miss + 1;

            max_waynum = 0;
            for (int a = 0; a < 8; a++) {   // age 가장 높은거 선정
                if (L->sets[index].lines[a].age > max_age) {
                    max_age = L->sets[index].lines[a].age;
                    max_waynum = a;
                }
            }
            if (L->sets[index].lines[max_waynum].modified == 1) {   // 업뎃된 적 있음 -> write_back
                *wb = *wb + 1;
                L->sets[index].lines[max_waynum].tag = tag;  // cache tag 바꿈
                L->sets[index].lines[max_waynum].modified = 1;   
                L->sets[index].lines[max_waynum].age = 0;

            }
            else {  // 업뎃된 적 없음
                L->sets[index].lines[max_waynum].tag = tag;  // cache tag 바꿈
                L->sets[index].lines[max_waynum].modified = 1;   
                L->sets[index].lines[max_waynum].age = 0;
            }
        }

    }
    
}


/*
            else {  // tag 다름 -> write miss
                *miss = *miss + 1;

                if (L->sets[index].lines[i].valid == 0) {   // allocate 해주고 dirty = 1
                    L->sets[index].lines[i].tag == tag;
                    L->sets[index].lines[i].valid == 1;
                    L->sets[index].lines[i].modified = 1;
                    L->sets[index].lines[i].age = 0;
                    break;
                }
                else {  // valid == 1 다 꽉차있음? old 버리고 새로 채움
                    for (int a = 0; a < 8; a++) {   // age 가장 높은거 선정
                        if (L->sets[index].lines[a].age > max_age) {
                            max_age = L->sets[index].lines[a].age;
                            max_waynum = a;
                        }
                    }
                    if (L->sets[index].lines[max_waynum].modified == 1) {   // 업뎃된 적 있음 -> write_back
                        *wb = *wb + 1;
                        L->sets[index].lines[max_waynum].tag = tag;  // cache tag 바꿈
                        L->sets[index].lines[max_waynum].age = 0;
                        L->sets[index].lines[max_waynum].modified = 1;   // 1인지 0인지 물어봐야함
                        //L->sets[index].lines[i].modified = 1;   // dirty = 1로 해줘야하나?
                        break;
                    }
                    else {  // 업뎃된 적 없음
                        L->sets[index].lines[max_waynum].tag = tag;  // cache tag 바꿈
                        L->sets[index].lines[max_waynum].age = 0;
                        L->sets[index].lines[max_waynum].modified = 1;   // 1인지 0인지 물어봐야함
                        //L->sets[index].lines[i].modified = 1;   // dirty = 1로 해줘야하나?
                        break;
                    }

                }
            }
*/

/***************************************************************/
/*                                                             */
/* Procedure : cdump                                           */
/*                                                             */
/* Purpose   : Dump cache configuration                        */
/*                                                             */
/***************************************************************/
void cdump(int capacity, int assoc, int blocksize)
{

    printf("Cache Configuration:\n");
    printf("-------------------------------------\n");
    printf("Capacity: %dB\n", capacity);
    printf("Associativity: %dway\n", assoc);
    printf("Block Size: %dB\n", blocksize);
    printf("\n");
}

/***************************************************************/
/*                                                             */
/* Procedure : sdump                                           */
/*                                                             */
/* Purpose   : Dump cache stat                                 */
/*                                                             */
/***************************************************************/
void sdump(int total_reads, int total_writes, int write_backs,
           int reads_hits, int write_hits, int reads_misses, int write_misses)
{
    printf("Cache Stat:\n");
    printf("-------------------------------------\n");
    printf("Total reads: %d\n", total_reads);
    printf("Total writes: %d\n", total_writes);
    printf("Write-backs: %d\n", write_backs);   // 기록 요청 시 캐시만 갱신. 캐시 라인(해당 데이터)은 Dirty 상태됨. 캐시가 eviction이 발생되는 경우 또는 명확히 clean 명령이 내려질 때에만 메모리에  기록
    printf("Read hits: %d\n", reads_hits);
    printf("Write hits: %d\n", write_hits);
    printf("Read misses: %d\n", reads_misses);  // 메모리에서 데이터 복사한 후 캐쉬안에 넣음 이후 실행
    printf("Write misses: %d\n", write_misses); //캐시에 있는 데이터가 있고 새로운 데이터를 cpu가 요청했을 때,
    printf("\n");
}


/***************************************************************/
/*                                                             */
/* Procedure : xdump                                           */
/*                                                             */
/* Purpose   : Dump current cache state                        */
/*                                                             */
/* Cache Design                                                */
/*                                                             */
/*      cache[set][assoc][word per block]                      */
/*                                                             */
/*                                                             */
/*       ----------------------------------------              */
/*       I        I  way0  I  way1  I  way2  I                 */
/*       ----------------------------------------              */
/*       I        I  word0 I  word0 I  word0 I                 */
/*       I  set0  I  word1 I  word1 I  work1 I                 */
/*       I        I  word2 I  word2 I  word2 I                 */
/*       I        I  word3 I  word3 I  word3 I                 */
/*       ----------------------------------------              */
/*       I        I  word0 I  word0 I  word0 I                 */
/*       I  set1  I  word1 I  word1 I  work1 I                 */
/*       I        I  word2 I  word2 I  word2 I                 */
/*       I        I  word3 I  word3 I  word3 I                 */
/*       ----------------------------------------              */
/*                                                             */
/*                                                             */
/***************************************************************/
void xdump(cache* L)
{
    int i, j, k = 0;
    int b = L->b, s = L->s;    // b = block offset, s = index bits
    int way = L->E, set = 1 << s;   
    //printf("set = 1 << s : %d\n", set);
    uint32_t line;

    printf("Cache Content:\n");
    printf("-------------------------------------\n");

    for(i = 0; i < way; i++) {
        if(i == 0) {
            printf("    ");
        }
        printf("      WAY[%d]", i);
    }
    printf("\n");

    for(i = 0; i < set; i++) {
        printf("SET[%d]:   ", i);

        for(j = 0; j < way; j++) {
            if(k != 0 && j == 0) {
                printf("          ");
            }
            if(L->sets[i].lines[j].valid) {
                line = L->sets[i].lines[j].tag << (s + b);
                line = line | (i << b);
            }
            else {
                line = 0;
            }
            printf("0x%08x  ", line);
        }
        printf("\n");
    }
    printf("\n");
}


int main(int argc, char *argv[])
{
    int capacity=1024;
    int way=8;
    int blocksize=8;
    int set;

    // Cache
    cache simCache;

    // Counts
    int read=0, write=0, writeback=0;
    int readhit=0, writehit=0;
    int readmiss=0, writemiss = 0;

    // Input option
    int opt = 0;
    char* token;
    int xflag = 0;

    // Parse file
    char *trace_name = (char*)malloc(32);
    FILE *fp;
    char line[16];
    char *op;
    uint32_t addr;

    /* You can define any variables that you want */

    trace_name = argv[argc-1];
    if (argc < 3) {
        printf("Usage: %s -c cap:assoc:block_size [-x] input_trace \n", argv[0]);
        exit(1);
    }

    while((opt = getopt(argc, argv, "c:x")) != -1) {
        switch(opt) {
            case 'c':
                token = strtok(optarg, ":");
                capacity = atoi(token);
                token = strtok(NULL, ":");
                way = atoi(token);
                token = strtok(NULL, ":");
                blocksize  = atoi(token);
                break;

            case 'x':
                xflag = 1;
                break;

            default:
                printf("Usage: %s -c cap:assoc:block_size [-x] input_trace \n", argv[0]);
                exit(1);

        }
    }

    // Allocate
    set = capacity / way / blocksize;

    /* TODO: Define a cache based on the struct declaration */
    simCache = build_cache(set, way, blocksize);

    // Simulate
    fp = fopen(trace_name, "r"); // read trace file
    if(fp == NULL) {
        printf("\nInvalid trace file: %s\n", trace_name);
        return 1;
    }

    cdump(capacity, way, blocksize);


    // 파일 한줄씩 읽으면서 access_cache()실행, 
    /* TODO: Build an access function to load and store data from the file */

    while (fgets(line, sizeof(line), fp) != NULL) {
        op = strtok(line, " ");
        addr = strtoull(strtok(NULL, ","), NULL, 16);

#ifdef DEBUG
        // You can use #define DEBUG above for seeing traces of the file.
        fprintf(stderr, "op: %s \n", op);
        fprintf(stderr, "addr: %x\n", addr);
#endif
        // ...
        if (strcmp(op, "R") == 0) { 
            access_cache(&simCache, op, addr,&read,&write, &readhit, &readmiss, &writeback);
            //printf("writehit: %d   writemiss: %d   readhit: %d   readmiss: %d   writeback: %d\n", writehit, writemiss, readhit, readmiss, writeback);
        }
        if (strcmp(op, "W") == 0) {
            access_cache(&simCache, op, addr, &read, &write, &writehit, &writemiss, &writeback);
            //printf("writehit: %d   writemiss: %d   readhit: %d   readmiss: %d   writeback: %d\n", writehit, writemiss, readhit, readmiss, writeback);
        }
        // ...
    }
    
    // test example
    sdump(read, write, writeback, readhit, writehit, readmiss, writemiss);
    if (xflag) {
        xdump(&simCache);
    }

    return 0;
}
