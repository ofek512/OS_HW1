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
#include <string>
#include <unistd.h>
#include <cstring>

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

char** init_args(){
    char** args = (char**) malloc(COMMAND_MAX_ARGS * sizeof(char**));
    if(!args){
        return nullptr;
    }
    for(int i = 0; i < COMMAND_MAX_ARGS; i++){
        args[i] = nullptr;
    }
    return args;
}

bool is_legit_num(const string& s){
    auto it = s.begin();
    for(; it != s.end() && std::isdigit(*it); it++){}
    return it == s.end();
} // Should I add more checks like 123 - 321, --321, empty string?

// TODO: Add your implementation for classes in Commands.h

string Command::getCommandS() {
    return cmd_line;
}


string SmallShell::getPrompt() const {
    return prompt;
}

//JobsList SmallShell::jobList;

/*SmallShell::SmallShell(): prompt("smash"), pid(getpid()),
                          current_process(-1),
                          prevWorkingDir(nullptr){}*/
SmallShell::SmallShell() : aliasMap(), sortedAlias(), prompt("smash"),current_process(-1), prevWorkingDir(nullptr),
                           jobList(new JobsList()), commands(), pid(getpid()) {
    createCommandVector();
/*    map<string,string> aliasMap;
    vector<string> sortedAlias;
    string prompt;
    pid_t current_process;
    //string currWorkingDir;
    char* prevWorkingDir;
    //static JobsList jobList;
    JobsList* jobList;
    vector<string> commands;*/
}

SmallShell::~SmallShell() {
    if(prevWorkingDir) free(prevWorkingDir);
    //should we do smth else?
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
        return new JobsCommand(cmd_line);
    } else if (firstWord == "showpid") {
        return new ShowPidCommand(cmd_line);
    } else if(firstWord == "cd") {
        return new ChangeDirCommand(cmd_line, &prevWorkingDir);
    } else if (firstWord == "quit") {
        return new QuitCommand(cmd_line,SmallShell::getInstance().getJobs());
    } else if (firstWord == "alias") {
        return new AliasCommand(cmd_line);
    } else if(firstWord == "fg") {
        return new ForegroundCommand(cmd_line, SmallShell::getInstance().getJobs()); // TODO: remove jobs
    } else if (firstWord == "unsetenv") {
        return new UnSetEnvCommand(cmd_line);
    }

    //if nothing else is matched, we treat as external command.
    return new ExternalCommand(cmd_line);
    //meow

    return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
    std::string cmd_s = _trim(std::string(cmd_line));
    std::string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    // Check if the command name is an alias
    if (!aliasMap.empty() && aliasMap.find(firstWord) != aliasMap.end()) {
        // Get the command that the alias maps to
        std::string aliasCommand = aliasMap[firstWord];

        // Get any arguments that followed the alias
        std::string args = "";
        size_t spacePos = cmd_s.find_first_of(" \n");
        if (spacePos != std::string::npos) {
            args = cmd_s.substr(spacePos);
        }

        // Create the new command by substituting the alias
        std::string newCmd = aliasCommand + args;

        // Execute the substituted command
        Command* cmd = CreateCommand(newCmd.c_str());
        if (cmd == nullptr) {
            return;
        }
        cmd->execute();
        delete cmd;
    } else {
        // Not an alias
        Command* cmd = CreateCommand(cmd_line);
        if (cmd == nullptr) {
            return;
        }
        cmd->execute();
        delete cmd;
    }
}



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

JobsList::JobsList(): jobsList(), job_map(), max_id(-1){}

