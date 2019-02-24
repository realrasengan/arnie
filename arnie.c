// arnie scripting language bot
// copyright (c) MIT
/////////////////////////////////////////////////////
#define DEBUG
#define IDENTIFIER_DEBUG
#define COMPARE_DEBUG
#define SEND_DEBUG
#define EVENTS_DEBUG
#define TOTAL_DEBUG
#define RFC_SIZELIMIT 512*sizeof(char)

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <ctype.h>

#define NICKNAME "arnie"
#define USERNAME "arnie"
#define REALNAME "arnie v0.1"

#define SERVER_IP ""  // numerical IPv4
#define SERVER_PORT 6667

#define SCRIPT_STARTUP "startup.arnie"
#define SCRIPT_EVENTS "events.arnie"

typedef struct IRC_event {
  char command[64];
  char message[256];
  char target[64];
  char response[1024];
  struct IRC_event *next;
} IRC_event;

typedef struct IRC_userhost {
  char nick[32];
  char user[32];
  char host[256];
} IRC_userhost;

typedef struct IRC_message {
  char sender[64];
  char target[64];
  char command[64];
  char message[512];
} IRC_message;

typedef struct IRC_var {
  char name[256];
  char value[1024];
  struct IRC_var *next;
} IRC_var;

char *arnie_identifiers(IRC_event *events, IRC_message *message);
void arnie_proc(int sock, IRC_event *events, IRC_message *message);

int irc_connect(char *ip,int port);
void irc_send(int sock, char *msg, ...);
int irc_read(int sock, char *buffer);

IRC_event irc_events_load();
IRC_message irc_parse(char *msg);
IRC_userhost irc_getuserhost(char *userhost);

void irc_auth(int sock);
void irc_startup(int sock);

void irc_strcpy(char dest[], const char *src);
void irc_strshift(char dest[], const char *src);
char *str_replace(char *orig, char *rep, char *with);

int wild_strcmp(const char *str1, const char *str2);

int main(int argc, char **argv)
{
  int x;
  int sockfd, authed=0, connected=0;
  char read_buffer[512];
  IRC_message ourMessage;
  IRC_event ourEvents,*ptrEvents;

  memset(&read_buffer,0,sizeof(read_buffer));
  memset(&ourMessage,0,sizeof(ourMessage));

  if(!(sockfd = irc_connect(SERVER_IP,SERVER_PORT)))
    return 0;
  printf("Connected to %s:%d\n",SERVER_IP,SERVER_PORT);

  ourEvents=irc_events_load();
  for(ptrEvents=&ourEvents,x=0;ptrEvents;ptrEvents=ptrEvents->next,x++);
  printf("Loaded %d events\n",x);

  while ((irc_read(sockfd,read_buffer)))
  {
#ifdef TOTAL_DEBUG
    printf("RAW: %s\n",read_buffer);
#endif
    if(!authed) {
      irc_auth(sockfd);
      authed++;
    }
    memset(&ourMessage,0,sizeof(ourMessage));
    ourMessage=irc_parse(read_buffer);
    if(!strlen(ourMessage.sender)) {
#ifdef DEBUG
      printf("Command: %s, Message: %s\n",ourMessage.command,ourMessage.message);
#endif
      if(!strcmp(ourMessage.command,"PING"))
        irc_send(sockfd,"PONG %s",ourMessage.message);
    }
    else {
#ifdef DEBUG
      printf("Command: %s, Sender: %s, Target: %s, Message: %s\n",ourMessage.command,ourMessage.sender,ourMessage.target,ourMessage.message);
#endif
      if(!strcmp(ourMessage.command,"376")) {
        if(!connected) {
          printf("Running Startup Scripts\n");
          irc_startup(sockfd);
          connected++;
        }
      }
      else {
        arnie_proc(sockfd, &ourEvents,&ourMessage);
      }
    }
    memset(&read_buffer,0,sizeof(read_buffer));
    memset(&ourMessage,0,sizeof(ourMessage));
  }

  return 0;
}

