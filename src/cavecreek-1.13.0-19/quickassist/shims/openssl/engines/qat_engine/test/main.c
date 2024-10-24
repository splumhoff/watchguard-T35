/***************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 *   redistributing this file, you may do so under either license.
 *
 *   GPL LICENSE SUMMARY
 *
 *   Copyright(c) 2007,2008,2009,2010,2011 Intel Corporation. All rights reserved.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of version 2 of the GNU General Public License as
 *   published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *   The full GNU General Public License is included in this distribution
 *   in the file called LICENSE.GPL.
 *
 *   Contact Information:
 *   Intel Corporation
 *
 *   BSD LICENSE
 *
 *   Copyright(c) 2007,2008,2009, 2010,2011 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 *
 ***************************************************************************/

/* macros defined to allow use of the cpu get and set affinity functions */
#define _GNU_SOURCE
#define __USE_GNU

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sched.h>
#include <sys/time.h>
#include <unistd.h>

#include <openssl/engine.h>
#include <openssl/evp.h>

#include "cpa.h"
#include "tests.h"
#include "qae_mem_utils.h"

/* test_count specify the number of workers that each thread  */
static pthread_cond_t ready_cond;
static pthread_cond_t start_cond;
static pthread_cond_t stop_cond;
static pthread_mutex_t mutex;
static int cleared_to_start;
static int active_thread_count;
static int ready_thread_count;
static pthread_mutex_t *open_ssl_mutex_list;
static long *openssl_mutex_count;

/* thread_count control number of threads create */
static int thread_count = 1;

/* define the initial test values */
static int core_count = 1;
static int enable_engine = 1;
static int test_count = 1;
static int test_size = 1024;
static int cpu_affinity = 0;
static int test_type = 0;
static int print_output = 0;
static int verify = 0;
static int zero_copy = 0;
static int cpu_core_info = 0;
extern int qatPerformOpRetries;
/* default engine declaration */
static ENGINE *engine = NULL;

/* Thread_info structure declaration */
typedef struct
{
    pthread_t th;
    int id;
    int count;
}
THREAD_INFO;

#define MAX_STAT 10
#define MAX_CORE 32
typedef union
{
    struct 
    {
        int user;
        int nice;
        int sys;
        int idle;
        int io;
        int irq;
        int softirq;
        int context;
    };
    int d[MAX_STAT];
}
cpu_time_t;

static cpu_time_t cpu_time[MAX_CORE];
static cpu_time_t cpu_time_total;
static cpu_time_t cpu_context;

#define MAX_THREAD 1024

THREAD_INFO tinfo[MAX_THREAD];

/******************************************************************************
* function:
*	 cpu_time_add (cpu_time_t *t1, cpu_time_t *t2, int subtract)
*   
* @param t1 [IN] - cpu time 
* @param t2 [IN] - cpu time 
* @param substract [IN] - subtract flag
*
* description:
*   CPU timing calculation functions.
******************************************************************************/
static void cpu_time_add (cpu_time_t *t1, cpu_time_t *t2, int subtract)
{
    int i;

    for (i = 0; i < MAX_STAT; i++)
    {
        if (subtract)
            t1->d[i] -= t2->d[i];
        else
            t1->d[i] += t2->d[i];
    }
}

