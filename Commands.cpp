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
#include <fstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>

#include <dirent.h>
#include <pwd.h>

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
}

bool extract_signal_number(char* input, int& signum){
    if(!input || strlen(input) < 2 || input[0] != '-'){
        return false;
    }
    string temp = (string(input)).erase(0, 1);

    if(!is_legit_num(temp)){
        return false;
    }
    signum = stoi(temp);

    if(signum > 31) return false;

    return true;
}

string Command::getCommandS() {
    return cmd_line;
}


string SmallShell::getPrompt() const {
    return prompt;
}

//JobsList SmallShell::jobList;

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

    std::string cmd_s = _trim(std::string(cmd_line));
    std::string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    // Redirection command
    if(strstr(cmd_line, ">>")) {
        return new RedirectionCommand(cmd_line,RedirectionCommand::CONCAT);
    } else if(strstr(cmd_line, ">")) {
        return new RedirectionCommand(cmd_line, RedirectionCommand::TRUNCATE);
    }

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
        return new ForegroundCommand(cmd_line); // TODO: remove jobs
    } else if (firstWord == "unsetenv") {
        return new UnSetEnvCommand(cmd_line);
    } else if (firstWord == "watchproc") {
        return new WatchProcCommand(cmd_line);
    } else if (firstWord == "kill") {
        return new KillCommand(cmd_line);
    } else if(firstWord == "unalias") {
        return new UnAliasCommand(cmd_line);
    } else if (firstWord =="du") {
        return new DiskUsageCommand(cmd_line);
    } else if (firstWord == "whoami") {
        return new WhoAmICommand(cmd_line);
    }
    //if nothing else is matched, we treat as external command.
    return new ExternalCommand(cmd_line);
    //meow

    return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
//remove finished jobs
    jobList->removeFinishedJobs();


    string cmd_s = _trim(std::string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    // Check if the command name is an alias
    if (!aliasMap.empty() && aliasMap.find(firstWord) != aliasMap.end()) {
        // Get the command that the alias maps to
        std::string aliasCommand = aliasMap[firstWord];

        // Get any arguments that followed the alias
        string args = "";
        size_t spacePos = cmd_s.find_first_of(" \n");
        if (spacePos != string::npos) {
            args = cmd_s.substr(spacePos);
        }

        // Create the new command by substituting the alias
        string newCmd = aliasCommand + args;

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
}

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
}

