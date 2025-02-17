#include "shelpers.hpp"
#include <cassert>
#include <ostream>
#include <sys/_types/_pid_t.h>
#include <sys/wait.h>
#include <signal.h>

//////////////////////////////////////////////////////////////////////////////////
//
// Author: Ben Jones (I think) with a lot of clean up by J. Davison de St. Germain
//
// Date:   2019?
//         Jan 2022 - Cleanup
//
// Class: CS 6013 - Systems I
//
//////////////////////////////////////////////////////////////////////////////////

using namespace std;

////////////////////////////////////////////////////////////////////////
// Example test commands you can try once your shell is up and running:
//
// ls
// ls | nl
// cd [dir]
// cat < shelpers.cpp
// cat < shelpers.cpp | nl
// cat shelpers.cpp | nl
// cat shelpers.cpp | nl | head -50 | tail -10
// cat shelpers.cpp | nl | head -50 | tail -10 > ten_lines.txt 
//
// - The following two commands are equivalent.  [data.txt is sent into nl and the
//   output is saved to numbered_data.txt.]
//
// nl > numbered_data.txt < data.txt
// nl < data.txt > numbered_data.txt 
//
// - Assuming numbered_data.txt has values in it... try running:
//   [Note this probably doesn't work like one might expect...
//    does it behave the same as your normal shell?]
//
// nl < numbered_data.txt > numbered_data.txt
//
// - The following line is an error (input redirection at end of line).
//   It should fail gracefully (ie, 1) without doing anything, 2) cleaning
//   up any file descriptors that were opened, 3) giving an appropriate
//   message to the user).
//
// cat shelpers.cpp | nl | head -50 | tail -10 > ten_lines.txt < abc
// 

////////////////////////////////////////////////////////////////////////
// This routine is used by tokenize().  You do not need to modify it.

bool splitOnSymbol( vector<string> & words, int i, char c )
{
   if( words[i].size() < 2 ){
      return false;
   }
   int pos;
   if( (pos = words[i].find(c)) != string::npos ){
      if( pos == 0 ){
         // Starts with symbol.
         words.insert( words.begin() + i + 1, words[i].substr(1, words[i].size() -1) );
         words[i] = words[i].substr( 0, 1 );
      }
      else {
         // Symbol in middle or end.
         words.insert( words.begin() + i + 1, string{c} );
         string after = words[i].substr( pos + 1, words[i].size() - pos - 1 );
         if( !after.empty() ){
            words.insert( words.begin() + i + 2, after );
         }
         words[i] = words[i].substr( 0, pos );
      }
      return true;
   }
   else {
      return false;
   }
}

////////////////////////////////////////////////////////////////////////
// You do not need to modify tokenize().  

vector<string> tokenize( const string& s )
{
   vector<string> ret;
   int pos = 0;
   int space;

   // Split on spaces:

   while( (space = s.find(' ', pos) ) != string::npos ){
      string word = s.substr( pos, space - pos );
      if( !word.empty() ){
         ret.push_back( word );
      }
      pos = space + 1;
   }

   string lastWord = s.substr( pos, s.size() - pos );

   if( !lastWord.empty() ){
      ret.push_back( lastWord );
   }

   for( int i = 0; i < ret.size(); ++i ) {
      for( char c : {'&', '<', '>', '|'} ) {
         if( splitOnSymbol( ret, i, c ) ){
            --i;
            break;
         }
      }
   }
   return ret;
}

////////////////////////////////////////////////////////////////////////

ostream& operator<<( ostream& outs, const Command& c )
{
   outs << c.execName << " [argv: ";
   for( const auto & arg : c.argv ){
      if( arg ) {
         outs << arg << ' ';
      }
      else {
         outs << "NULL ";
      }
   }
   outs << "] -- FD, in: " << c.inputFd << ", out: " << c.outputFd << " "
        << (c.background ? "(background)" : "(foreground)");
   return outs;
}

////////////////////////////////////////////////////////////////////////
//
// getCommands()
//
// Parses a vector of command line tokens and places them into (as appropriate)
// separate Command structures.
//
// Returns an empty vector if the command line (tokens) is invalid.
//
// You'll need to fill in a few gaps in this function and add appropriate error handling
// at the end.  Note, most of the gaps contain "assert( false )".
//