/******************************************************************************
* function:
*	read_stat (int init)
*   
* @param init [IN] - op flag  
*
* description:
*  read in CPU status from proc/stat file 
******************************************************************************/
static void read_stat (int init)
{
    char line[1024];
    char tag[10];
    FILE *fp;
    int index = 0;
    int i;
    cpu_time_t tmp;

    if ((fp = fopen ("/proc/stat", "r")) == NULL)
    {
        fprintf (stderr, "Can't open proc stat\n");
        exit (1);
    }

    while (!feof (fp))
    {
        if (fgets (line, sizeof line - 1, fp) == NULL)
            break;

        if (!strncmp (line, "ctxt", 4))
        {
            if (sscanf (line, "%*s %d", &tmp.context) < 1)
                goto parse_fail;

            cpu_time_add (&cpu_context, &tmp, init);
            continue;
        }

        if (strncmp (line, "cpu", 3))
            continue;

        if (sscanf (line, "%s %d %d %d %d %d %d %d",
                tag, 
                &tmp.user,
                &tmp.nice,
                &tmp.sys,
                &tmp.idle,
                &tmp.io,
                &tmp.irq,
                &tmp.softirq) < 8)
        {
            goto parse_fail;
        }

        if (!strcmp (tag, "cpu"))
            cpu_time_add (&cpu_time_total, &tmp, init);
        else if (!strncmp (tag, "cpu", 3))
        {
            index = atoi (&tag[3]);
            cpu_time_add (&cpu_time[index], &tmp, init);
        }
    }

    if (!init && cpu_core_info)
    {
        printf ("      %10s %10s %10s %10s %10s %10s %10s\n", 
                "user", "nice", "sys", "idle", "io", "irq", "sirq");
        for (i = 0; i < MAX_CORE + 1; i++)
        {
            cpu_time_t *t;

            if (i == MAX_CORE)
            {
                printf ("total ");
                t = &cpu_time_total;
            }
            else
            {
                printf ("cpu%d  ", i);
                t = &cpu_time[i];
            }

            printf (" %10d %10d %10d %10d %10d %10d %10d\n", 
                    t->user,
                    t->nice,
                    t->sys,
                    t->idle,
                    t->io,
                    t->irq,
                    t->softirq);
        }

        printf ("Context switches: %d\n", cpu_context.context);
    }

    fclose (fp);
    return;

parse_fail:
    fprintf (stderr, "Failed to parse %s\n", line);
    exit (1);
}

/******************************************************************************
* function:
*   rdtsc (void)
*
* description:
*   Timetamp Counter for measuring clock cycles in performance testing.
******************************************************************************/
static __inline__ unsigned long long rdtsc(void)
{
    unsigned long a, d;

    asm volatile ("rdtsc":"=a" (a), "=d"(d));

    return (((unsigned long long)a) | (((unsigned long long)d) << 32));
}

/******************************************************************************
* function:
*           *test_name(int test)
*
* @param test [IN] - test case
*
* description:
*   test_name selection list
******************************************************************************/
static char *test_name(int test)
{
    switch (test)
    {
        case TEST_SHA512:
            return "SHA512";
            break;
        case TEST_SHA256:
            return "SHA256";
            break;
        case TEST_SHA1:
            return "SHA1";
            break;
        case TEST_AES256:
            return "AES256";
            break;
        case TEST_AES192:
            return "AES192";
            break;
        case TEST_AES128:
            return "AES128";
            break;
        case TEST_RC4:
            return "RC4";
            break;
        case TEST_DES3:
            return "DES3";
            break;
        case TEST_DES3_ENCRYPT:
            return "DES3 encrypt";
            break;
        case TEST_DES3_DECRYPT:
            return "DES3 decrypt";
            break;
        case TEST_DES:
            return "DES";
            break;
        case TEST_DSA_SIGN:
            return "DSA sign";
            break;
        case TEST_DSA_VERIFY:
            return "DSA verify";
            break;
        case TEST_RSA_SIGN:
            return "RSA sign";
            break;
        case TEST_RSA_VERIFY:
            return "RSA verify";
            break;
        case TEST_RSA_ENCRYPT:
            return "RSA encrypt";
            break;
        case TEST_RSA_DECRYPT:
            return "RSA decrypt";
            break;
        case TEST_HMAC:
            return "HMAC";
            break;
        case TEST_MD5:
            return "MD5";
            break;
        case TEST_DH:
            return "DH";
            break;
        case TEST_AES128_CBC_HMAC_SHA1:
            return "AES128 CBC HMAC SHA1";
            break;
        case TEST_AES256_CBC_HMAC_SHA1:
            return "AES256 CBC HMAC SHA1";
            break;
	case 0:
            return "all tests";
            break;
    }
    return "*unknown*";
}