void JobsList::addJob(Command *cmd, pid_t pid, bool isStopped) {
    removeFinishedJobs();
    int it = 1;
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

bool SmallShell::removeAlias(string name){

    // Check whether alias exist
    if(SmallShell::getInstance().getAlias(name) == ""){
        return false;
    }

    // Erase from map and vector
    aliasMap.erase(name);
    sortedAlias.erase(find(sortedAlias.begin(), sortedAlias.end(), name));

    return true;
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
        char* current_path = getcwd(NULL, 0);

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

// Unalias command
UnAliasCommand::UnAliasCommand(const char *cmd_line): BuiltInCommand(cmd_line) {}

void UnAliasCommand::execute() {
    char** args = init_args();
    int num_of_args = _parseCommandLine(this->cmd_line, args);

    // Check whether malloc succeed
    if(!args){
        cout << "smash error: malloc failed" << endl;
        return;
    }

    // Check validity of arguments
    if(num_of_args == 1) {
        cout << "smash error: unalias: not enough arguments" << endl;
        free_args(args, num_of_args);
        return;
    }

    for(int i = 1; i < num_of_args; i++){
        string current_alias(args[i]);

        // Check whether alias exist
        if(!SmallShell::getInstance().removeAlias(current_alias)){
            cout << "smash error: unalias: " << current_alias << " alias does not exist" << endl;
            free_args(args, num_of_args);
            return;
        }
    }
    free_args(args, num_of_args);
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
    if (cmd_segments.size() < 2) {
        cout << "smash error: unsetenv: not enough arguments" << endl;
        return;
    }

    for (size_t i = 1; i < cmd_segments.size();  i++) {
        // Get the variable name
        string var_name = cmd_segments[i];

        // Check if the variable exists
        if (getenv(var_name.c_str()) == nullptr) {
            cout << "smash error: unsetenv: " << var_name << " does not exist" << std::endl;
            return;
        }

        // Remove the environment variable
        if (unsetenv(var_name.c_str()) != 0) {
            perror("smash error: unsetenv failed"); //maybe we need to print
            return;
        }
    }
}

// Jobs command
void JobsCommand::execute() {
    SmallShell::getInstance().getJobs()->printJobsList();
}

// ForeGround command
ForegroundCommand::ForegroundCommand(const char *cmd_line): BuiltInCommand(cmd_line) {}

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

// Watchproc command
WatchProcCommand::WatchProcCommand(const char *cmd_line): BuiltInCommand(cmd_line) {}

void WatchProcCommand::execute() {
    char** args = init_args();
    int num_of_args = _parseCommandLine(this->cmd_line, args);
    // Check whether malloc succeed
    if(!args){
        cerr << "smash failed: memory allocation error" << endl;
        return;
    }

    // Check correctness of input
    if(num_of_args > 2 || num_of_args == 1 || !is_legit_num(args[1])){
        perror("smash error: watchproc: invalid arguments");
        free_args(args, num_of_args);
        return;
    }

    int pid = stoi(args[1]);
    //ifstream memory_file("/proc/" + to_string(pid) + "/status");

    string memory_path = ("/proc/" + to_string(pid) + "/status");
    int memory_fd = open(memory_path.c_str(), O_RDONLY);

    // Check whether pid exists
    if(memory_fd == -1){
        cout << "smash error: watchproc: pid " << pid << " does not exist" << endl;
        free_args(args, num_of_args);
        return;
    }

    char buffer[4096] = {0};
    ssize_t bytes_read = read(memory_fd, buffer, sizeof(buffer) - 1);
    close(memory_fd);

    // Check whether read succeed
    if(bytes_read <= 0) {
        perror("smash error: read failed");
        free_args(args, num_of_args);
        return;
    }

    buffer[bytes_read] = '\0';

    int memory_usage = 0;
    char* line = strtok(buffer, "\n");
    while(line){
        if(strncmp(line, "VmRSS:", 6) == 0) {
            char* value_start = line + 6;
            // Skip whitespaces
            while(*value_start == ' ' || *value_start == '\t') {
                value_start++;
            }
            memory_usage = atoi(value_start);
            break;
        }
        line = strtok(nullptr, "\n");
    }

    // CPU file
    string cpu_path = "/proc/" + to_string(pid) + "/stat";
    int cpu_fd = open(cpu_path.c_str(), O_RDONLY);

    if(cpu_fd == -1){
        perror("smash error: open failed");
        free_args(args, num_of_args);
        return;
    }

    memset(buffer, 0, sizeof(buffer));
    bytes_read = read(cpu_fd, buffer, sizeof(buffer) - 1);
    close(cpu_fd);

    // Check whether read succeed
    if(bytes_read <= -1) {
        perror("smash error: read failed");
        free_args(args, num_of_args);
        return;
    }

    buffer[bytes_read] = '\0';

    long utime = 0, stime = 0;
    int field = 0;

    // Retrieve CPU stats
    char* cpu_line = strtok(buffer, " ");
    while(cpu_line){
        field++;
        if(field == 14) {
            utime = atol(cpu_line);
        } else if (field == 15){
            stime = atol(cpu_line);
            break;
        }
    }

    long ticks_per_sec = sysconf(_SC_CLK_TCK);
    cout << "PID: " << pid << " | CPU Usage: "<< std::setprecision(2) << (utime + stime) / (double)ticks_per_sec
         << "% | Memory Usage: " << std::setprecision(2) << (double)memory_usage / 1024 << " MB" << endl;
    free_args(args, num_of_args);
} // Need to check calculation of CPU time

// Kill command
KillCommand::KillCommand(const char *cmd_line): BuiltInCommand(cmd_line) {}

void KillCommand::execute() {
    char** args = init_args();
    int num_of_args = _parseCommandLine(this->cmd_line, args);
    SmallShell& smash = SmallShell::getInstance();

    // Check whether malloc succeed
    if(!args){
        cerr << "smash error: malloc failed" << endl;
        return;
    }

    // Check validity of arguments
    if(num_of_args != 3){
        cerr << "smash error: kill: invalid arguments" << endl;
        free_args(args, num_of_args);
        return;
    }

    int signum = 0, jobid = 0;

    // Check validity of jobid
    if(!is_legit_num(args[2])) {
        cout << "smash error: kill: invalid arguments" << endl;
        free_args(args, num_of_args);
        return;
    }

    int job_id = stoi(args[2]);
    JobsList::JobEntry* job_to_kill = smash.getJobs()->getJobById(job_id);

    // Check whether job exists
    if(!job_to_kill) {
        cout << "smash error: kill: job-id " << job_id << " does not exist" << endl;
        free_args(args, num_of_args);
        return;
    }

    // Check whether signum is valid
    if(!extract_signal_number(args[1], signum)){
        cout << "smash error: kill: invalid arguments" << endl;
        free_args(args, num_of_args);
        return;
    }

    // Check whether kill succed
    if(kill(job_to_kill->pid, signum) == -1) {
        perror("smash error: kill failed");
        free_args(args, num_of_args);
        return;
    }

    cout << "signal number " << signum << " was sent to pid " << job_to_kill->pid << endl;

    if(signum == SIGTSTP) {
        job_to_kill->isStopped = true;
    } else if(signum == SIGCONT) {
        job_to_kill->isStopped = false; // Maybe need to make some signal handler?
    }

    free_args(args, num_of_args);
}


/////////////////////////////--------------External commands-------//////////////////////////////

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
            if (waitpid(pid, &status, WUNTRACED) == -1)  {
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

/////////////////////////////--------------Special commands-------//////////////////////////////

// Redirection command
RedirectionCommand::RedirectionCommand(const char *cmd_line, command_type type):
                                       Command(cmd_line), type(type) {
    string cmd;
    string file;
    size_t index = 0;
    string input(cmd_line);
    if(type == CONCAT){
        index = input.find(">>");
    } else {
        index = input.find('>');
    }

    // Parse command
    cmd = _trim(input.substr(0, index));
    command = (char*) malloc(sizeof(char) * (cmd.length() + 1));

    // Check whether malloc succeed
    if(!command){
        perror("smash error: malloc failed");
        throw bad_alloc();
    }

    strcpy(command, cmd.c_str());
    if(_isBackgroundComamnd(command)) {
        _removeBackgroundSign(command);
    }

    // Parse path
    file = _trim(input.substr(index + type));
    file_name = (char*) malloc(sizeof(char) * (file.length() + 1));

    // Check whether malloc succeed
    if(!file_name){
        free(command);
        perror("smash error: malloc failed");
        throw bad_alloc();
    }

    strcpy(file_name, file.c_str());

}

RedirectionCommand::~RedirectionCommand() {
    free(command);
    free(file_name);
}

void RedirectionCommand::execute() {

    int fd = 0;
    stdout_copy = dup(1);

    // Trying to open the file
    if(type == CONCAT) {
        fd = open(file_name, O_WRONLY | O_CREAT | O_APPEND, 0644);
    } else {
        fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    }

    // Check whether open succeed
    if(fd < 0) {
        perror("smash error: open failed");
        close(stdout_copy);
        return;
    }

    // Check whether redirection succeed
    if (dup2(fd, 1) == -1) {
        perror("smash error: dup2 failed");
        close(fd);
        close(stdout_copy);
        return;
    }

    // Command execution
    SmallShell::getInstance().executeCommand(command);

    // Restore redirection to stdout
    dup2(stdout_copy, 1);
    close(fd);
    close(stdout_copy);
}

DiskUsageCommand::DiskUsageCommand(const char *cmd_line) : Command(cmd_line) {
    createSegments(cmd_line, cmd_segments);
}

long calculate_dir_size(const char* path) {
    DIR* dir;
    struct dirent* entry;
    struct stat file_stat;
    long size = 0;

    // Open directory
    if (!(dir = opendir(path))) {
        return -1;  // Error opening directory
    }

    // Read directory contents
    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".." directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Create full path for the entry
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        // Get file/directory stats
        if (stat(full_path, &file_stat) == 0) {
            // Add file size
            size += file_stat.st_size;

            // If entry is a directory, recursively calculate its size
            if (S_ISDIR(file_stat.st_mode)) {
                long dir_size = calculate_dir_size(full_path);
                if (dir_size >= 0) {
                    size += dir_size;
                }
            }
        }
    }

    closedir(dir);
    return size;
}

void DiskUsageCommand::execute() {
    // Check number of arguments
    if (cmd_segments.size() > 2) {
        std::cout << "smash error: du: too many arguments" << std::endl;
        return;
    }

    // Determine directory path
    const char* dir_path;
    if (cmd_segments.size() == 1) {
        // No path specified, use current directory
        dir_path = ".";
    } else {
        dir_path = cmd_segments[1].c_str();
    }

    // Check if directory exists
    DIR* dir = opendir(dir_path);
    if (dir == NULL) {
        std::cout << "smash error: du: directory " << dir_path << " does not exist" << std::endl;
        return;
    }
    closedir(dir);

    // Calculate total size
    long size_in_bytes = calculate_dir_size(dir_path);
    if (size_in_bytes < 0) {
        std::cout << "smash error: du: failed to calculate disk usage" << std::endl;
        return;
    }

    // Convert to KB with proper rounding
    // This ensures that files less than 1KB still contribute to the total
    long kb_size = (size_in_bytes + 512) / 1024; // Round up if more than half KB

    std::cout << "Total disk usage: " << kb_size << " KB" << std::endl;
}

WhoAmICommand::WhoAmICommand(const char *cmd_line) : Command(cmd_line) {
    // No special initialization needed since arguments will be ignored
}

void WhoAmICommand::execute() {
    // Get user ID of the current process
    uid_t uid = getuid();

    // Get user information from user ID
    struct passwd *pw = getpwuid(uid);

    if (pw == nullptr) {
        // Error retrieving user information
        std::cerr << "smash error: whoami: failed to get user information" << std::endl;
        return;
    }

    // Output username and home directory
    std::cout << pw->pw_name << " " << pw->pw_dir << std::endl;
}
