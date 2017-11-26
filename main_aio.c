#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <aio.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#define BUFFER 4096

void fill_control_block(struct aiocb *b, int fd, off_t offset, void *buf, size_t nbytes)
{
  b->aio_fildes = fd;
  b->aio_offset = offset;
  b->aio_buf = buf;
  b->aio_nbytes = nbytes;
  b->aio_sigevent.sigev_notify = SIGEV_NONE;
}

typedef struct aio_cp_el{
  int status;
  struct aiocb *read;
  struct aiocb *write;
} aio_cp_el;

int main (int argc, char *argv[]){
  unsigned n, buffer_size = BUFFER;
  off_t read_offset = 0, write_offset = 0;
  int in, out, i, verbose = 0;
  aio_cp_el *block_array;
  off_t file_size, written = 0, write_tmp, read = 0;
  struct stat file_stat;
  char *bufsize_str;

  if (argc < 4) {
    fprintf(stderr, "Usage: %s <from> <to> <number of async operation> [<size of buffer>] [-v]\n-v stands for 'verbose'\n",
	    argv[0]);
    exit(EXIT_FAILURE);
  }

  if ((in = open(argv[1], O_RDONLY)) < 0){
     printf("error open %s\n", argv[1]);
    return -1;
  }
  
  if ((out = open(argv[2], O_WRONLY | O_CREAT)) < 0){
    printf("error open %s\n", argv[2]);
    return -1;
  }

  sscanf(argv[3], "%d", &n);

  if (argc > 4){
    sscanf(argv[4], "%d", &buffer_size);
    if (buffer_size == 0)
      buffer_size = BUFFER;
  }

  printf("Buffersize is %d\n", buffer_size);

  if (argc > 5){
    if(strstr(argv[5], "-v") != NULL){
      printf("Verbose mode is active.\n");
      verbose = 1;
    }
  }

  if(!fstat(in, &file_stat)){
    file_size = file_stat.st_size;
  }else{
    printf("error: %s\n", strerror(errno));
    return -1;
  }

  if ((file_size / buffer_size) < n){
    printf("File is to small for %d aio operations!\n", n);
    return 1;
  }
  
  block_array = (aio_cp_el*) malloc(n*sizeof(aio_cp_el));  

  printf("Filesize is: %lu\n", file_size);

  struct timespec requestStart, requestEnd;
  clock_gettime(CLOCK_REALTIME, &requestStart);
  
  // initialize all async parts, start all independent read operation
  for(i = 0; i < n; i++){
    block_array[i].read = malloc(sizeof(struct aiocb));
    block_array[i].write = malloc(sizeof(struct aiocb));
    memset(block_array[i].read, 0, sizeof(struct aiocb));
    memset(block_array[i].write, 0, sizeof(struct aiocb));
    fill_control_block(block_array[i].read, in, read_offset, NULL, buffer_size);
    block_array[i].read->aio_buf = malloc(buffer_size);
    read_offset+=buffer_size;
    aio_read(block_array[i].read);
    fill_control_block(block_array[i].write, out, write_offset, NULL, buffer_size);
    write_offset+=buffer_size;
    block_array[i].write->aio_buf = malloc(buffer_size);
    block_array[i].status = 0; // 0 - read started, 1 - write started, 2 - nothing
  }

  while (written < file_size){
    for (i = 0; i < n; i++){
      if (block_array[i].status == 0){
	if (aio_error(block_array[i].read) == 0){ // block was read
	  block_array[i].status = 1;
	  block_array[i].write->aio_nbytes = aio_return(block_array[i].read);
	  block_array[i].write->aio_offset = block_array[i].read->aio_offset;
	  read += block_array[i].write->aio_nbytes;
	  if (verbose){
	    printf("%ld bytes at offset %ld was read by %d!\n", block_array[i].write->aio_nbytes,block_array[i].write->aio_offset, i);
	    printf("%ld bytes read already!\n", read);
	  }
	  memcpy(block_array[i].write->aio_buf, block_array[i].read->aio_buf, block_array[i].write->aio_nbytes);
	  aio_write(block_array[i].write);
	}
      }else if (block_array[i].status == 1){ // there is writing going on
	if (aio_error(block_array[i].write) == 0){ // block was successfully written
	  written += aio_return(block_array[i].write);
	  if (verbose){
	    printf("%ld bytes written at %ld offset by %d!\n",aio_return(block_array[i].write), block_array[i].write->aio_offset ,i );
	    printf("written already: %ld\n", written);
	  }
	  block_array[i].status = 2; // 0 - read started, 1 - write started
	  if (read < file_size && (block_array[i].read->aio_offset + n*buffer_size) < file_size){ // need to read more
	    block_array[i].read->aio_offset += n*buffer_size;
	    block_array[i].write->aio_offset += n*buffer_size;
	    //printf("Goint to read %ld bytes from %ld offset! Already read: %ld\n", block_array[i].read->aio_nbytes, block_array[i].read->aio_offset, read );
	    aio_read(block_array[i].read);
	    block_array[i].status = 0; // 0 - read started, 1 - write started
	  }
	}
      }
    }
  }

  clock_gettime(CLOCK_REALTIME, &requestEnd);
  long accum = ( requestEnd.tv_nsec - requestStart.tv_nsec ) + ( requestEnd.tv_sec - requestStart.tv_sec ) * 1E9;
  printf("Async copy took %ld nanosec\n", accum);
  
  for (i=0;i<n;i++){
    free(block_array[i].read->aio_buf);
    free(block_array[i].write->aio_buf);
    free(block_array[i].read);
    free(block_array[i].write);
  }
  close(in);
  close(out);
  free(block_array);
  printf("Total writen: %ld\n", written);

  FILE *stats = fopen("./async_stats.txt", "a");
  if (stats == NULL){
    printf("error: %s\n", strerror(errno));
    return -1;
  }
  fprintf(stats, "%ld %d %d\n", accum, n, buffer_size);
   
  fclose(stats);
  
  return 0;
}