void arnie_proc(int sockfd, IRC_event *events, IRC_message *message) {
  int x,n;
  char symbol='+';
  char modes[256];
  char modes_rcpt[256];
  char modes_temp[256];
  char *temp=(char *)malloc(sizeof(char)*1024);
  char *modes_who;

#ifdef EVENTS_DEBUG
  IRC_event *irc_event;
  for(irc_event=events;irc_event;irc_event=irc_event->next) {
    printf("Events: Command: %s, Message: %s, Response: %s, Target: %s\n",irc_event->command,irc_event->message,irc_event->response,irc_event->target);
  }
#endif
  for(x=0;events;events=events->next,x++) {
#ifdef COMPARE_DEBUG
    printf("Comparing %s to %s\n",message->command,events->command);
#endif
    if(!strcmp(message->command,events->command)) {

#ifdef COMPARE_DEBUG
      printf("Comparing %s to %s\n",message->target,events->target);
#endif
      if(!strcmp(message->target,events->target) ||
        (strlen(events->target)==1 && events->target[0]=='#' && message->target[0]=='#') ||
        (strlen(events->target)==1 && events->target[0]=='?' && message->target[0]=='?') ||
        (strlen(events->target)==1 && events->target[0]=='*')) {
        if(!strcmp(message->command,"PART")||!strcmp(message->command,"JOIN")||!strcmp(message->command,"KICK")) {
          printf("Firing event %s\n",message->command);
          irc_send(sockfd,arnie_identifiers(events,message));
        }
        else {

          if(!strcmp(message->command,"MODE")) {
#ifdef COMPARE_DEBUG
            printf("Comparing MODE %s to %s\n",message->message,events->message);
#endif

            strcpy(temp,message->message);
            irc_strcpy(modes,strtok(temp," "));
            irc_strcpy(modes_rcpt,strtok(NULL,"\0\r\n"));

            if(*modes_rcpt==':')
              irc_strshift(modes_rcpt,modes_rcpt);

            modes_who=strtok(modes_rcpt," ");
            for(n=0;modes && n<strlen(modes);n++) {
              switch(modes[n]) {
                case '+': symbol='+';
                          break;
                case '-': symbol='-';
                          break;
                case 'o':
                case 'b':
                case 'k':
                case 'l':
                          memset(&modes_temp,0,sizeof(modes_temp));
                          modes_temp[0]=symbol;
                          modes_temp[1]='o';
                          modes_temp[2]=' ';
                          if(modes_who && *modes_who==':')
                            irc_strshift(modes_who,modes_who);
                          strcat(modes_temp,modes_who);
                          modes_who=strtok(NULL," ");
#ifdef COMPARE_DEBUG
                          printf("Comparing MODE SINGLE %s to %s\n",events->message,modes_temp);
#endif
                          if(wild_strcmp(modes_temp,events->message)) {
                            printf("Firing event %s\n",message->command);
                            irc_send(sockfd,arnie_identifiers(events,message));
                          }
                          break;
                default: break;
              }
            }

          }
          else {

#ifdef COMPARE_DEBUG
            printf("Comparing %s to %s\n",message->message,events->message);
#endif
            if(wild_strcmp(message->message,events->message)) {
              printf("Firing event %s\n",message->command);
              irc_send(sockfd,arnie_identifiers(events,message));
            }
          }
        }
      }
    }
  }
}
int wild_strcmp(const char *str1, const char *str2) {
  int n;
  if(strchr(str2,'*')) {
    for(n=0;str1[n];n++) {
      if(!str2[n])
        return 0;
      else if(str2[n]==str1[n])
        continue;
      else {
        if(str2[n]=='*')
          return 1;
        else
          return 0;
      }
    }
    if(n<strlen(str2))
      return 0;
    else
      return 1;
  }
  else if(!strcmp(str1,str2))
    return 1;
  else
    return 0;
}