vector<Command> getCommands( const vector<string> & tokens )
{
   vector<Command> commands( count( tokens.begin(), tokens.end(), "|") + 1 ); // 1 + num |'s commands

   int first = 0;
   int last = find( tokens.begin(), tokens.end(), "|" ) - tokens.begin();

   bool error = false;

   for( int cmdNumber = 0; cmdNumber < commands.size(); ++cmdNumber ){
      const string & token = tokens[ first ];

      if( token == "&" || token == "<" || token == ">" || token == "|" ) {
         error = true;
         break;
      }

      Command & command = commands[ cmdNumber ]; // Get reference to current Command struct.
      command.execName = token;

      // Must _copy_ the token's string (otherwise, if token goes out of scope (anywhere)
      // this pointer would become bad...) Note, this fixes a security hole in this code
      // that had been here for quite a while.

      command.argv.push_back( strdup( token.c_str() ) ); // argv0 == program name

      command.inputFd  = STDIN_FILENO;
      command.outputFd = STDOUT_FILENO;

      command.background = false;
	
      for( int j = first + 1; j < last; ++j ) {

         if( tokens[j] == ">" || tokens[j] == "<" ) {
            // Handle I/O redirection tokens
            //
            // Note, that only the FIRST command can take input redirection
            // (all others get input from a pipe)
            // Only the LAST command can have output redirection!
         
            //assert( false );

            //I/O redirection is just ONE command, detect the symbol, get the file, then done.
            if (tokens[j] == "<" && cmdNumber == 0) {
               int fd = open(tokens[j + 1].c_str(), O_RDONLY);
               command.inputFd = fd;
               break;
            } else if (tokens[j] == ">" && cmdNumber == commands.size() - 1) {
               int fd = open(tokens[j + 1].c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
               command.outputFd = fd;
               break; 
            } else {
               error = true;
            }


         }
         else if( tokens[j] == "&" ){
            command.background = true;
            //assert(false);
         }
         else {
            // Otherwise this is a normal command line argument! Add to argv.
            command.argv.push_back( tokens[j].c_str() );
         }
      }

      if( !error ) {

         if( cmdNumber > 0 ){
            // There are multiple commands.  Open a pipe and
            // connect the ends to the fd's for the commands!

            //assert( false );
            int fds[2];
            int rc = pipe(fds); 
            if (rc < 0) {
               error = true;
            }
            int writefd = fds[1];
            int readfd = fds[0];

            command.inputFd = readfd;
            commands[cmdNumber - 1].outputFd = writefd;
         }

         // Exec wants argv to have a nullptr at the end!
         command.argv.push_back( nullptr );

         // Find the next pipe character
         first = last + 1;

         if( first < tokens.size() ){
            last = find( tokens.begin() + first, tokens.end(), "|" ) - tokens.begin();
         }
      } // end if !error
   } // end for( cmdNumber = 0 to commands.size )

   if( error ){

      // Close any file descriptors you opened in this function and return the appropriate data!

      // Note, an error can happen while parsing any command. However, the "commands" vector is
      // pre-populated with a set of "empty" commands and filled in as we go.  Because
      // of this, a "command" name can be blank (the default for a command struct that has not
      // yet been filled in).  (Note, it has not been filled in yet because the processing
      // has not gotten to it when the error (in a previous command) occurred.
      for (Command com : commands) {
        if (com.inputFd != STDIN_FILENO && com.inputFd != STDERR_FILENO) {
            close(com.inputFd);
        }
        if (com.outputFd != STDOUT_FILENO && com.outputFd != STDERR_FILENO) {
            close(com.outputFd);
        }
      }
      std::cerr << "Error occurred while processing commands. File descriptors closed." << std::endl;
      //assert(false);
   }

   return commands;

} // end getCommands()


void change_directory(const std::string& path) {
    const char* dir;
    if (path.empty() || path == "~") {
        dir = getenv("HOME");
        if (dir == nullptr) {
            std::cerr << "Error: HOME environment variable not set." << std::endl;
            return;
        }
    } else {
        dir = path.c_str();
    }

    if (chdir(dir) != 0) {
        perror("cd failed");
    } else    {
      cout << "Changed directory to: " << dir << endl;
    }
}

void handle_sigchld(int sig) {
    pid_t zombiePid;
    // non-block waiting
    while ((zombiePid = waitpid(-1, nullptr, WNOHANG)) > 0) {
        string message = "\n" + to_string(zombiePid) + " is done!\nmyshell> ";
        write(STDOUT_FILENO, message.c_str(), message.size());
    }
}

int main() {
   signal(SIGCHLD, handle_sigchld);

   string input;
   // print the prompt
   cout << "myshell> ";
   while(getline(std::cin, input)) {//ctrl+d will jump out of the loop
        

      if (input == "exit") {
         cout << "Exiting...\n";
         break;
      }

        
      vector<string> inputTokens = tokenize(input);   
      std::vector<Command> commands = getCommands(inputTokens);
      if (commands.empty()) {
         continue;
      }
      
      if (commands[0].execName == "cd") {
         if (commands[0].argv.size() > 2) {//argv always has a null at the end
            change_directory(commands[0].argv[1]);
         } else {
            change_directory("");
         }
         cout << "myshell> ";
         continue;
         //every process has its own work dir
         //just let the parent process exec cd.
      }

      for (int i = 0; i < commands.size(); i++) {
         pid_t pid = fork();

         //!!! Important: Must close the outputFds(writeFd of pipe) in parent
         //!!! Otherwise the pipe will not completely closed
         //!!! command like "cat somefile.txt | tail", the tail's readFd will block
         //!!! beacause it couldn't read the EOF.
         //!!! "cat somefile.txt | head" works fine even though writeFd of pipe in parent not completely closed
         //!!! because head do Not need to read the EOF
         if (pid > 0) {
            if (i < commands.size() - 1 && commands[i].outputFd != STDOUT_FILENO) {
               close(commands[i].outputFd);
            }
         }

         if (pid == 0) {
                // children ：redirection before exec
            if (commands[i].inputFd != STDIN_FILENO) {
               dup2(commands[i].inputFd, STDIN_FILENO);
               close(commands[i].inputFd);
            }
            if (commands[i].outputFd != STDOUT_FILENO) {
               dup2(commands[i].outputFd, STDOUT_FILENO);
               close(commands[i].outputFd);
            }
            execvp(commands[i].execName.c_str(), const_cast<char* const*>(commands[i].argv.data()));
            // if execvp fail
            std::cerr << "Error: Failed to execute command(child process)\n";
            exit(1);
         } else {
            if (!commands[i].background) {
               // block SIGCHLD signal
               //we don't need to catch the front child process signal
               sigset_t set;
               sigemptyset(&set);
               sigaddset(&set, SIGCHLD);
               sigprocmask(SIG_BLOCK, &set, nullptr);

               waitpid(pid, nullptr, 0);
               // recove SIGCHLD sigal
               sigprocmask(SIG_UNBLOCK, &set, nullptr);
            }
         }

         // also could close parent's writeFd here
         //if (i < commands.size() - 1 && commands[i].outputFd != STDOUT_FILENO) {
           // close(commands[i].outputFd);
         //}
      }
         

      std::cout << "myshell> ";
   }

   return 0;
}