#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <cctype>
#include <string>
#include <vector>
using namespace std;
#include "noncanmode.h"
#define MAX 4096

#include <iostream>

void printPrompt()
{
  char buffer[256];
  int i;

  if (getcwd(buffer, 256) != NULL)
  {
    if (strlen(buffer) > 16)
    {
      write(STDOUT_FILENO, "/.../", 5);

      for (i = strlen(buffer) - 1; buffer[i] != '/'; i--);

      write(STDOUT_FILENO, &buffer[i+1], strlen(buffer) - i);
      write(STDOUT_FILENO, "% ", 2);
    }

    else
    {
      write(STDOUT_FILENO, &buffer, strlen(buffer));
      write(STDOUT_FILENO, "% ", 2);
    }
  }
} // printPrompt()

int hindex = 0;
vector<char *> history;

void storeInput(char* input)
{
  char *newstr = (char *) malloc(strlen(input) + 1);
  strcpy(newstr, input);

  if(hindex == (int) history.size())
    hindex = history.size() + 1;
  if (history.size() == 10)
  {
    history.erase(history.begin());
    history.push_back(newstr);
  }

  else
    history.push_back(newstr);
} // storeInput() - stores input in history

char* arrowKey(char* input)
{
  char c;
  bool changed = false;
  read(STDIN_FILENO, &c, 1);

  if ((int) c == 91)
  {
    read(STDIN_FILENO, &c, 1);

    if ((int) c == 65)
    {
      if(hindex > 0)
      {
        hindex--;
        changed = true;
      }
    }
    else if ((int) c == 66)
    {
      if(hindex < (int) history.size()) {
        hindex++;
        changed = true;
      }
    }

    if(hindex <= (int) history.size() && changed)
    {
      for (int i = strlen(input) - 1; i >= 0; i--)
        write(STDOUT_FILENO, "\b \b", 3);
    }

     if (hindex < (int) history.size() && changed)
      {
        write(STDOUT_FILENO, history[hindex], strlen(history[hindex]));
        input = history[hindex];
      }

      else
      {
        //input[0] = '\0';
      }
    } // down arrow key

  return input;
} // arrowKey() - takes care of up/down keys

void backspace(char* input, int& i)
{
  if (i == 0)
    write(STDOUT_FILENO, "\a", 1);

  else
  {
    write(STDOUT_FILENO, "\b \b", 3);
    input[i] = '\0';
    --i;
  }
} // backspace() - implements backspace functionality

void reallocating(char* inp, int& size)
{
  size *= 2;
  inp = (char*) realloc(inp, size);
} // reallocating() - reallocates double the memory

char* readInput()
{
  struct termios noncanmode;
  char c; 
  int i = 0; 
  int size = MAX; 
  //int count = history.size();

  char* input = (char*) calloc(sizeof(char), size);
  if (!input)
    write(STDERR_FILENO, "input calloc error", 18);

  SetNonCanonicalMode(STDIN_FILENO, &noncanmode);

  do
  {
    read(STDIN_FILENO, &c, 1);
    
    if (isprint(c))
    {
      write(STDOUT_FILENO, &c, 1);
      input[i] = c;
      i++;
    } // if char is printable, store and print
    
    else if (c == '\n')
    {
      if (input[0])
        storeInput(input);
      
      break;
    } // store input and then break out of loopk
    
    else if ((int) c == 27)
    { 
      input = arrowKey(input);
      i = strlen(input);
    }

    else if (c == 0x7f)
      backspace(input, i);

    if (i == size)
      reallocating(input, size);
  } while (1);

  ResetCanonicalMode(STDIN_FILENO, &noncanmode);
  write(STDOUT_FILENO, "\n", 1);

  return input;
} // readInput()

char** tokenize(char* inp)
{ 
  int size = 256;
  int pos = 0;
  char* token;
  char** tokens = (char**) calloc(size, sizeof(char*));

  if (!tokens)
    write(STDERR_FILENO, "token malloc error", 18);

  token = strtok(inp, " ");

  while (token != NULL)
  {
    tokens[pos] = token;
    pos++;

    if (pos >= size)
    {
      size *= 2;
      tokens = (char**) realloc(tokens, size);

      if (!tokens)
        write(STDERR_FILENO, "token realloc error", 19);
    } 

    token = strtok(NULL, " ");
  }

  return tokens;
} // tokenize()

