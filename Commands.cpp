#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void free_args(char** args, int num_of_args){
    for(int i = 0; i < num_of_args || args[i] != nullptr; i++){
        free(args[i]);
    }
    //free(args); //need to think
}

void _removeBackgroundSign(char *cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

// TODO: Add your implementation for classes in Commands.h


JobsList SmallShell::jobList;

SmallShell::SmallShell(): prompt("smash"), pid(getpid()),
                          current_process(-1),
                          prevWorkingDir(nullptr){}

SmallShell::~SmallShell() {
    if(prevWorkingDir) free(prevWorkingDir);
    //should we do smth else?
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {
    // For example:
  /*
  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

  if (firstWord.compare("pwd") == 0) {
    return new GetCurrDirCommand(cmd_line);
  }
  else if (firstWord.compare("showpid") == 0) {
    return new ShowPidCommand(cmd_line);
  }
  else if ...
  .....
  else {
    return new ExternalCommand(cmd_line);
  }
  */
    std::string cmd_s = _trim(std::string(cmd_line));
    std::string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
    // TODO: change to factory
    if (firstWord == "chprompt") {
        return new ChpromptCommand(cmd_line);
    } else if (firstWord =="pwd") {
        //return new GetCurrDirCommand(cmd_line);
    } else if (firstWord == "jobs") {
        //return new JobsCommand(cmd_line,SmallShell::getInstance().getJobs());
    } else if (firstWord == "showpid") {
        return new ShowPidCommand(cmd_line);
    } else if(firstWord == "cd") {
        return new ChangeDirCommand(cmd_line, &prevWorkingDir);
    }


    return nullptr;
}

// execute commands //

/*void GetCurrDirCommand::execute() {
    cout << SmallShell::getInstance().getCurrWorkingDir() << endl;
}*/



void JobsCommand::execute() {
    jobs->printJobsList();
}


void SmallShell::executeCommand(const char *cmd_line) {
    // TODO: Add your implementation here
    Command* cmd = CreateCommand(cmd_line);
    //if cmd is nullptr, it means that the command is not recognized
    if(cmd == nullptr) {
        return;
    }
    cmd->execute();
    //delete cmd for memory leak prevention
    delete cmd;
    // Please note that you must fork smash process for some commands (e.g., external commands....)
}


// TODO: small shell implementation of functions and etc

void SmallShell::setPrompt(string newPrompt)
{
    prompt=newPrompt;
}

//helper function to remove background sign from string and not char*
void removeBackgroundSignFromString(std::string& cmd_line) {
    char* char_cmd = new char[cmd_line.length() + 1];
    strcpy(char_cmd, cmd_line.c_str());

    _removeBackgroundSign(char_cmd);

    cmd_line = std::string(char_cmd);
    delete[] char_cmd;
}

/////////////////////////////--------------Job List implementation-------//////////////////////////////

JobsList::JobsList(): jobsList(), job_map(), max_id(1){}

void JobsList::printJobsList() {
    removeFinishedJobs();
    for(JobsList::JobEntry* job : jobsList) {
        cout << "[" <<job->jobId << "] " <<job->command << " : " <<job->pid << endl; //print jobs, need to check if theyre sorted
    }
}

bool isFinished(JobsList::JobEntry* job)
{
    if(job == nullptr)
    {
        return true;
    }

    int status;
    pid_t result = waitpid(job->pid, &status, WNOHANG);

    if (result > 0) {
        // Process finished, interpret exit status if needed
        return true;
    }
    else if (result == 0) {
        // Process is still running
        return false;
    }
    else {
        // Error occurred (result is -1)
        if (errno == ECHILD) {
            // No child process exists - consider it finished
            return true;
        } else {
            // Other error
            perror("waitpid");
            return false; // Safer to assume it's still running TODO check
        }
    }
}

void JobsList::removeFinishedJobs() {
    jobsList.remove_if(isFinished);
}


/////////////////////////////--------------Built-in commands-------//////////////////////////////

BuiltInCommand::BuiltInCommand(const char *cmd_line): Command(cmd_line){
    //TODO: need to add check for whether command is background
}

void ChpromptCommand::execute()
{
    if(cmd_segments.size()>1)//there was a new prompt
    {
        string prompt=cmd_segments[1];
        removeBackgroundSignFromString(prompt);
        //if prompt empty, set default prompt
        if(prompt.empty()) {
            newSmashPrompt="smash";
        }
            //if prompt is not empty, set new prompt
        else {
            newSmashPrompt=prompt;
        }
    }
    else
    {
        newSmashPrompt="smash";
    }
    SmallShell::getInstance().setPrompt(newSmashPrompt);
}

/* ShowPid command */
ShowPidCommand::ShowPidCommand(const char *cmd_line): BuiltInCommand(cmd_line){}

void ShowPidCommand::execute() {
    SmallShell &shell = SmallShell::getInstance();
    cout << "smash pid is " << shell.pid << endl;
    return;
}

/* cd command */
ChangeDirCommand::ChangeDirCommand(const char *cmd_line, char **plastPwd): BuiltInCommand(cmd_line),
                                                                           plastPwd(plastPwd){}
void ChangeDirCommand::execute() {
    char* args_arr[COMMAND_MAX_ARGS]; // I'm not sure
    char** args = args_arr;
    int num_of_args = _parseCommandLine(this->cmd_line, args);
    if(!args){
        // We can don't check due to the assumption that malloc doesn't fail?
    }
    if(num_of_args > 2){
        cerr << "smash error: cd: too many arguments" << endl;
    } else if(num_of_args == 1){
    } else {
        string path = args[1];
        char* current_path = getcwd(NULL, 0); //check whether it's working

        if(path == "-"){
            if(!(*plastPwd)){
                cout << "smash error: cd: OLDPWD not set" << endl;
                free(current_path);
            } else {
                if(chdir(*plastPwd) == -1){
                    cout << "smash error: cd: chdir failure" << endl;
                    free(current_path);
                    return;
                } else {
                    *plastPwd = current_path; // Should free previous path?
                }
            }
        } else {
            if(chdir(args[1]) == -1){
                cout << "smash error: cd: chdir failure" << endl;
                free(current_path);
            } else {
                *plastPwd = current_path; //should free previous path?
            }
        }
    }
    free_args(args, num_of_args);
}