char *arnie_identifiers(IRC_event *events, IRC_message *message) {
  int x;
  char *identifier;
  char *dest=(char *)malloc(sizeof(char)*1024);
  IRC_userhost userhost=irc_getuserhost(message->sender);

  if(strchr(events->response,'$')) {
    identifier=(char *)malloc(sizeof(char)*1024);
    memset(&identifier,0,sizeof(identifier));
    identifier=events->response;
    while(strstr(identifier,"$target")) {
#ifdef IDENTIFIER_DEBUG
      printf("Replacing $target\n");
#endif
      identifier=str_replace(identifier,"$target",message->target);
    }
    while(strstr(identifier,"$sender")) {
#ifdef IDENTIFIER_DEBUG
      printf("Replacing $sender\n");
#endif

      if(strchr(message->sender,'!'))
        identifier=str_replace(identifier,"$sender",userhost.nick);
      else
        identifier=str_replace(identifier,"$sender",message->sender);
    }
   //TODO: add the $1 $2 $3 shit
//    for(x=1;


   strcpy(dest,identifier);
   free(identifier);
  }
  else {
    dest=realloc(dest,(sizeof(char))*1024);

    strcpy(dest,events->response);
  }
  return dest;
}

void irc_auth(int sock) {
  irc_send(sock, "USER %s hostname %s %s\n",USERNAME,SERVER_IP,REALNAME);
  irc_send(sock, "NICK %s\n",NICKNAME);
}

IRC_message irc_parse(char *msg) {
  int x;
  char *temp=(char *)malloc(RFC_SIZELIMIT);
  IRC_message m_message;

  memset(&temp,0,sizeof(temp));
  memset(&m_message,0,sizeof(m_message));

  if(!strchr(msg, ' ')) {
    return m_message;
  }
  else {
    irc_strcpy(m_message.sender,strtok(msg," "));
    if(m_message.sender[0] == ':') {
      irc_strshift(m_message.sender,m_message.sender);
      irc_strcpy(m_message.command,strtok(NULL, " "));
      irc_strcpy(m_message.target,strtok(NULL," "));
      if((temp=strtok(NULL,"\n\0\r")))
        irc_strcpy(m_message.message,temp);
      else
        irc_strshift(m_message.target,m_message.target);
      if(m_message.message!="") {
        if(m_message.message[0] == ':') {
          irc_strshift(m_message.message, m_message.message);
        }
      }
    }
    else {
      if(m_message.sender!="")
        irc_strcpy(m_message.command,m_message.sender);
      irc_strcpy(m_message.sender,"");
      irc_strcpy(m_message.message,strtok(NULL,"\n\0\r"));
      if(*m_message.message == ':') {
        irc_strshift(m_message.message,m_message.message);
      }
      irc_strcpy(m_message.target,"");
    }
  }
  return m_message;
}

IRC_userhost irc_getuserhost(char *userhost) {
  IRC_userhost ret;
  if(strchr(userhost,'!')) {
    irc_strcpy(ret.nick, strtok(userhost,"!"));
    irc_strcpy(ret.user, strtok(NULL,"@"));
    irc_strcpy(ret.host, strtok(NULL,"\n\r\0"));
  }
  return ret;
}

int irc_read(int sock, char *buffer) {
  int n;

  char return_val[512];
  char *one_char=(char *)malloc(sizeof(char));

  memset(&return_val,0,sizeof(return_val));

  for(n=0;n<RFC_SIZELIMIT;n++) {
    if(!read(sock, one_char, sizeof(char)))
      return 0;
    else
      if(*one_char=='\n')
         break;
      else
        return_val[n]=*one_char;
  }
  if(n==0)
    return 0;
  if(n<RFC_SIZELIMIT/sizeof(char))
    return_val[n] = '\0';
  else
    return_val[n-1] = '\0';
  irc_strcpy(buffer,return_val);
  free(one_char);
  return 1;
}
void irc_send(int sock, char *msg, ...) {
  va_list args;

  char *write_val=(char *)malloc(RFC_SIZELIMIT*2);

  if(!msg || strlen(msg)<=1)
    return;

  va_start(args, msg);
  vsprintf(write_val, msg, args);
  va_end(args);


#ifdef SEND_DEBUG
  printf("Sending: %s\n",write_val);
#endif
  write(sock,write_val,strlen(write_val)+1);
  write(sock,"\n",sizeof(char));
  free(write_val);
}

