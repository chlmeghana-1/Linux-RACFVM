#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <iconv.h>
#include <sys/socket.h>
#include <netiucv/iucv.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/ipc.h>
#include <sys/msg.h>

#include "logger/log_codes.h"
#include "logger/log_macros.h"


#define RACF_SERVER_ID "RACFVM  "
#define RACF_SERVICE_NAME "RISERVER"

#define REQ_BUFFER_SIZE 255
#define RESP_BUFFER_SIZE 2000000

#define LOCK_PATH "/var/lock/iucv_session.lock"
#define LOG_FILE_NAME "/var/log/vmrac.log"
#define LOGGER_MODULE_NAME "vmrac"