void cd(char** args)
{
  struct stat buffer;

  if (!args[1])
  {
    string str = "/home/";
    char* home = getlogin();
    str.append(home);
    const char* result = str.c_str();
    chdir(result);
  }

  else if (stat(args[1], &buffer) == 0 && S_ISREG(buffer.st_mode))
  {
    write(STDERR_FILENO, args[1], strlen(args[1]));
    write(STDERR_FILENO, " not a directory!\n", 18);
  }

  else
  {
    if (chdir(args[1]) == -1)
      write(STDERR_FILENO, "Error changing directory\n", 25);
  }
} // cd()

void printFileMode(mode_t mode)
{
  const char *permissions = "rwxrwxrwx"; 
  const int masks[] = {S_IRUSR, S_IWUSR, S_IXUSR, S_IRGRP, S_IWGRP, S_IXGRP, S_IROTH, S_IWOTH, S_ISVTX};

  if(S_ISDIR(mode))
    write(STDOUT_FILENO, "d", 1);
  else
    write(STDOUT_FILENO, "-", 1);

  for(int i = 0; i < 9; i++)
  {
    if(mode & masks[i])
      write(STDOUT_FILENO, &permissions[i], 1);
    else
      write(STDOUT_FILENO, "-", 1);
  }

  write(STDOUT_FILENO, " ", 1);
}

void ls(char** tokens)
{
  DIR *d;
  struct dirent *entry;
  char *dirPath;
  char *currentDir = strdup(".");
  char filePath[4096];
  struct stat buffer;
  
  if(*(++tokens) && **(tokens) != '>' && **(tokens) != '<' && **(tokens) != '|')
  { 
    dirPath = *tokens;
  }
  else
    dirPath = currentDir;

  d = opendir(dirPath);
  
  if(!d)
  { 
    write(STDERR_FILENO, "Failed to open directory \"", 27);
    write(STDERR_FILENO, dirPath, strlen(dirPath));
    write(STDERR_FILENO, "/\"", 2);
    return;
  }
  
  do
  {
    if((entry = readdir(d)) != NULL) {
      if(strcmp(dirPath, ".") == 0)
      { 
        getcwd(filePath, 256); 
        strncat(filePath, "/", 256);
      }
      else
      {
        strcpy(filePath, dirPath);
        strcat(filePath, "/");
      }

      strncat(filePath, entry->d_name, 256);

      stat(filePath, &buffer);
      printFileMode(buffer.st_mode);

      write(STDOUT_FILENO, entry->d_name, strlen(entry->d_name));
      write(STDOUT_FILENO, "\n", 1);
    }
  } while(entry);

  closedir(d);
  return;
} // ls()

void pwd(char** args)
{
  char buffer[256];
  getcwd(buffer, 256);
  write(STDOUT_FILENO, buffer, strlen(buffer));
  write(STDOUT_FILENO, "\n", 1);
} // pwd()

