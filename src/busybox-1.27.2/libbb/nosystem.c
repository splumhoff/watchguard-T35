/**
 *
 * nosystem.c
 *
 * Replace system() 
 *
 * Copyright &copy; 2012, WatchGuard Technologies, Inc.
 * All Rights Reserved
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 *        
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>

/* Prototypes */
int nosystem(const char *cmd);

static char ** nosystem_parse_tokens(const char *input, int *argc_ptr);
static void nosystem_free_tokens(char ***tokens);
static int nosystem_path_find(const char *file, char *buffer, size_t buflen);

/*
 * nosystem_parse_tokens()
 *
 * Supply a string and a pointer-to-int, and this will parse your string
 * into white-space separated tokens and return a char** that points at
 * malloced memory containing the array.  Free the memory when done
 * with 'nosystem_free_tokens()'.
 *
 * The list will always contain a NULL as the last token, but that will
 * not be reflected in the count of the int* passed.  This is handy when
 * you want to pass the tokens into something like execvp(), which expects
 * a NULL-terminated char**.
 *
 * simple quotation is supported, ie:
 *
 *        tok1 tok2 "tok 3" "'tok 4'  "  'tok 5'
 *
 * would return: (tok1) (tok2) (tok 3) ('tok 4'  ) (tok 5) (NULL)
 *
 * No escaping is supported, so "\"" is just (\") literally.
 *
 * If the quotes are not ballanced, then you will probably get the rest
 * of the string as the last token.
 *
 * See Also: nosystem_free_tokens()  ( for a complete example )
 *
 *
 * input - string to tokenize
 *
 * argc_ptr - pointer to number of tokens
 *            set by function
 *
 * Return: char ** - a pointer to the list of tokens.
 *                   NULL - list not created
*/

static char ** nosystem_parse_tokens(const char *input, int *argc_ptr) {
  void *blk = NULL;
  int *argc = argc_ptr;
  int argc_val = 0;

  if (!input) {
      return 0;
  }
  
  if (argc_ptr == 0) {
    argc = &argc_val;
  }
  
  *argc = NULL;

  if(input) {

    /* 100907.EN: Make sure that we also copy the \nul byte such that
     * we don't overrun our buffers in the while( *str ) loop.
     */
    int len = strlen((char *)input) + 1;

    /*
     * 070207.AJK: Make sure blk is big enough to hold the input, with NULL
     * terminator, and character pointers for each character in the input.
     */
    blk = calloc(len+1+(sizeof(char*)*len), sizeof(char));

    if(blk) {
      char **argv = blk;
      char *str, *tok;

      str = blk + (sizeof(char*)*len);
      memcpy(str, input, len);

      while( *str ) {

          /* Skip past break characters in case there are multiple in a row */
          while( *str=='\r' || *str=='\n' || *str==' ' || *str=='\t' ) {
              str++;
          }
          
          tok = str;
            
          /* Skip until matching quote: '\"' or '\'' */
          if(*str == '"' || *str == '\'') {

              for(tok=str+1; *tok && *tok != *str; tok++) {
                  /* noop */ 
              }
              str += 1;
          } else {
              
              /* Skip until break character: '\0' '\n' '\r' '\t' or ' ' */
              for(tok=str; *tok && *tok != '\r' && *tok != '\n' && *tok != ' ' && *tok != '\t'; tok++ ) { 
                  /* noop */ 
              }
          }

          if (tok >= str) {
              argv[ *argc ] = str;
              *tok = 0x00;
              *argc += 1;
          }

          str = tok + 1;
      }
    }
  }

  return( (char **)blk );
}


/*
 * nosystem_free_tokens()
 * free tokens that were parsed/allocated with 'nosystem_parse_tokens()'
 *
 * It is OK to pass-in a NULL, or a pointer to NULL.
 *
 * The pointer will be set to NULL when the memory is freed, so you can
 * safely re-use your pointer for another parse -- just be sure to always
 * initialize your pointers.  This is an example of using these routines:
 *
 *   char **tokens = NULL;
 *   int argc;
 *  
 *   // etc... open a file, etc... 
 *
 *   while((line = fgets(buf, sizeof(buf), fh))) {
 *      nosystem_free_tokens( &tokens );  // free last tokenization (if any)
 *      if((tokens = nosystem_parse_tokens(buf, &argc))) {
 *        // -- do something with the tokens
 *      }
 *   }
 *   nosystem_free_tokens( &tokens );  // free last tokenizatio (if any)
 *
 *
 * @param tokens - pointer to tokens to free
 *
 * @retval none
*/
static void nosystem_free_tokens(char ***tokens) {
    if ( ((void*)*tokens ) ) {
        free( (void*)*tokens );
        *tokens = 0;
    }
}