/******************************************************************************
* function:
*           usage(char *program)
*
*
* @param program [IN] - input argument
*
* description:
*   test application usage help
******************************************************************************/
static void usage(char *program)
{
    int i;

    printf("\nUsage:\n");
    printf("\t%s [-t <type>] [-c <count>] [-s <size>] "
           "[-n <count>] [-nc <count>] [-af] [-d]\n", program);
    printf("Where:\n");
    printf("\t-t  specifies the test type to run (see below)\n");
    printf("\t-c  specifies the test iteration count\n");
    printf("\t-s  specifies the test message size (default 1024)\n");
    printf("\t-n  specifies the number of threads to run\n");
    printf("\t-nc specifies the number of CPU cores\n");
    printf("\t-af enables core affinity\n");
    printf("\t-d  disables use of the QAT engine\n");
    printf("\t-p  print the test output\n");
    printf("\t-i  icp config section name\n");
    printf("\t-v  verify the output\n");
    printf("\t-u  display cpu usage per core\n");
    printf("\t-z  enable zero copy mode\n");
    printf("\t-h  print this usage\n");
    printf("\nand where the -t test type is:\n\n");

    for (i = 1; i <= TEST_TYPE_MAX; i++)
        printf("\t%-2d = %s\n", i, test_name(i));
   
    printf("\nIf test type is not specified, one iteration "
           "of each test type is executed and verified.\n");

    /* In order to measure the maximum throughput from QAT, the iteration 
     * test will repeat actual operation to keep QAT busy without reset 
     * input variables such as initial vector. Thus, the iteration count should 
     * limited to one for verification propose. 
     */ 	
    printf("The test iteration count will set to 1 if the verify flag raised.\n\n");
    
    exit(EXIT_SUCCESS);
}