void ff(char** tokens)
{
  DIR *d;
  char *dirPath;
  char *currentDir = strdup(".");
  char newPath[4096];
  char *filename;
  struct dirent *entry;
  struct stat buffer;

  filename = *(++tokens);

  if(*(++tokens) && **(tokens) != '>' && **(tokens) != '<' && **(tokens) != '|')
    dirPath = *tokens;
  else
    dirPath = currentDir;

  d = opendir(dirPath);

  if(!d)
  {
    write(STDERR_FILENO, "Failed to open directory \"", 27);
    write(STDERR_FILENO, dirPath, strlen(dirPath));
    write(STDERR_FILENO, "/\"\n", 3);
    
    return;
  }

  do
  {
    if((entry = readdir(d)) != NULL) {
      if(strcmp(dirPath, ".") == 0)
      {
        getcwd(newPath, 256);
        strncat(newPath, "/", 256);
      }
      else
      {
        strcpy(newPath, dirPath);
        strcat(newPath, "/");
      }

      strncat(newPath, entry->d_name, 256);
      lstat(newPath, &buffer);

      if(S_ISLNK(buffer.st_mode))
        continue;

      if(S_ISDIR(buffer.st_mode) && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
      {
        char **args = (char **)malloc(sizeof(char *) * 3);
        args[1] = filename;
        if(dirPath[0] == '.')
          args[2] = entry->d_name;
        else
          args[2] = newPath;
  
        ff(args);
        free(args);
      }

      if(strcmp(entry->d_name, filename) == 0 && S_ISREG(buffer.st_mode))
      {
        if(dirPath[0] != '.')
          write(STDOUT_FILENO, "./", 2);
        write(STDOUT_FILENO, dirPath, strlen(dirPath));
        write(STDOUT_FILENO, "/", 1);
        write(STDOUT_FILENO, entry->d_name, strlen(entry->d_name));
        write(STDOUT_FILENO, "\n", 1);
      }
    }
  } while(entry);

  closedir(d);
  return;
} // ff()

bool processTokens(char** args)
{
  int i = 0;
  int pipes[256];
  int start = 0;
  int numChildren = 0;
  int offsets[256] = {0};
  int inFile = 0;
  int outFile = 0;
  int end;

  if (!args[0])
    return true;

  if(strcmp(args[0], "cd") == 0)
  {
    cd(args);
    return true;
  }
  else if(strcmp(args[0], "exit") == 0)
    return false;

  while (args[i])
  {
    if (strcmp(args[i], "|") == 0)
    {
      args[i] = NULL; // remove the pipe characeter
      offsets[numChildren++] = start;
      start = i + 1; // start checking from next token
    }
    i++;
  } // check for pipes

  offsets[numChildren++] = start;

  // Create pipes
  for(int j = 0; j < numChildren - 1; j++)
    pipe(&pipes[j*2]);

  // Execute each child
  for(int j = 0; j < numChildren; j++)
  {
    int pid = fork();

    if(pid == 0) {
      i = offsets[j];
      inFile = 0;
      outFile = 0;
      if(j != (numChildren -1))
        end = offsets[j+1];
      else
        end = -1;

      // Check for redirection
      while(args[i] && i != end)
      {
        // input from file
        if(strcmp(args[i], "<") == 0) {
          inFile = open(args[i+1], O_RDONLY);

          if(inFile < 0)
          {
            write(STDOUT_FILENO, "File \"", 6);
            write(STDOUT_FILENO, args[i+1], strlen(args[i+1]));
            write(STDOUT_FILENO, "\" does not exist!\n", 18);
            exit(1);
          }

          dup2(inFile, STDIN_FILENO);
          close(inFile);
          args[i] = NULL; // arguments for program stop here
        }
        // output to file
        else if(strcmp(args[i], ">") == 0) {
          outFile = creat(args[i+1], 0644);

          if(outFile < 0)
          {
            write(STDOUT_FILENO, "File \"", 6);
            write(STDOUT_FILENO, args[i+1], strlen(args[i+1]));
            write(STDOUT_FILENO, "\" does not exist!\n", 18);
            exit(1);
          }

          dup2(outFile, STDOUT_FILENO);
          close(outFile);
          args[i] = NULL;
        }
        i++;
      }

      // Use the pipes if we aren't redirecting input/output to/from a file already
      if(j < numChildren-1 && outFile == 0)
        dup2(pipes[j*2 + 1], STDOUT_FILENO);
      if(j > 0 && inFile == 0)
        dup2(pipes[(j-1)*2], STDIN_FILENO);
      for(int k = 0; k < (numChildren - 1)*2; k += 2)
      {
        close(pipes[k]);
        close(pipes[k+1]);
      }

    // Handle built-in commands
    if (strcmp(args[offsets[j]], "ls") == 0)
    { 
      ls(&args[offsets[j]]);
      exit(0);
    }
    else if (strcmp(args[offsets[j]], "pwd") == 0)
    { 
      pwd(&args[offsets[j]]);
      exit(0);
    }
    else if (strcmp(args[offsets[j]], "ff") == 0)
    { 
      ff(&args[offsets[j]]);
      exit(0);
    }
    
    // Not handled internally so we execute something else.
    else if (execvp(args[offsets[j]], &args[offsets[j]]) < 0)
    {
      write(STDOUT_FILENO, "Failed to execute ", 18);
      write(STDOUT_FILENO, args[offsets[j]], strlen(args[offsets[j]]));
      write(STDOUT_FILENO, "\n", 1);
      exit(1);
    }
    }
  }

  // Pipes that were duplicated can be closed
  for(int j = 0; j < (numChildren - 1)*2; j += 2)
  {
    close(pipes[j]);
    close(pipes[j+1]);
  }

  // Wait for all children to terminate
  for(int j = 0; j < numChildren; j++)
    wait(NULL);

  return true;
} // processTokens()

int main(int argc, char** argv)
{
  bool cont;
  char* inp;
  char** args;
  do
  {
    printPrompt();
    //inp = readInput(history, index, count);
    inp = readInput();
    args = tokenize(inp);
    cont = processTokens(args);
  } while (cont == true);

  free(inp);
  return (0);
} // main()