/**
 *
 * nosystem_path_find.c
 *
 * determines if file is in the PATH.
 *
 *      The function looks through the directories in the PATH
 *      environment variable for a file. If found it fills in the
 *      passed buffer with a full path to the file.
 *        
 * file - file to find
 *
 * buffer - supplied buffer
 *
 * buflen - length of buffer
 *
 * Return 0: file found
 *        1: program not found
 *       -1: error
 *
*/
static int nosystem_path_find(const char *file, char *buffer, size_t buflen) {
    const char *path = getenv("PATH");
    char buf[1024];
    int mylen, err = 1;

   if (!file || !buffer || !buflen) {
       err = -1;
       goto done;
   }
  
   /*
    * first check and see if 'file' exists, as-is -- but don't check unless
    * there is path-info in 'file' -- this keeps 'foo' in the current dir
    * from getting matched if 'file' is 'foo' -- ie: 'file' must be './foo'
   */
   if(strstr(file, "/") && access(file, X_OK) == 0) {
       strncpy(buffer, file, buflen);
       err = 0;
       goto done;
   }

   mylen = strlen(file);
   *buffer = 0;

   if(path == NULL) {
       path = "/bin:/usr/bin";
   }

   while(path && *path) {
       const char *e = path;
       for(e=path; e && *e; e++) {
           if(*e == ':') break;
       }
       
       if((e - path) && ((e - path) < (sizeof(buf) - 1 - mylen))) {
           memcpy(buf, path, e - path);
           buf[e-path] = 0;
           strcat(buf, "/");
           strcat(buf, file);

           if(access(buf, X_OK) == 0) {
               strncpy(buffer, buf, buflen);
               err = 0;
               break;
           }

       }
       
       path = *e ? e + 1 : e;
   }

done:
   return(err);
}

extern char **environ;

/*
 * nosystem()
 *
 * This function can be used in place of the stdlib 'system(3)'
 * command, when you have a system with no '/bin/sh' -- note that you will not
 * be able to do things that a shell can do, but if you need to run an
 * external command that does not need shell metacharacter expansion or
 * other shell capabilities, then this is for you!
 *
 * SEE ALSO: 
 *
 *     system(3)
 *
 * Example:
 *
 *     int rc;
 *      
 *     rc = nosystem("mkfs.ext3 -j /dev/ram0");
 *
 *     if(rc < 0) printf("formatting failed!\n"); 
 *     else printf("ramdisk formatted ok.\n");
 *
 *
 * cmd -- The command-line you wish to execute.  The command at the
 *            beginning of the string need not have full path info, as
 *            the envvar PATH will be searched (but you may include full
 *            path info as well -- it speeds things up)
 *
 *
 * return >=0 - return code of command
 *
 */

int nosystem(const char *cmd) {
  pid_t pid = 0;
  char **av = NULL, exe[PATH_MAX];
  int ac = -1, rc = -1, status = 0;
  __sighandler_t s_quit, s_int, s_chld;

  if (cmd == NULL) {
      goto done;
  }

  /* parse command line */
  if((av = nosystem_parse_tokens(cmd, &ac)) == NULL) {
      goto done;
  }
  
  /* find executable in path */
  if(nosystem_path_find(av[0], exe, PATH_MAX - 1) != 0) {
    strncpy(exe, av[0], PATH_MAX - 1);
  }

  /* set signal handlers - ignore SIGQUIT and SIGINT */
  s_quit = signal(SIGQUIT, SIG_IGN);
  s_int = signal(SIGINT, SIG_IGN);
  s_chld = signal(SIGCHLD, SIG_DFL);

  /* fork and exec new process */
  if((pid = fork()) == 0) {
      char *argv[ac+1];
      int i = 0;
    
      memset(argv, 0, sizeof(argv));
      for (i=1; i<ac; i++) {
          argv[i] = av[i];
      }
    
      argv[0] = exe;
      signal(SIGQUIT, SIG_DFL);
      signal(SIGINT, SIG_DFL);
      signal(SIGCHLD, SIG_DFL);

      /*
      **TODO: do we really wanat to use execve() here ?? 
      */
      execve(argv[0], argv, environ);
      exit(-1);
  } else if (pid < 0) {
      goto fix_signals;
  }

  signal(SIGQUIT, SIG_IGN);
  signal(SIGINT, SIG_IGN);

  if (wait4(pid, &status, 0, 0) > -1) {
      if(WIFEXITED(status)) {
          rc = WEXITSTATUS(status);
      }
  }

fix_signals:
  signal(SIGQUIT, s_quit);
  signal(SIGINT, s_int);
  signal(SIGCHLD, s_chld);

done:
  if (av != NULL ) {
      nosystem_free_tokens(&av);
  }
  
  return(rc);
}