/******************************************************************************
* function:
* parse_option(int *index,
*                        int argc,
*                       char *argv[],
*                        int *value)
*
* @param index [IN] - index pointer
* @param argc [IN] - input argument count
* @param argv [IN] - argument buffer
* @param value [IN] - input value pointer
*
* description:
*   user input arguments check
******************************************************************************/
static void parse_option(int *index, int argc, char *argv[], int *value)
{
    if (*index + 1 >= argc)
    {
        fprintf(stderr, "\nParameter expected\n");
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    (*index)++;

    *value = atoi(argv[*index]);

}

/******************************************************************************
* function:
*           handle_option(int argc,
*                         char *argv[],
*                         int *index)
*
* @param argc [IN] - input argument count
* @param argv [IN] - argument buffer
* @param index [IN] - index pointer
*
* description:
*   input operation handler
******************************************************************************/
static void handle_option(int argc, char *argv[], int *index)
{
    char *option = argv[*index];

    if (!strcmp(option, "-n"))
        parse_option(index, argc, argv, &thread_count);
    else if (!strcmp(option, "-t"))
        parse_option(index, argc, argv, &test_type);
    else if (!strcmp(option, "-c"))
        parse_option(index, argc, argv, &test_count);
    else if (!strcmp(option, "-af"))
        cpu_affinity = 1;
    else if (!strcmp(option, "-nc"))
        parse_option(index, argc, argv, &core_count);
    else if (!strcmp(option, "-s"))
        parse_option(index, argc, argv, &test_size);
    else if (!strcmp(option, "-i"))
    {
        if (*index + 1 >= argc)
        {
            fprintf(stderr, "\nParameter expected\n");
            usage(argv[0]);
            exit(EXIT_FAILURE);
        }

        (*index)++;

        extern char *ICPConfigSectionName_start;

        ICPConfigSectionName_start = argv[*index];
    }
    else if (!strcmp(option, "-d"))
    {
	enable_engine = 0;
	printf("QAT Engine disabled ! \n");
    }
    else if (!strcmp(option, "-p"))
        print_output = 1;
    else if (!strcmp(option, "-v"))
        verify = 1;
    else if (!strcmp(option, "-z"))
        zero_copy = 1;
    else if (!strcmp(option, "-u"))
        cpu_core_info = 1;
    else if (!strcmp(option, "-h"))
        usage(argv[0]);
    else
    {
        fprintf(stderr, "\nInvalid option '%s'\n", option);
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }
    
    if(verify)
    {
	test_count = 1;
	core_count = 1;
	thread_count = 1;
    }
}

/******************************************************************************
* function:
*	pthreads_locking_callback(int mode, int type, const char *file,
*                                      int line)
*
* @param mode [IN] - mutex lock mode  
* @param type [IN] - mutex ID 
* @param file [IN] - file pointer 
* @param line [IN] - line number
*
* description:
*   unlock mutes if not CRYTO_LOCK
******************************************************************************/
static void pthreads_locking_callback(int mode, int type, const char *file,
                                      int line)
{
    if (mode & CRYPTO_LOCK)
    {
        pthread_mutex_lock(&(open_ssl_mutex_list[type]));
        openssl_mutex_count[type]++;
    }
    else
    {
        pthread_mutex_unlock(&(open_ssl_mutex_list[type]));
    }
}

/******************************************************************************
* function:
*	pthreads_thread_id(void)
*
* description:
*   return pthreads ID.
******************************************************************************/
static unsigned long pthreads_thread_id(void)
{
    return (unsigned long)pthread_self();
}

/******************************************************************************
* function:
*           *thread_worker(void *arg)
*
* @param arg [IN] - thread structure info
*
* description:
*   thread worker setups. the threads will lunch at the same time after
*   all of them in ready condition.
******************************************************************************/
static void *thread_worker(void *arg)
{
    extern void qat_set_instance_for_thread(int);

    THREAD_INFO *info = (THREAD_INFO *) arg;
    int rc;
    int performance = 1;

    if (enable_engine)
        qat_set_instance_for_thread(info->id);

    /* mutex lock for thread count */
    rc = pthread_mutex_lock(&mutex);
    ready_thread_count++;
    pthread_cond_broadcast(&ready_cond);
    rc = pthread_mutex_unlock(&mutex);

    /* waiting for thread clearance */
    rc = pthread_mutex_lock(&mutex);

    while (!cleared_to_start)
        pthread_cond_wait(&start_cond, &mutex);

    rc = pthread_mutex_unlock(&mutex);

    tests_run(&info->count,     /* Interation count */
              test_type,        /* Test type */
              test_size,        /* Test message size */
              engine,           /* Engine indicator */
              info->id,         /* Thread ID */
              print_output,     /* Print out results flag */
              verify,           /* Verify flag */
              performance       /* Performance or functional */
        );

    /* update active threads */
    rc = pthread_mutex_lock(&mutex);
    active_thread_count--;
    pthread_cond_broadcast(&stop_cond);
    rc = pthread_mutex_unlock(&mutex);

    return NULL;
}

/******************************************************************************
* function:
*           performance_test(void)
*
* description:
*   performers test application running on user definition .
******************************************************************************/
static void performance_test(void)
{
    int i;
    int coreID = 0;
    int rc = 0;
    int sts = 1;
    cpu_set_t cpuset;
    struct timeval start_time;
    struct timeval stop_time;
    int elapsed = 0;
    unsigned long long rdtsc_start = 0;
    unsigned long long rdtsc_end = 0;
    int bytes_to_bits = 8;
    int crypto_ops_per_test = 1;
    float throughput = 0.0;
    char name[20];

    open_ssl_mutex_list =
        OPENSSL_malloc(CRYPTO_num_locks() * sizeof(pthread_mutex_t));
    if (!open_ssl_mutex_list)
        printf("%s: open_ssl_mutex_list malloc failed.\n", __func__);

    openssl_mutex_count = OPENSSL_malloc(CRYPTO_num_locks() * sizeof(long));
    if (!openssl_mutex_count)
        printf("%s: openssl_mutex_count malloc failed.\n", __func__);

    for (i = 0; i < CRYPTO_num_locks(); i++)
    {
        openssl_mutex_count[i] = 0;
        pthread_mutex_init(&(open_ssl_mutex_list[i]), NULL);
    }

    CRYPTO_set_id_callback(pthreads_thread_id);
    CRYPTO_set_locking_callback(pthreads_locking_callback);

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&ready_cond, NULL);
    pthread_cond_init(&start_cond, NULL);
    pthread_cond_init(&stop_cond, NULL);

    for (i = 0; i < thread_count; i++)
    {
        THREAD_INFO *info = &tinfo[i];

        info->id = i;
        info->count = test_count / thread_count;

        pthread_create(&info->th, NULL, thread_worker, (void *)info);
        sprintf(name, "worker-%d", i);

        /* cpu affinity setup */
        if (cpu_affinity == 1)
        {
            CPU_ZERO(&cpuset);

            /* assigning thread to different cores */
            coreID = (i % core_count);
            CPU_SET(coreID, &cpuset);

            sts = pthread_setaffinity_np(info->th, sizeof(cpu_set_t), &cpuset);
            if (sts != 0)
            {
                printf("pthread_setaffinity_np error, status = %d \n", sts);
                exit(EXIT_FAILURE);
            }
           sts = pthread_getaffinity_np(info->th, sizeof(cpu_set_t), &cpuset);
            if (sts != 0)
            {
                printf("pthread_getaffinity_np error, status = %d \n", sts);
                exit(EXIT_FAILURE);
            }

            if (CPU_ISSET(coreID, &cpuset))
                printf("Thread %d assigned on CPU core %d\n", i, coreID);
        }
    }

    /* set all threads to ready condition */
    rc = pthread_mutex_lock(&mutex);

    while (ready_thread_count < thread_count)
    {
        pthread_cond_wait(&ready_cond, &mutex);
    }

    rc = pthread_mutex_unlock(&mutex);

    printf("Beginning test ....\n");
    /* all threads start at the same time */
    read_stat (1);
    gettimeofday(&start_time, NULL);
    rdtsc_start = rdtsc();
    rc = pthread_mutex_lock(&mutex);
    cleared_to_start = 1;
    pthread_cond_broadcast(&start_cond);
    pthread_mutex_unlock(&mutex);

    /* wait for other threads stop */
    rc = pthread_mutex_lock(&mutex);

    while (active_thread_count > 0)
        pthread_cond_wait(&stop_cond, &mutex);

    rc = pthread_mutex_unlock(&mutex);

    for (i = 0; i < thread_count; i++)
    {
        if (pthread_join(tinfo[i].th, NULL))
            printf("Could not join thread id - %d !\n", i);
    }

    CRYPTO_set_locking_callback(NULL);

    for (i = 0; i < CRYPTO_num_locks(); i++)
    {
        pthread_mutex_destroy(&(open_ssl_mutex_list[i]));
    }

    OPENSSL_free(open_ssl_mutex_list);
    OPENSSL_free(openssl_mutex_count);

    rdtsc_end = rdtsc();
    gettimeofday(&stop_time, NULL);
    read_stat (0);
    printf("All threads complete\n\n");

    if (!test_count)
    {
        for (i = 0; i < thread_count; i++)
        {
            test_count -= tinfo[i].count;
        }
    }

    /* generate report */
    elapsed = (stop_time.tv_sec - start_time.tv_sec) * 1000000 +
        (stop_time.tv_usec - start_time.tv_usec);

    /* Cipher tests, except for DES3_ENCRYPT and DES3_DECRYPT, contain 2
       performOp calls. */
    if (test_type <= MAX_CIPHER_TEST_TYPE && test_type != TEST_DES3_ENCRYPT
        && test_type != TEST_DES3_DECRYPT)
        crypto_ops_per_test = 2;

    /* Cast test_size * test_count to avoid int overflow */
    throughput = ((float)test_size * (float)test_count *
                  (bytes_to_bits * crypto_ops_per_test) / (float)elapsed);

    printf("Elapsed time   = %.3f msec\n", (float)elapsed / 1000);
    printf("Operations     = %d\n", test_count);

    printf("Time per op    = %.3f usec (%d ops/sec)\n",
           (float)elapsed / test_count,
           (int)((float)test_count * 1000000.0 / (float)elapsed));

    printf("Elapsed cycles = %llu\n", rdtsc_end - rdtsc_start);

    printf("Throughput     = %.2f (Mbps)\n", throughput);

    printf("Retries        = %d\n", qatPerformOpRetries);

    printf("\nCSV summary:\n");

    printf("Algorithm,"
           "Test_type,"
           "Using_engine,"
           "Core_affinity,"
           "Elapsed_usec," 
           "Cores," 
           "Threads," 
           "Count," 
           "Data_size," 
           "Mbps,"
           "CPU_time,"
           "User_time,"
           "Kernel_time\n");

    int cpu_time = 0;
    int cpu_user = 0;
    int cpu_kernel = 0;

    cpu_time = (cpu_time_total.user + 
                cpu_time_total.nice + 
                cpu_time_total.sys + 
                cpu_time_total.io + 
                cpu_time_total.irq + 
                cpu_time_total.softirq) * 10000 / core_count;
    cpu_user = cpu_time_total.user * 10000 / core_count;
    cpu_kernel = cpu_time_total.sys * 10000 / core_count;

    printf("csv,%s,%d,%s,%s,%d,%d,%d,%d,%d,%.2f,%d,%d,%d\n",
           test_name(test_type),
           test_type,
           (enable_engine) ? "Yes" : "No",
           cpu_affinity ? "Yes" : "No",
           elapsed,
           core_count, thread_count, test_count, test_size, throughput,
           cpu_time * 100 / elapsed,
            cpu_user * 100 / elapsed,
            cpu_kernel * 100 / elapsed);
}

