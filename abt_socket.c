#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "abt.h"
#include "abt-io.h"
#include "abt-snoozer.h"

#define PORTNUM 8888
#define BACKLOG 10000
#define CORES 4

void handle_client(void *);
void sighandler(int);

ABT_xstream *xstreams;
ABT_pool pool, g_pool;
ABT_thread threads[BACKLOG];
abt_io_instance_id abtio;
int num_threads;
int main(int argc, char *argv[])
{
  // Argobots definitions
  int ret, i;
  num_threads = 0;

  // Sockets Definitions
  int fd, cfd;
  struct sockaddr_in svaddr;
  struct sockaddr_storage claddr;
  socklen_t addrlen;
  signal(SIGINT, sighandler);
  ABT_init(argc, argv);
  abtio = abt_io_init(CORES);
  int abts = 0;//ABT_snoozer_xstream_self_set();
  if (abts != 0){
    fprintf(stderr, "%s\n", "ABT snoozer xstream self error");
    exit(-1);
  }
  xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * CORES);
  ret = ABT_xstream_self(&xstreams[0]);
  if(ret != 0){
    fprintf(stderr, "%s\n", "ABT xstream self error");
    exit(-1);
  }
  ret = ABT_xstream_get_main_pools(xstreams[0], 1, &pool);
  if(ret != 0){
    fprintf(stderr, "%s\n", "ABT xstream pool error");
    exit(-1);
  }
  ABT_pool_create_basic(ABT_POOL_FIFO, ABT_POOL_ACCESS_MPMC, ABT_TRUE, &g_pool);

  /* ES creation */
  ABT_xstream_self(&xstreams[0]);
  ABT_xstream_set_main_sched_basic(xstreams[0], ABT_SCHED_DEFAULT,
                                   1, &g_pool);
  for (i = 1; i < CORES; i++) {
      ABT_xstream_create_basic(ABT_SCHED_DEFAULT, 1, &g_pool,
                               ABT_SCHED_CONFIG_NULL, &xstreams[i]);
      ABT_xstream_start(xstreams[i]);
  }

  fd = socket(AF_INET, SOCK_STREAM, 0);
  if(fd < 0){
    fprintf(stderr, "%s\n", "Socket creating error");
    exit(-1);
  }
  memset(&svaddr, 0, sizeof(struct sockaddr_in));
  svaddr.sin_family = AF_INET;
  svaddr.sin_addr.s_addr = INADDR_ANY;
  svaddr.sin_port = htons(PORTNUM);

  ret = bind(fd, (struct sockaddr *) &svaddr, sizeof(struct sockaddr_in));
  if(ret < 0){
    fprintf(stderr, "%s\n", "Socket binding error");
    exit(-1);
  }
  if( listen(fd, BACKLOG) == -1)
  {
    fprintf(stderr, "%s\n", "Socket listening error");
    exit(-1);
  }

  addrlen = sizeof(struct sockaddr_storage);
  while(1){
    cfd = accept(fd, (struct sockaddr *)&claddr, &addrlen);
    if(cfd == -1){
      fprintf(stderr, "%s\n", "Socket accepting error");
      exit(-1);
    }
    printf("accepted client on file descriptor %d\n", cfd);
    ABT_thread_create(g_pool, handle_client, &cfd, ABT_THREAD_ATTR_NULL, threads[num_threads++]);
  }

  return 0;
}

void handle_client(void * arg)
{
  int fd = *(int *)arg;
  printf("doing nothing for now in %d \n", fd);
}

void sighandler(int sig)
{
  printf("%s\n", "Caught signal for terminating...");
  printf("%s\n", "Joining threads...");
  int i;
  /* join other threads */
  for(i = 0; i < num_threads;i++){
    ABT_thread_join(threads[i]);
    ABT_thread_free(&threads[i]);
  }
  printf("%s\n", "Joining streams...");

  /* join ESs */
  for (i = 1; i < CORES; i++) {
      ABT_xstream_join(xstreams[i]);
      ABT_xstream_free(&xstreams[i]);
  }

  ABT_finalize();
  abt_io_finalize(abtio);
  free(xstreams);
  exit(0);
}
