#define _GNU_SOURCE
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static int send_all(int fd,const unsigned char *p,int n){while(n){int k=send(fd,p,n,0);if(k<=0)return 0;p+=k;n-=k;}return 1;}
static int ssl_all(SSL *ssl,const unsigned char *p,int n){while(n){int k=SSL_write(ssl,p,n);if(k<=0)return 0;p+=k;n-=k;}return 1;}
static void serve(SSL_CTX *ctx,int fd){unsigned char data[65537];int n=0,need=-1,k,backend=-1;SSL *ssl=SSL_new(ctx);struct addrinfo hint={.ai_socktype=SOCK_STREAM},*addr=0;alarm(180);SSL_set_fd(ssl,fd);
 if(SSL_accept(ssl)!=1)goto done;
 while(n<65536){k=SSL_read(ssl,data+n,65536-n);if(k<=0)goto done;n+=k;data[n]=0;char *end=strstr((char *)data,"\r\n\r\n");if(end&&need<0){int head=(int)(end-(char *)data)+4;long length=0;char *cl=strcasestr((char *)data,"\r\nContent-Length:");if(cl&&cl<end){length=strtol(cl+17,0,10);if(length<0||length>65536-head)goto done;}need=head+(int)length;}if(need>=0&&n>=need)break;}
 if(need<0||n!=need)goto done;
 if(getaddrinfo("backend","8080",&hint,&addr))goto done;
 backend=socket(addr->ai_family,SOCK_STREAM,0);if(backend<0||connect(backend,addr->ai_addr,addr->ai_addrlen))goto done;
 if(!send_all(backend,data,n))goto done;
 while((k=recv(backend,data,sizeof(data)-1,0))>0)if(!ssl_all(ssl,data,k))goto done;
 /* Give the cooperative OEM stack time to drain the final application record
  * before close_notify/FIN; test compatibility with short response bodies. */
 usleep(2000000);
 SSL_shutdown(ssl);
done:if(addr)freeaddrinfo(addr);if(backend>=0)close(backend);SSL_free(ssl);close(fd);}
int main(void){int fd,yes=1;struct sockaddr_in a={.sin_family=AF_INET,.sin_port=htons(443),.sin_addr.s_addr=INADDR_ANY};signal(SIGCHLD,SIG_IGN);signal(SIGPIPE,SIG_IGN);SSL_library_init();SSL_load_error_strings();SSL_CTX *ctx=SSL_CTX_new(SSLv23_server_method());SSL_CTX_set_options(ctx,SSL_OP_NO_SSLv2);SSL_CTX_set_mode(ctx,SSL_MODE_AUTO_RETRY);
 if(!SSL_CTX_set_cipher_list(ctx,"RC4-MD5")||SSL_CTX_use_certificate_file(ctx,"/cert/server.crt",SSL_FILETYPE_PEM)!=1||SSL_CTX_use_PrivateKey_file(ctx,"/cert/server.key",SSL_FILETYPE_PEM)!=1){ERR_print_errors_fp(stderr);return 1;}
 fd=socket(AF_INET,SOCK_STREAM,0);setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));if(bind(fd,(struct sockaddr *)&a,sizeof(a))||listen(fd,8)){perror("listen");return 1;}
 fprintf(stderr,"Legacy SSL frontend listening on 443 (RC4-MD5)\n");for(;;){int c=accept(fd,0,0);if(c<0)continue;if(!fork()){close(fd);serve(ctx,c);_exit(0);}close(c);}}