/******************************************************************************
* function:
*           functional_test(void)
*
* description:
*    Default testing application, a single thread test running through all the
*    test cases with testing function definition values
******************************************************************************/
static void functional_test(void)
{
    int i;
    int id = 0;
    int verify = 1;
    int count = 1;
    int size = 1024;
    int performance = 0;

    printf("\nResults for functional test cases:\n");

    for (i = 1; i <= TEST_TYPE_MAX; i++)
    {
        if (zero_copy &&
            (i == TEST_SHA1 || 
             i == TEST_SHA256 ||
             i == TEST_SHA512 || 
             i == TEST_MD5 || 
             i == TEST_DSA_SIGN || 
             i == TEST_DSA_VERIFY || 
             i == TEST_DH || 
             i == TEST_HMAC))
        {
            printf ("skipping %s in zero copy mode\n",
                    test_name(i));
            continue;
        }

        tests_run(&count, i, size, engine, id, print_output, verify,
                  performance);
    }
}

/******************************************************************************
* function:
*           main(int argc,
*                char *argv[])
*
* @param argc [IN] - input argument count
* @param argv [IN] - argument buffer
*
* description:
*    main function is used to setups QAT engine setups and define the testing type.
******************************************************************************/
int main(int argc, char *argv[])
{
    int i = 0;

    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] != '-')
            break;

        handle_option(argc, argv, &i);
    }

    /* The thread count should not be great than the test cout */ 
    if (thread_count > test_count )
    {
	thread_count = test_count;

 	printf("\nWARNING - Thread Count cannot aceed the Test Count");
        printf("\nThread Count adjusted to: %d\n\n", thread_count);
    } 



    if (zero_copy)
    {
        if (access(QAT_DEV, F_OK) == 0)
        {
            if (CRYPTO_set_mem_ex_functions(qaeMemAlloc, qaeMemRealloc, qaeMemFree))
            {
                DEBUG("%s: CRYPTO_set_mem_functions succeeded\n", __func__);
            }

            setMyVirtualToPhysical (qaeMemV2P);
        }
        else
        {
            perror(QAT_DEV);
        }
    }

    if (i < argc)
    {
        fprintf(stderr,
                "This program does not take arguments, please use -h for usage.\n");
        exit(EXIT_FAILURE);
    }

    active_thread_count = thread_count;
    ready_thread_count = 0;

    /* Load qat engine for workers */
    if (enable_engine)
    {
        ENGINE_load_builtin_engines();
        engine = tests_initialise_engine();

        if (!engine)
        {
            fprintf(stderr, "ENGINE load error, exit! \n");
            exit(EXIT_FAILURE);
        }
    }
    else
	printf("QAT Engine disabled ! \n");
    
    printf("\nQAT openssl engine test application\n");
    printf("\n\tCopyright (C) 2010 Intel Corporation\n");
    printf("\nTest parameters:\n\n");
    printf("\tTest type:           %d (%s)\n", test_type, test_name(test_type));
    printf("\tTest count:          %d\n", test_count);
    printf("\tThread count:        %d\n", thread_count);
    printf("\tMessage size:        %d\n", test_size);
    printf("\tPrint output:        %s\n", print_output ? "Yes" : "No");
    printf("\tCPU core affinity:   %s\n", cpu_affinity ? "Yes" : "No");
    printf("\tNumber of cores:     %d\n", core_count);
    printf("\tQAT Engine enabled:  %s\n", (enable_engine) ? "Yes" : "No");

    printf("\n");

    if (test_type == 0)
        functional_test();
    else
        performance_test();

    if (engine)
        tests_cleanup_engine(engine);
    ENGINE_cleanup();

    return 0;
}
