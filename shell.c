#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include "parse.h"   /*include declarations for parse-related structs*/
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
struct job{
    int pid;
    char * command;
};

typedef struct{
    int length;
    int size;
    struct job **jobs;
}job_list;

void init_job_list(job_list * return_jobs, int list_size){
    return_jobs -> size = list_size;
    return_jobs -> length = 0;
    return_jobs -> jobs = malloc(sizeof(struct job *) * list_size);
}

void add_job(job_list * j, char * job_command, int job_pid){
    struct job * new_job = malloc(sizeof(struct job));
    new_job->pid = job_pid;
    new_job->command  = malloc(sizeof(char)*strlen(job_command));
    strcpy(new_job->command,job_command);
    if(j->length + 1 > j->size){
        int i;
        struct job ** new_jobs;
        j->size += 10;
        new_jobs = malloc(sizeof(struct job *) * j->size);
        for(i=0; i < j->length; i++){
            new_jobs[i] = j->jobs[i];
        }
        free(j->jobs);
        j->jobs = new_jobs;
    }
        j->length++;
    j->jobs[j->length-1] = new_job;
}

void remove_job(job_list * j, int job_num){
    int i;
    free(j->jobs[job_num]->command);
    free(j->jobs[job_num]);
    j->jobs[job_num] = NULL;
    j->length--;
    for(i = job_num; i < j->length; i++ ){
        j->jobs[i] = j->jobs[i+1];
    }
    
}

void update_jobs(job_list * j){
    int i = 0;
    while( i < j->length){
        if(waitpid(j->jobs[i]->pid,NULL,WNOHANG) != 0){
            remove_job(j,i);
        }
    else{
        i++;
    }
    }
}

enum
BUILTIN_COMMANDS { NO_SUCH_BUILTIN=0, EXIT,JOBS,CD,HISTORY,KILL,HELP,XHISTORY};
 
char *
buildPrompt()
{
  return  "%";
}


int
isBuiltInCommand(char * cmd){
  
  if ( strncmp(cmd, "exit", strlen("exit")) == 0){
    return EXIT;
  }
  else if( strncmp(cmd, "jobs", strlen("jobs")) == 0){
      return JOBS;
  }
  else if( strncmp(cmd, "help", strlen("help")) == 0){
      return HELP;
  }
  else if( strncmp(cmd, "cd", strlen("cd")) == 0){
      return CD;
  }
  else if( strncmp(cmd, "history", strlen("history")) == 0){
      return HISTORY;
  }
  else if(strncmp(cmd, "kill", strlen("kill")) == 0){
      return KILL;
  }
  else if(strncmp(cmd, "!", strlen("!")) == 0){
      return XHISTORY;
  }
  else{
  return NO_SUCH_BUILTIN;
  }
  
}