void JobsList::printJobsList() {
    removeFinishedJobs();
    for(JobsList::JobEntry* job : jobsList) {
        cout << "[" <<job->jobId << "] " <<job->command << endl; //TODO initialise job->command to have the name
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

void JobsList::removeJobById(int jobId) {
    if(job_map.find(jobId) == job_map.end()) {
        cerr << "removeJobById error: no job with id = " << jobId << endl;
        return;
    }
    auto job = job_map[jobId];
    // Deletion from map
    job_map.erase(jobId);
    // Deletion from list
    for(auto it = jobsList.begin(); it != jobsList.end(); it++) {
        if ((*it)->jobId == jobId) {
            jobsList.erase(it);
            // Update maxId
            if (max_id == jobId) {
                jobsList.sort();
                max_id = jobsList.empty() ? -1 : jobsList.back()->jobId;
            }
            delete job;
            break;
        }
    }
} // Need to check for correctness

void JobsList::addJob(Command *cmd, pid_t pid, bool isStopped) {
    removeFinishedJobs();
    int it = 0;
    while(job_map[it] != nullptr && it++ != 100){}

    if(it == 100){
        cerr << "addJob error: reached limit of processes" << endl;
        return;
    }
    JobEntry* job_to_insert = new JobEntry(cmd, isStopped, it, pid, cmd->getCommandS());//im not sure how to convert it to string correcrtly
    job_map[it] = job_to_insert;
    jobsList.push_back(job_to_insert);

    if(max_id < it) {
        max_id = it;
    }
} // need to check correctness

bool JobsList::JobEntry::operator<(const JobsList::JobEntry &other) const{
    return this->jobId < other.jobId;
}

JobsList::JobEntry* JobsList::getJobById(int jobId) {
    if(job_map.find(jobId) == job_map.end()){
        cerr << "getJobById error: no job with jobId = " << jobId << endl;
        return nullptr;
    }
    return job_map[jobId];
}

JobsList* SmallShell::getJobs() {
    return jobList;
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


void SmallShell::setAlias(string name, string command) {
    aliasMap[name] = command;
    sortedAlias.push_back(name);
}

/////////////////////////////--------------Built-in commands-------//////////////////////////////

void createSegments(const char* cmd_line, vector<string>& segments)
{
    std::string temp=cmd_line;
    std::string toAdd;
    stringstream stringLine(temp);
    while (getline(stringLine, toAdd, ' ')) {
        if(!toAdd.empty())
        {
            segments.push_back(toAdd);
        }
    }
}

BuiltInCommand::BuiltInCommand(const char *cmd_line): Command(cmd_line){
    createSegments(cmd_line, cmd_segments);
    //TODO: need to add check for whether command is background
}

ChpromptCommand::ChpromptCommand(const char* cmd_line) : BuiltInCommand(cmd_line), newSmashPrompt("smash") {}

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

    // Add this line for debugging
    std::cout << "Prompt changed to: " << newSmashPrompt << std::endl;
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
    char** args = init_args();
    int num_of_args = _parseCommandLine(this->cmd_line, args);
    if(!args){
        // We can don't check due to the assumption that malloc doesn't fail?
    }
    if(num_of_args > 2){
        cout << "smash error: cd: too many arguments" << endl;
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

// Alias command
AliasCommand::AliasCommand(const char *cmd_line): BuiltInCommand(cmd_line) {}

void AliasCommand::execute() {
    // Just 'alias' without arguments - list all aliases
//    int size = cmd_segments.size();
//    std::cerr << "DEBUG: Original cmd_line = '" << cmd_line << "'" << std::endl;
//    printf("%d",size);
    if (cmd_segments.size() == 1) {
        vector<string> aliases;
        SmallShell::getInstance().getAllAlias(aliases);
        for (const string& alias : aliases) {
            cout << alias << std::endl;
        }
        return;
    }

    // DEBUG: Print the original command line
    // std::cerr << "DEBUG: Original cmd_line = '" << cmd_line << "'" << std::endl;

    // Use the original command line directly
    std::string fullCommand = cmd_line;
    if (this->backGround) {
        removeBackgroundSignFromString(fullCommand);
    }

    // Trim the command to ensure no leading/trailing whitespace
    fullCommand = _trim(fullCommand);

    // Keep the ^ anchor but make the pattern more flexible for internal spacing
    static const std::regex aliasPattern("^alias\\s+([a-zA-Z0-9_]+)\\s*=\\s*'([^']*)'$");

    std::smatch matches;
    bool matched = std::regex_search(fullCommand, matches, aliasPattern);

    if (!matched) {
        cout << "smash error: alias: invalid alias format" << std::endl;
        return;
    }

    // Extract alias name and command from regex matches
    std::string aliasName = matches[1];
    std::string aliasCommand = matches[2];

    // Check if alias already exists
    if (SmallShell::getInstance().getAlias(aliasName).compare("") != 0) {
        cout << "smash error: alias: " << aliasName << " already exists or is a reserved command" << std::endl;
        return;
    }

    // Check if alias name is a reserved command
    if (SmallShell::getInstance().validCommand(aliasName)) {
        cout << "smash error: alias: " << aliasName << " already exists or is a reserved command" << std::endl;
        return;
    }

    // Create the alias
    SmallShell::getInstance().setAlias(aliasName, aliasCommand);
}

// Quit command
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

// pwd command
GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line): BuiltInCommand(cmd_line) {}

void GetCurrDirCommand::execute() {
    cout << SmallShell::getInstance().getCurrWorkingDir() << endl;
}

char* SmallShell::getCurrWorkingDir() const {
    char* current_path = getcwd(NULL, 0);
    return current_path;
}

// Jobs command
JobsCommand::JobsCommand(const char* cmd_line): BuiltInCommand(cmd_line){}

// unsetenv command

UnSetEnvCommand::UnSetEnvCommand(const char *cmd_line): BuiltInCommand(cmd_line){}
void UnSetEnvCommand::execute() {
    extern char **environ;  // Access the global environment array directly
    
    if (cmd_segments.size() < 2) {
        cout << "smash error: unsetenv: not enough arguments" << endl;
        return;
    }
    
    for (size_t i = 1; i < cmd_segments.size(); i++) {
        string var_name = cmd_segments[i];
        string var_prefix = var_name + "=";
        bool found = false;
        
        // Find the index of the environment variable
        for (int j = 0; environ[j] != nullptr; j++) {
            // Check if this entry starts with our variable name followed by '='
            if (strncmp(environ[j], var_prefix.c_str(), var_prefix.length()) == 0) {
                found = true;
                
                // Shift all subsequent elements (including NULL) one position back
                for (int k = j; environ[k] != nullptr; k++) {
                    environ[k] = environ[k+1];
                }
                
                break;  // Variable found and removed
            }
        }
        


void JobsCommand::execute() {
    SmallShell::getInstance().getJobs()->printJobsList();
}

// ForeGround command
ForegroundCommand::ForegroundCommand(const char *cmd_line, JobsList *jobs): BuiltInCommand(cmd_line) {}

void ForegroundCommand::execute() {
    char** args = init_args();

    if(!args){
        cerr << "Malloc failed" << endl;
        return;
    }

    SmallShell& smash = SmallShell::getInstance();
    int num_of_args = _parseCommandLine(this->cmd_line, args);
    if(num_of_args > 2){
        cerr << "Smash error: fg: invalid arguments" << endl;
        free_args(args, num_of_args);
        return;
    } else if(num_of_args == 1){
        // Check whether jobList is empty
        if(smash.getJobs()->max_id == -1){
            cerr << "smash error: fg: jobs list is empty" << endl;
            free_args(args, num_of_args);
            return;
        }
        JobsList::JobEntry* max_id_job = smash.getJobs()->job_map[smash.getJobs()->max_id];
        if(max_id_job->isStopped){
            if(kill(max_id_job->pid, SIGCONT) == -1){
                perror("smash error: kill failed"); // Unable to invoke process
                free_args(args, num_of_args);
                return;
            }
        }
        cout << max_id_job->command << " " << max_id_job->pid << endl;
        smash.getJobs()->removeJobById(max_id_job->jobId);
        smash.current_process = max_id_job->pid;

        int status;
        if(waitpid(max_id_job->pid, &status, 0) == -1){
            perror("smash error: waitpid failed");
            free_args(args, num_of_args);
            return;
        }
    } else {
        // Check whether arguments is a number
        if(!is_legit_num(args[1])){
            cerr << "smash error: fg: invalid arguments" << endl;
            free_args(args, num_of_args);
            return;
        }
        int job_id = stoi(args[1]);
        JobsList::JobEntry* job_to_fg = smash.getJobs()->getJobById(job_id);
        // Check whether find succeed
        if(!job_to_fg){
            cerr << "smash error: fg: job-id " << job_id << " does not exist" << endl;
            free_args(args, num_of_args);
            return;
        }
        int job_pid = job_to_fg->pid;
        if(job_to_fg->isStopped) {
            // Trying to invoke process if it stopepd
            if (kill(job_pid, SIGCONT) == -1) {
                perror("smash error: kill failed");
                free_args(args, num_of_args);
                return;
            }
        }
        cout << job_to_fg->command << " " << job_pid << endl;
        smash.current_process = job_pid;
        smash.getJobs()->removeJobById(job_id);
        // Check whether waitpid succeed
        int status;
        if(waitpid(job_pid, &status, 0) == -1){
            perror("smash error: waitpid failed");
            free_args(args, num_of_args);
            return;
        }
    }
    free_args(args, num_of_args);
}
//External commands
ExternalCommand::ExternalCommand(const char *cmd_line) : Command(cmd_line) {
    // Store the original command line
    backGround = _isBackgroundComamnd(cmd_line);

    // Create a clean version of the command for segments
    std::string clean_cmd_line = cmd_line;
    if (backGround) {
        // Create a modifiable copy of the command line
        char* cmd_copy = strdup(cmd_line);
        if (cmd_copy) {
            // Remove the & sign
            _removeBackgroundSign(cmd_copy);
            // Store the clean command segments
            createSegments(cmd_copy, cmd_segments);
            free(cmd_copy);
        }
    } else {
        // If not a background command, just use as is
        createSegments(cmd_line, cmd_segments);
    }
}

void ExternalCommand::execute() {
    if (cmd_segments.empty()) {
        return;  // Nothing to execute
    }

    // Create a clean version of the command line for execution
    char* cmd_copy = strdup(cmd_line);
    if (!cmd_copy) {
        perror("smash error: memory allocation failed");
        return;
    }

    // If it's a background command, remove the & sign
    if (backGround) {
        _removeBackgroundSign(cmd_copy);
    }

    // Parse the cleaned command
    char** args = init_args();
    if (!args) {
        perror("smash error: memory allocation failed");
        free(cmd_copy);
        return;
    }

    int num_args = _parseCommandLine(cmd_copy, args);
    free(cmd_copy);  // Done with the copy

    if (num_args == 0) {
        free_args(args, num_args);
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        // Fork failed
        perror("smash error: fork failed");
        free_args(args, num_args);
        return;
    }

    if (pid == 0) {
        // Child process
        setpgrp(); // Set process group ID to dissociate from shell

        // Execute the command using execvp which searches in PATH
        execvp(args[0], args);

        // If execvp failed, report error and exit
        perror("smash error: execvp failed");
        free_args(args, num_args);
        exit(1);
    } else {
        // Parent process
        SmallShell& smash = SmallShell::getInstance();

        if (!backGround) {
            // Wait for child process to complete if not a background command
            smash.current_process = pid;
            int status;
            if (waitpid(pid, &status, WUNTRACED) == -1) {
                perror("smash error: waitpid failed");
            }
            smash.current_process = -1;
        } else {
            // Add job to jobs list if it's a background command
            smash.getJobs()->addJob(this, pid, false);
        }

        free_args(args, num_args);
    }
}

