#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include "linux_client.h"

// Acquire system-wide lock
int take_lock() {
    int fd = open(LOCK_PATH, O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
        perror("open lock");
        return -1;
    }
    if (flock(fd, LOCK_EX) < 0) {
        perror("flock");
        close(fd);
        return -1;
    }
    return fd; // Keep fd open to hold the lock
}

// Release lock
void release_lock(int fd) {
    if (fd >= 0) {
        flock(fd, LOCK_UN);
        close(fd);
    }
}

// Connect to RACF via IUCV sock_fdet and get sock_fd
int connect_to_server(int *sock_fd) {
    struct sockaddr_iucv addr;
    memset(&addr, 0, sizeof(addr));
    addr.siucv_family = AF_IUCV;
    memset(addr.siucv_user_id, ' ', 8);
    memcpy(addr.siucv_user_id, RACF_SERVER_ID, 8);
    strncpy(addr.siucv_name, RACF_SERVICE_NAME, 16);
    addr.siucv_port = 0;

    //*sock_fd = socket(AF_IUCV, SOCK_STREAM | SOCK_NONBLOCK, 0);
    *sock_fd = socket(AF_IUCV, SOCK_STREAM, 0);
    if (*sock_fd < 0) {
        LOG_ERR(IUCVCLNT010, "%s", strerror(errno));
	printf("Unable to create socket(%s:%s). %s!!!\n",
		RACF_SERVER_ID, RACF_SERVICE_NAME, strerror(errno));
        return -1;
    }

    LOG_INFO(IUCVCLNT011, "(%s:%s)", RACF_SERVER_ID, RACF_SERVICE_NAME);

    int ret = connect(*sock_fd, (struct sockaddr*)&addr, sizeof(addr));

    if (ret == 0) {
        LOG_INFO(IUCVCLNT012, "(%s:%s)", RACF_SERVER_ID, RACF_SERVICE_NAME);
	return 0;
    } else {
        if (errno == EINPROGRESS || errno == EAGAIN) {
            LOG_ERR(IUCVCLNT013, "(%s:%s)", RACF_SERVER_ID, RACF_SERVICE_NAME); 
        } else if (errno == ECONNREFUSED) {
            LOG_ERR(IUCVCLNT014, "(%s:%s)", RACF_SERVER_ID, RACF_SERVICE_NAME); 
        } else {
	    LOG_ERR(IUCVCLNT015, "%s", strerror(errno)); 
        }
	printf("Unable to procees the command. Error connecting to the server(%s:%s). %s!!!\n",
		RACF_SERVER_ID, RACF_SERVICE_NAME, strerror(errno));
        close(*sock_fd);
        return -1;
    }
    return 0;
}

int ascii_to_ebcdic(const char *in, char *out, size_t outlen) {
    iconv_t cd = iconv_open("EBCDIC-US", "ASCII");
    if (cd == (iconv_t)-1) return -1;

    char *pin = (char *)in;
    char *pout = out;
    size_t inbytes = strlen(in);
    size_t outbytes = outlen;

    if (iconv(cd, &pin, &inbytes, &pout, &outbytes) == (size_t)-1) {
        iconv_close(cd);
        return -1;
    }
    *pout = '\0';
    iconv_close(cd);
    return 0;
}

void ebcdic_to_ascii(char* out, const char* in, size_t len) {
    iconv_t cd = iconv_open("ASCII//TRANSLIT", "IBM1047");
    if (cd == (iconv_t)-1) {
        perror("iconv_open");
        return;
    }

    size_t inbytesleft = len;
    size_t outbytesleft = len * 2; // generous buffer
    char* inbuf = (char*)in;
    char* outbuf = out;
    char* outbuf_start = out;

    memset(out, 0, outbytesleft); // clear output buffer

    size_t result = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
    if (result == (size_t)-1) {
        perror("iconv");
    }

    iconv_close(cd);

    for (char* p = out; *p; ++p) {
        if (*p == '?') {
            *p = '\n';
        }
    }
}

int send_command(int sockfd, char* buffer, int buffer_len) {
    size_t byte_len = 0;

   int ret = send(sockfd, buffer, buffer_len, 0);
    if (ret < 0) {
        LOG_ERR(IUCVCLNT026, "SockFD:%d Error:%s", sockfd, strerror(errno));
	printf("Unable to process the command. %s\n", strerror(errno));
        return -1;
    }
    return 0;
}



