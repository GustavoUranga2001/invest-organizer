#define _POSIX_C_SOURCE 200809L
#include "lock.h"
#include "storage.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
static int lock_fd = -1;

static int create_parents(const char *path) {
    char temp[PATH_MAX];size_t i, n=strlen(path);
    if (n>=sizeof(temp))return -1;
    memcpy(temp,path,n+1);
    for (i=1;i<n;i++)if (temp[i]=='/') {
        temp[i]=0;
        if (mkdir(temp,0700)<0 && errno!=EEXIST)return -1;
        temp[i]='/';
    }
    return 0;
}
int invest_lock_acquire(char *error,size_t size) {
    char path[PATH_MAX];struct flock fl={0};char *slash;
    if (lock_fd>=0)return 0;
    if (storage_path(path,sizeof(path))!=0 || !(slash=strrchr(path,'/')) ||
        (size_t)(path+sizeof(path)-(slash+1)) < sizeof(".invest.lock") ||
        snprintf(slash+1,(size_t)(path+sizeof(path)-(slash+1)),".invest.lock")<0 ||
        create_parents(path)!=0)goto fail;
    lock_fd=open(path,O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW,0600);
    if(lock_fd<0)goto fail;
    fl.l_type=F_WRLCK;fl.l_whence=SEEK_SET;
    if(fcntl(lock_fd,F_SETLK,&fl)<0){
        if(error&&size)snprintf(error,size,"Outro invest está aberto (ou arquivo de bloqueio indisponível)");
        close(lock_fd);lock_fd=-1;return -1;
    }
    return 0;
fail:
    if(error&&size)snprintf(error,size,"Não foi possível criar bloqueio da pasta de dados");
    return -1;
}
void invest_lock_release(void){
    if(lock_fd>=0){close(lock_fd);lock_fd=-1;}
}
