#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <assert.h>
#include <string.h>
#include <sys/random.h>

#ifdef DEBUG
#define TIMEOUT 300
#define SEQUENCELEN_RANDOM 1024
#define RANDOMBYTES 16
#define FLAGLEN 32
#else
#define TIMEOUT 300
#define SEQUENCELEN_RANDOM 131072
#define RANDOMBYTES 1024
#define FLAGLEN 64
#endif

#define SEQUENCELEN (SEQUENCELEN_RANDOM + FLAGLEN)
#define VALUEBYTES ((SEQUENCELEN / 8) + 1)

#define ERRORMSG "Error.\n"

#define FLAGPREFIX "techx{"
#define FLAGSUFFIX "}"

void timer_callback(int signum)
{
  printf("Out of time.\n");
  exit(1);
}

unsigned char step(unsigned char* value, unsigned int length)
{
  unsigned char result;
  result = value[0] & 1; 
  if(result)
  {
    unsigned long tmp = 1;
    unsigned long new_tmp = 0;
    for(unsigned int i = 0; i < length; i++)
    {
      new_tmp = (unsigned long)value[i];
      new_tmp *= 3;
      new_tmp += tmp;
      tmp = new_tmp >> 8;
      new_tmp &= 0xFF;
      value[i] = (unsigned char)new_tmp;
    }
    if(tmp)
    {
      printf(ERRORMSG);
#ifdef DEBUG
      printf("Overflow\n");
#endif
      exit(99);
    }
  }
  unsigned char extra = 0;
  unsigned char new_extra;
  for(signed int i = length - 1; i >= 0; i--)
  {
    new_extra = value[i] & 1;
    value[i] >>= 1;
    value[i] |= (extra * 0x80);
    extra = new_extra;
  }
  assert(extra == 0);
  return result;
}

int main()
{
  setbuf(stdin, NULL);
  setbuf(stdout, NULL);
#ifdef DEBUG
#endif

  assert(SEQUENCELEN_RANDOM / RANDOMBYTES == 2 * FLAGLEN);

  struct sigaction alarm_action;
  alarm_action.sa_handler = timer_callback;
  sigemptyset(&alarm_action.sa_mask);
  alarm_action.sa_flags = 0;

  sigaction(SIGALRM, &alarm_action, NULL);

  alarm(TIMEOUT);

  unsigned int seed;
  if(getrandom(&seed, sizeof(seed), 0) != sizeof(seed))
  {
    printf(ERRORMSG);
    exit(2);
  }
  printf("%X\n", seed);
  srand(seed);

#ifdef DEBUG
#endif

  unsigned char * random = malloc(RANDOMBYTES);
  if(getrandom(random, RANDOMBYTES, 0) != RANDOMBYTES)
  {
    printf(ERRORMSG);
    exit(8);
  }

  FILE * flag_fp;
  flag_fp = fopen("./flag.txt", "r");
  if(!flag_fp)
  {
    printf(ERRORMSG);
    exit(3);
  }
  unsigned int flag_total_len = strlen(FLAGPREFIX) + FLAGLEN + strlen(FLAGSUFFIX);
  unsigned char * flag_full = malloc(flag_total_len + 1);
  if(!flag_full)
  {
    printf(ERRORMSG);
    exit(4);
  }
  if(fread(flag_full, 1, flag_total_len, flag_fp) != flag_total_len)
  {
    printf(ERRORMSG);
    exit(5);
  }
  fclose(flag_fp);

  assert(!strncmp((char*)flag_full, FLAGPREFIX, strlen(FLAGPREFIX)));
  assert(!strncmp((char*)((long)flag_full + strlen(FLAGPREFIX) + FLAGLEN), FLAGSUFFIX, strlen(FLAGSUFFIX)));
  unsigned char * flag;
  flag = (unsigned char *)((long)(flag_full + strlen(FLAGPREFIX)));
  for(unsigned int i = 0; i < FLAGLEN; i++)
  {
    assert((0x5f <= flag[i]) && (flag[i] <= 0x7a));
  }
  
#ifdef DEBUG
#endif
  unsigned char * value = malloc(VALUEBYTES);
  if(!value)
  {
    printf(ERRORMSG);
    exit(6);
  }
  if(fread(value, 1, VALUEBYTES, stdin) != VALUEBYTES)
  {
    printf(ERRORMSG);
    exit(7);
  }
#ifdef DEBUG
#endif

  unsigned char * flag_enc = calloc(FLAGLEN, 1);
  unsigned char v;
  unsigned int index;
  unsigned char should_xor = 0;

#ifdef REVENGE
  unsigned int * random_occ_count = calloc(sizeof(unsigned int)*RANDOMBYTES, 1);
  if(!random_occ_count)
  {
    printf(ERRORMSG);
    exit(20);
  }
  unsigned int * random_xor_count = calloc(sizeof(unsigned int)*RANDOMBYTES, 1);
  if(!random_xor_count)
  {
    printf(ERRORMSG);
    exit(21);
  }
  unsigned char ** flag_xor_count = malloc(sizeof(unsigned char *)*FLAGLEN);
  if(!flag_xor_count)
  {
    printf(ERRORMSG);
    exit(22);
  }
  for(unsigned int i = 0; i < FLAGLEN; i++)
  {
    flag_xor_count[i] = calloc(RANDOMBYTES / 8, 1);
    if(!flag_xor_count[i])
    {
      printf(ERRORMSG);
      exit(23);
    }
  }
#endif

  for(unsigned int i = 0; i < SEQUENCELEN; i++)
  {
    if(i < SEQUENCELEN_RANDOM)
    {
      index = (unsigned int)rand() % RANDOMBYTES;
#ifdef REVENGE
      random_occ_count[index] += 1;
#endif
      v = random[index];
    }
    else
    {
      v = flag[i % FLAGLEN];
    }
    should_xor = step(value, VALUEBYTES);
    if(should_xor)
    {
      flag_enc[i % FLAGLEN] ^= v;
#ifdef REVENGE
      if(i < SEQUENCELEN_RANDOM)
      {
        random_xor_count[index] += 1;
        flag_xor_count[i % FLAGLEN][index >> 3] ^= 1 << (index % 8);
      }
#endif
    }
  }

  #ifdef REVENGE
  for(unsigned int flag_index = 0; flag_index < FLAGLEN; flag_index++)
  {
    unsigned char changed = 0;
    for(unsigned int j = 0; j < (RANDOMBYTES / 8); j++)
    {
      changed |= flag_xor_count[flag_index][j];
    }
    if(changed == 0)
    {
      printf(ERRORMSG);
#ifdef DEBUG
      printf("Flag byte %i has not been xored into!\n", flag_index);
#endif
      exit(10);
    }
  }
  for(unsigned int i = 0; i < RANDOMBYTES; i++)
  {
    if(random_xor_count[i] + FLAGLEN < random_occ_count[i])
    {
      printf(ERRORMSG);
#ifdef DEBUG
      printf("Random byte %i not used enough!\n", i);
#endif
      exit(11);
    }
  }
  #endif

  for(unsigned int i = 0; i < FLAGLEN; i++)
  {
    printf("%0X ", flag_enc[i]);
  }
  printf("\n");

  free(random);
  free(flag_full);
  free(value);
  free(flag_enc);
#ifdef REVENGE
  free(random_xor_count);
  free(random_occ_count);
  for(unsigned int i = 0; i < FLAGLEN; i++)
  {
    free(flag_xor_count[i]);
  }
  free(flag_xor_count);
#endif
}