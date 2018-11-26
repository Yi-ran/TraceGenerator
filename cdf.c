//
//  main.c
//  TraceGenerator
//
//  Created by Yiran on 2018/11/25.
//  Copyright © 2018年 Yiran. All rights reserved.
//

#include "cdf.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>


#define LINK_CAPACITY_BASE    1000000000
double START_TIME = 0.0;
double END_TIME = 0.25;
double FLOW_LAUNCH_END_TIME = 0.1;
/* initialize a CDF distribution */
void init_cdf(struct cdf_table *table)
{
    if(!table)
        return;
    
    table->entries = (struct cdf_entry*)malloc(TG_CDF_TABLE_ENTRY * sizeof(struct cdf_entry));
    table->num_entry = 0;
    table->max_entry = TG_CDF_TABLE_ENTRY;
    table->min_cdf = 0;
    table->max_cdf = 1;
    
    if (!(table->entries))
        perror("Error: malloc entries in init_cdf()");
}

/* free resources of a CDF distribution */
void free_cdf(struct cdf_table *table)
{
    if (table)
        free(table->entries);
}

/* get CDF distribution from a given file */
void load_cdf(struct cdf_table *table, const char *file_name)
{
    FILE *fd = NULL;
    char line[256] = {0};
    struct cdf_entry *e = NULL;
    int i = 0;
    
    if (!table)
        return;
    
    fd = fopen(file_name, "r");
    if (!fd){
        printf("name:%s.\n",file_name);
        perror("Error: open the CDF file in load_cdf()");
    }
    
    while (fgets(line, sizeof(line), fd))
    {
        /* resize entries */
        if (table->num_entry >= table->max_entry)
        {
            table->max_entry *= 2;
            e = (struct cdf_entry*)malloc(table->max_entry * sizeof(struct cdf_entry));
            if (!e)
                perror("Error: malloc entries in load_cdf()");
            for (i = 0; i < table->num_entry; i++)
                e[i] = table->entries[i];
            free(table->entries);
            table->entries = e;
        }
        
        sscanf(line, "%lf %lf", &(table->entries[table->num_entry].value), &(table->entries[table->num_entry].cdf));
        
        if (table->min_cdf > table->entries[table->num_entry].cdf)
            table->min_cdf = table->entries[table->num_entry].cdf;
        if (table->max_cdf < table->entries[table->num_entry].cdf)
            table->max_cdf = table->entries[table->num_entry].cdf;
        
        table->num_entry++;
    }
    fclose(fd);
}

/* print CDF distribution information */
void print_cdf(struct cdf_table *table)
{
    int i = 0;
    
    if (!table)
        return;
    
    for (i = 0; i < table->num_entry; i++)
        printf("%.2f %.2f\n", table->entries[i].value, table->entries[i].cdf);
}

/* get average value of CDF distribution */
double avg_cdf(struct cdf_table *table)
{
    int i = 0;
    double avg = 0;
    double value, prob;
    
    if (!table)
        return 0;
    
    for (i = 0; i < table->num_entry; i++)
    {
        if (i == 0)
        {
            value = table->entries[i].value / 2;
            prob = table->entries[i].cdf;
        }
        else
        {
            value = (table->entries[i].value + table->entries[i-1].value) / 2;
            prob = table->entries[i].cdf - table->entries[i-1].cdf;
        }
        avg += (value * prob);
    }
    
    return avg;
}

double interpolate(double x, double x1, double y1, double x2, double y2)
{
    if (x1 == x2)
        return (y1 + y2) / 2;
    else
        return y1 + (x - x1) * (y2 - y1) / (x2 - x1);
}

/* generate a random floating point number from min to max */
double rand_range(double min, double max)
{
    return min + rand() * (max - min) / RAND_MAX;
}

/* generate a random value based on CDF distribution */
double gen_random_cdf(struct cdf_table *table)
{
    int i = 0;
    double x = rand_range(table->min_cdf, table->max_cdf);
    /* printf("%f %f %f\n", x, table->min_cdf, table->max_cdf); */
    
    if (!table)
        return 0;
    
    for (i = 0; i < table->num_entry; i++)
    {
        if (x <= table->entries[i].cdf)
        {
            if (i == 0)
                return interpolate(x, 0, 0, table->entries[i].cdf, table->entries[i].value);
            else
                return interpolate(x, table->entries[i-1].cdf, table->entries[i-1].value, table->entries[i].cdf, table->entries[i].value);
        }
    }
    
    return table->entries[table->num_entry-1].value;
}

double poission_gen_interval(double avg_rate)
{
    if (avg_rate > 0)
        return -logf(1.0 - (double)rand() / RAND_MAX) / avg_rate;
    else
        return 0;
}

int rand_range_int (int min, int max)
{
    return min + ((double)max - min) * rand () / RAND_MAX;
}


/*generate trace file
 input parameters: load(%), servernum, link capacity, cdffile
 output parameters:
 starttime, src, dest, size*/
int main(int argc, const char * argv[])
{
    char cdfFileName[20] = "DCTCP.txt";
    int servernum = 128;
    int k = 16; //server num per leaf
    int LeafNum = servernum / k;
    double load = 0.3;
    double linkc = 10;
    long flowCount = 0;
    long flowSize = 0;
    long totalFlowSize = 0;
    unsigned randomSeed = 0;
    int i, j,src,dest;
    double startTime;
    FILE *fd = NULL;
    char output[100] = "websearchload30.txt";
    
    
    //Initiate cdf_table
    struct cdf_table cdfTable;
    
    init_cdf (&cdfTable);
    load_cdf (&cdfTable, cdfFileName);
    
    
    //Calculating request rate.  oversubscription:2
    double requestRate = load * linkc * LINK_CAPACITY_BASE * k / 2 /(8 * avg_cdf (&cdfTable)) / k;
    printf("Average request rate: %lf per second\n", requestRate);
    
    //Initialize random seed
    if (randomSeed == 0)
    {
        srand ((unsigned)time(NULL));
    }
    else
    {
        srand (randomSeed);
    }
    
    fd = fopen(output, "w");
    if (!fd)
    {
        printf("Error: open the CDF file when writing to file");
        return 0;
    }
    //generate cross leaf traffic
    for(i = 0; i < LeafNum; i++)
    {
        for (j = 0; j < k; j++)
        {
            
            src = i * k + j;
            dest = src;
            startTime = START_TIME + poission_gen_interval (requestRate);
            while (startTime < FLOW_LAUNCH_END_TIME)
            {
                flowCount ++;
                while (dest >= i * k && dest < i * k + k)
                {
                    dest = rand_range_int (0, k * LeafNum);
                }
                flowSize = gen_random_cdf (&cdfTable);
                totalFlowSize += flowSize;
                fprintf(fd,"%.8f %d %d %ld\n",startTime,src,dest,flowSize);
                startTime += poission_gen_interval (requestRate);
            }
        }
    }
    
    
    fclose(fd);
    printf("Total flow: %ld.\n ",flowCount);
    
    printf("Actual average flow size: %ld.\n",totalFlowSize/flowCount);
    
    
    
    return 0;
}