int irc_connect(char *ip,int port) {
  struct sockaddr_in serv_addr;

  int sockfd = 0;

  memset(&serv_addr, '0', sizeof(serv_addr));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);

  if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    return 0;
  if(inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr)<=0)
    return 0;
  if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
     return 0;

  return sockfd;
}

IRC_event irc_events_load() {
  IRC_event irc_event_,*irc_event;
  FILE *fp;

  int first=1;
  char buffer[512];
  irc_event=&irc_event_;
  if(!(fp=fopen(SCRIPT_EVENTS,"r"))) {
    printf("\nError: Unable to open %s\n",SCRIPT_EVENTS);
    return irc_event_;
  }
  else {
    while(fgets(buffer,sizeof(buffer),fp)) {
#ifdef EVENTS_DEBUG
      printf("Events: %s\n",buffer);
#endif
      if(strchr(buffer,':')) {
        if(first)
          first=0;
        else {
          irc_event->next=(IRC_event *)malloc(sizeof(IRC_event));
          irc_event=irc_event->next;
        }
        irc_strcpy(irc_event->command,strtok(buffer,":"));
        if(!strcmp(irc_event->command,"JOIN") || !strcmp(irc_event->command,"KICK") || !strcmp(irc_event->command,"PART")) {
#ifdef EVENTS_DEBUG
          printf("Events: Detected JOIN, KICK, or PART\n");
#endif
          strcpy(irc_event->message,strtok(NULL,":"));
          strcpy(irc_event->target,irc_event->message);
          strcpy(irc_event->response,strtok(NULL,"\n"));
        }
        else {
#ifdef EVENTS_DEBUG
          printf("Regular event\n");
#endif
          strcpy(irc_event->message,strtok(NULL,":"));
          strcpy(irc_event->target,strtok(NULL,":"));
          strcpy(irc_event->response,strtok(NULL,"\n"));
        }
      }
    }
  }
  irc_event->next=NULL;
  fclose(fp);
  return irc_event_;
}
void irc_startup(int sock) {
  FILE *fp;
  char buffer[512];

  if(!(fp=fopen(SCRIPT_STARTUP,"r"))) {
    printf("\nError: Unable to open %s\n",SCRIPT_STARTUP);
    return;
  }
  else {
    while(fgets(buffer,sizeof(buffer),fp)) {
      if(strlen(buffer)>0) {
       irc_send(sock,buffer);
      }
    }
  }
  fclose(fp);
}

void irc_strcpy(char dest[], const char *src) {
  int n=0,src_length = strlen(src);
  for(n=0;!iscntrl(src[n]);dest[n]=src[n],n++);
  dest[n]='\0';
}
void irc_strshift(char dest[], const char *src) {
  int n=0,src_length = strlen(src);
  for(n=0;src[n+1];dest[n]=src[n+1],n++);
  dest[n]='\0';
}

// You must free the result if result is non-NULL.
// from stack overflow
char *str_replace(char *orig, char *rep, char *with) {
    char *result; // the return string
    char *ins;    // the next insert point
    char *tmp;    // varies
    int len_rep;  // length of rep (the string to remove)
    int len_with; // length of with (the string to replace rep with)
    int len_front; // distance between rep and end of last rep
    int count;    // number of replacements

    // sanity checks and initialization
    if (!orig || !rep)
        return NULL;
    len_rep = strlen(rep);
    if (len_rep == 0)
        return NULL; // empty rep causes infinite loop during count
    if (!with)
        with = "";
    len_with = strlen(with);

    // count the number of replacements needed
    ins = orig;
    for (count = 0; tmp = strstr(ins, rep); ++count) {
        ins = tmp + len_rep;
    }

    tmp = result = (char *)malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result)
        return NULL;

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig);
    return result;
}
