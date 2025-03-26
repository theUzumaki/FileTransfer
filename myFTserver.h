#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <semaphore.h>

sem_t sem;
