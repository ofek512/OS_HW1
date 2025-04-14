#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <algorithm>
#include <regex>

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


SmallShell::SmallShell() : aliasMap(), sortedAlias(), prompt("smash"), pid(getPid()), currWorkingDir(getCurrWorkingDir()), prevWorkingDir(""), jobList(new JobsList()), commands() {
    createCommandVector();
    // map<string,string> aliasMap;
    // vector<string> sortedAlias;
    // string prompt;
    // int pid;
    // string currWorkingDir;
    // string prevWorkingDir;
    // JobsList* jobList;
    // vector<string> commands;
}

SmallShell::~SmallShell() {
// TODO: add your implementation
    delete jobList;
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
        return new GetCurrDirCommand(cmd_line);
    } else if (firstWord == "jobs") {
        return new JobsCommand(cmd_line,SmallShell::getInstance().getJobs());
    } else if (firstWord == "quit") {
        return new QuitCommand(cmd_line,SmallShell::getInstance().getJobs());
    } else if (firstWord == "alias") {
        return new AliasCommand(cmd_line);
    }


    return nullptr;
}

// execute commands //

void GetCurrDirCommand::execute() {
    cout << SmallShell::getInstance().getCurrWorkingDir() << endl;
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

void JobsCommand::execute() {
    jobs->printJobsList();
}

QuitCommand::QuitCommand(const char *cmd_line, JobsList *jobs):BuiltInCommand(cmd_line),jobs(jobs){}

void QuitCommand::execute() {
    //check if there is argument for kill
    if(cmd_segments.size() > 1) {
        string kill = cmd_segments[1];
        removeBackgroundSignFromString(kill);
        if(kill == "kill") {
            jobs -> removeFinishedJobs();
            jobs -> printJobsBeforeQuit();
            jobs -> killAllJobs();
        }
    }
    exit(0);
}

void AliasCommand::execute() {
    // Just 'alias' without arguments - list all aliases AAAAAAAAAAAAAAAAAAAA
    if (cmd_segments.size() == 1) {
        vector<string> aliases;
        SmallShell::getInstance().getAllAlias(aliases);
        for (const string& alias : aliases) {
            cout << alias << std::endl;
        }
        return;
    }

    // Construct the full command string
    std::string fullCommand;
    for (size_t i = 0; i < cmd_segments.size(); i++) {
        fullCommand += cmd_segments[i];
        if (i < cmd_segments.size() - 1) {
            fullCommand += " ";
        }
    }

    if (this->backGround) {
        removeBackgroundSignFromString(fullCommand);
    }

    // Validate format with regex
    static const std::regex aliasPattern("^alias ([a-zA-Z0-9_]+)='([^']*)'$");
    std::smatch matches;

    if (!std::regex_search(fullCommand, matches, aliasPattern)) {
        std::cerr << "smash error: alias: invalid alias format" << std::endl;
        return;
    }

    // Extract alias name and command from regex matches
    std::string aliasName = matches[1];
    std::string aliasCommand = matches[2];

    // Check if alias already exists
    if (SmallShell::getInstance().getAlias(aliasName).compare("") != 0) {
        std::cerr << "smash error: alias: " << aliasName << " already exists or is a reserved command" << std::endl;
        return;
    }

    // Check if alias name is a reserved command, TODO check if correct
    if (SmallShell::getInstance().validCommand(aliasName)) {
        std::cerr << "smash error: alias: " << aliasName << " already exists or is a reserved command" << std::endl;
        return;
    }

    // Create the alias
    SmallShell::getInstance().setAlias(aliasName, aliasCommand);
}


void SmallShell::executeCommand(const char *cmd_line) {
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

// jobs related functions //

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
} //check for correctness

void JobsList::removeFinishedJobs() {
    jobsList.remove_if(isFinished);
}

void JobsList::printJobsBeforeQuit()
{
    //remove finished jobs before printing the jobs.
    removeFinishedJobs();
    std::cout << "smash: sending SIGKILL signal to "<<jobsList.size()<<" jobs:" << std::endl;
    for(auto listIt=jobsList.begin(); listIt!=jobsList.end();++listIt)
    {
        JobsList::JobEntry* job=*listIt;
        std::cout << job->pid <<": " << job->command  << std::endl;
    }
} //check for correctness

void JobsList::killAllJobs()
{
    for(auto listIt=jobsList.begin(); listIt!=jobsList.end();++listIt)
    {
        if (*listIt) {  // Check for null pointers
            kill((*listIt)->pid, SIGKILL);
            delete *listIt;
        }
    }
    jobsList.clear();  // check if we need to do clear
}

void SmallShell::getAllAlias(vector<string> &aliases) {
    for(auto element :  sortedAlias) {
        aliases.push_back(element + "='"+aliasMap[element]+"'");
    }
}

string SmallShell::getAlias(string name) {
    if(aliasMap.find(name) == aliasMap.end()) {
        return "";
    } else {
        return aliasMap[name];
    }
}

bool SmallShell::validCommand(string name) {
    for (string command : commands) {
        if (command == name) {
            return true;
        }
    }
    return false;
}

//todo important to add this to constructor of smallshell
void SmallShell::createCommandVector() {
    commands = {"chprompt", "showpid", "pwd", "cd", "jobs", "fg", "quit",
                     "kill", "unalias", "alias", "unsetenv", "watchproc"};
}

int SmallShell::getPid() {
    return getpid();
}

void SmallShell::setAlias(string name, string command) {
    aliasMap[name] = command;
    sortedAlias.push_back(name);
}