int receive_iucv_response(int sockfd, char* cmd_request, char* cmd_response)
{
    int ret;
    char l_recv_buffer[1001];
    char l_ascii_buffer[1001];
    char l_ebcidic_buffer[1001];
    int tget_count = 0;
    size_t used = 0;
    int hx_count=0;
    struct timeval timeout = {600, 0};
    cmd_response[0] = '\0';

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        LOG_ERR(IUCVCLNT025, "%s", strerror(errno));
        return -1;
    }

    while ((ret = recv(sockfd, l_recv_buffer, sizeof(l_recv_buffer) - 1, 0)) > 0) {
       //  Convert whole line EBCDIC->ASCII 
        ebcdic_to_ascii(l_ascii_buffer, l_recv_buffer, ret);
        l_ascii_buffer[ret] = '\0';

	if (strstr(l_ascii_buffer, "IKJ56718A REENTER THIS OPERAND+ -") != 0)
	    hx_count++;

	// Send user command when first TGET is received in response
	// Send END command when 2nd TGET is received in response
        if (strcmp(l_ascii_buffer, "TGET") == 0) {
             const char* msg = cmd_request;

             if(tget_count == 1 || tget_count == 2 )
                 msg = "end";

             if (hx_count == 1) {
                 hx_count ++;
                 msg = "end";
	     }
            tget_count++;
            
	    char ebcdic_msg[1001];
            ascii_to_ebcdic(msg, l_ebcidic_buffer, sizeof(l_ebcidic_buffer));

            int s = send_command(sockfd, l_ebcidic_buffer, strlen(l_ebcidic_buffer));
            if (s < 0) {
                LOG_ERR(IUCVCLNT016, "%s", strerror(errno));
                return -1;
            }
	    if (tget_count== 2 && hx_count == 0)
		    return 0;
            if (tget_count == 3 ){
            if (hx_count == 0)
                return 0;
            else{
                 printf("IKJ56718A - INVALID COMMAND\n");
                 printf("PLEASE ENTER AGAIN\n");
                 return 0;
            }
            }
            continue;
        }

        //  NOT TGET → append entire chunk to cmd_response 
                   if (strstr(l_ascii_buffer, "INVALID") == 0 && 
                strstr(l_ascii_buffer, "NAME TO BE ADDED TO RACF DATA SET ALREADY EXISTS") == 0 &&
	       strstr(l_ascii_buffer, "TO TERMINATE ENTER ") == 0 &&	
                strstr(l_ascii_buffer, "END      NOT ADDED") == 0 && 
                strstr(l_ascii_buffer, "RACF CMND ERROR.") == 0 && 
                strstr(l_ascii_buffer, "IKJ56718A REENTER THIS OPERAND+ -") == 0)  {
            size_t chunk_len = strlen(l_ascii_buffer);

            if (used + chunk_len + 1 < RESP_BUFFER_SIZE) {
                memcpy(cmd_response + used, l_ascii_buffer, chunk_len);
                used += chunk_len;
                cmd_response[used++] = '\n';
            }
        }
    }

    if (ret < 0) {
        LOG_ERR(IUCVCLNT016, "SockFD:%d RC:%d Error:%s",
                sockfd, ret, strerror(errno));
        return -1;
    }

    if (ret == 0) {
        LOG_ERR(IUCVCLNT017, "SockFD:%d RC:%d Error:%s",
                sockfd, ret, strerror(errno));
        return -1;
    }

    return 0;
}

int execute_user_command(int* sockfd, char* cmd_request, char* cmd_response) {

    LOG_INFO(IUCVCLNT002, "Cmd:%s", cmd_request);

    // Handle the response
    int resp_code = receive_iucv_response(*sockfd, cmd_request, cmd_response);
    if(0 == resp_code) {
        //Print the response
	printf("%s\n", cmd_response);
    } else {
        LOG_ERR(IUCVCLNT022, "ResponseCode:%d\n", resp_code);
	printf("Error while receiving the command response[%d]\n", resp_code);
        return resp_code;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    set_logger_name(LOGGER_MODULE_NAME);
    set_log_file(LOG_FILE_NAME);
    int sock_fd = -1;
    char* cmd_request = malloc(REQ_BUFFER_SIZE + 1);
    char* cmd_response = malloc(RESP_BUFFER_SIZE + 1);
 
    if (geteuid() != 0) {
        printf("Usage: sudo %s <RACF Command>\n", argv[0]);
        return 1;
    }

    // Validate argument count
    if (argc != 2) {
        printf("Usage: sudo %s <RACF Command>\n", argv[0]);
        return 2; // Non-zero exit code for error
    }

    strcpy(cmd_request, argv[1]);
    cmd_request[strcspn(cmd_request, "\n")] = '\0';

    // Acquire lock
    int lock_fd = take_lock();
    if (lock_fd < 0) return 1;

    // Connect to CMS server
    int ret = connect_to_server(&sock_fd);
    if (ret != 0) {
	    release_lock(lock_fd);
	    return -1;
    }
    
    // Send request
    int ret_code = execute_user_command(&sock_fd, cmd_request, cmd_response);

    // Cleanup
    close(sock_fd);
    release_lock(lock_fd);
    free(cmd_request);
    free(cmd_response);

    return 0;
}