int 
main (int argc, char **argv)
{

  char cwd[256];/*buffer for current directory*/
  char * cmdLine;
  parseInfo *info; /*info stores all the information returned by parser.*/
  struct commandType *com; /*com stores command name and Arg list for one command.*/
  int built_in;/* variable for the type of built in command*/
  const char * hist_ptr = NULL;/*if not null excecute comand instead of readline*/
  int child_pid;
    FILE * in_file;
    FILE * out_file;
    job_list current_jobs;
    init_job_list(&current_jobs,10);
    
    



    using_history();
  while(1){
    /*insert your code to print prompt here*/
      
      if(hist_ptr == NULL){
      getcwd(cwd, sizeof(cwd));
      printf("%s",cwd);
     

    cmdLine = readline(buildPrompt());
    if (cmdLine == NULL) {
      fprintf(stderr, "Unable to read command\n");
      continue;
    }

      }
      else{
          cmdLine = (char *) malloc(sizeof(*hist_ptr));
          strcpy(cmdLine, hist_ptr);
          hist_ptr = NULL;
      }
     
   
     
    /*calls the parser*/
    info = parse(cmdLine);
    if (info == NULL){
      free(cmdLine);
      continue;
    }
      
    

    /*com contains the info. of the command before the first "|"*/
    com=&info->CommArray[0];
    if ((com == NULL)  || (com->command == NULL)) {
      free_info(info);
      free(cmdLine);
      continue;
    }
   
    
    /*com->command tells the command name of com*/
    built_in =isBuiltInCommand(com->command);
    if(built_in != XHISTORY)/*dont record repeat history executions*/
    add_history(cmdLine);
      
      if(info->boolInfile){
          in_file = fopen(info->inFile,"r");
          if(in_file == NULL){
              fprintf(stderr,"Invalid file name\n");
              free_info(info);
              free(cmdLine);
              continue;
          }
      }
      else{
          in_file = stdin;
      }
      if(info->boolOutfile){
          out_file = fopen(info->outFile,"w");
          if(out_file == NULL){
              fprintf(stderr,"Invalid file name\n");
              free_info(info);
              free(cmdLine);
              continue;
          }
      }
      else{
          out_file = stdout;
      }
    if (built_in == EXIT){
      update_jobs(&current_jobs);
        if(current_jobs.length > 0){
            fprintf(stderr,"There are still background jobs running\n");
        }
        else
      exit(1);
    }
    else if(built_in == JOBS){
        
        update_jobs(&current_jobs);
        if(current_jobs.length == 0){
            fprintf(out_file,"no jobs are currently running\n");
        }
        else{
         int i;
            for(i = 0; i < current_jobs.length; i++)
                fprintf(out_file,"[%d] %s PID:%d\n",i,current_jobs.jobs[i]->command,current_jobs.jobs[i]->pid);
        }
        
    }
    else if(built_in == CD){
        if( com->VarList[1] != NULL)
            chdir(com->VarList[1]);
    }
    else if(built_in == HISTORY){
        int i;
        
        for(i = 0; i < history_length; i++){
        history_set_pos(i);
        fprintf(out_file,"[%d] %s\n",i,current_history()->line);
        }
    }
    else if(built_in == KILL){
        update_jobs(& current_jobs);
        if(com->VarList[1] != NULL){
        if(com->VarList[1][0] == '%' && strlen(com->VarList[1]) > 1){
            int index = atoi(com->VarList[1]+1);
            if(index < current_jobs.length){
                kill(current_jobs.jobs[index]->pid, SIGKILL);
            }
            else{
                fprintf(stderr,"job number doesn not exit\n");
            }
        }
        else if(strncmp(com->VarList[1], "all", strlen("all")) == 0){
            int i;
            update_jobs(& current_jobs);
            for(i = 0 ; i < current_jobs.length; i++)
                kill(current_jobs.jobs[i]->pid, SIGKILL);
        }
            else{
                fprintf(stderr,"invalid input: kill %%jobnum where jobnum is from jobs command or kill all to kill all background jobs\n");
                
            }
        }
        else
            fprintf(stderr,"invalid input: kill requires the argument %%jobnum or all\n");
        
    }
    else if(built_in == HELP){
        fprintf(out_file,"exit: terminate shell\nhistory: lists history of previous commands\n!number will repeat the command number listed in history\n!-1 will repeat the last command issued, !-2 will repeat the second last, ect. \njobs: lists the active backround jobs that were ran with &\nkill: terminates background jobs\nthe argument %%num will terminate the job n from the job list\nthe argument all will terminate all jobs\ncd: changes the current directory to the argument provided\n");
    }
    else if(built_in == XHISTORY){
        if(strlen(com->VarList[0])>1){
       int index= atoi(com->VarList[0]+1);
            
          if(index < history_length && index >= (history_length) * -1){
              
              if(index >= 0){
               history_set_pos(index);
              }
              else{
                  index += history_length;
                  history_set_pos(index);
              }
           hist_ptr = current_history()->line;
           }
          else{fprintf(stderr,"invalid history index \n");}
        }
        else {fprintf(stderr,"! requires an appended number from history listing\n");}
    }
    else if(built_in == NO_SUCH_BUILTIN){
        
         child_pid = fork();
        if(child_pid == 0){
            
            dup2(fileno(out_file),1);
            dup2(fileno(in_file),0);
            execvp(com->command,com->VarList);
            
            fprintf(stderr,"%s: command not found\n",com->command);
            exit(1);
            
            
        }
        else{
            if(info->boolBackground){
                add_job(& current_jobs, com->command, child_pid);
            }
            else{
            waitpid(child_pid, NULL, 0);
            }
            
        }
        
    }
      fflush(out_file);
   

    free_info(info);
    free(cmdLine);
  }
}
  





