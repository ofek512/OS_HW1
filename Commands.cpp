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
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

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

int get_ip_and_subnet(const char* interface, string to_fill[]);

int get_default_gateway(string to_fill[]);

int get_DNS_servers(vector<string>& dns_servers);

string Command::getCommandS() {
    return cmd_line;
}

string ExternalCommand::getCommandS() {
    return full_cmd;
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
Command *SmallShell::CreateCommand(char* cmd_line) {

    std::string cmd_s = _trim(std::string(cmd_line));
    std::string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    // Pipe command
    if (strstr(cmd_line, "|")) {
        return new PipeCommand(cmd_line, PipeCommand::STDOUT);
    } else if (strstr(cmd_line, "|&")) {
        return new PipeCommand(cmd_line, PipeCommand::STDERR);
    }

    // Complex external command
    if(strstr(cmd_line, "*") || strstr(cmd_line, "?")) {
        return new ComplexExternalCommand(cmd_line);
    }
    //check if the command first word ends with &
    if(firstWord.back() == '&') {
        firstWord.pop_back();
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
    } else if (firstWord =="netinfo") {
        return new NetInfo(cmd_line);
    }

    // Redirection command
    if(strstr(cmd_line, ">>")) {
        return new RedirectionCommand(cmd_line,RedirectionCommand::CONCAT);
    } else if(strstr(cmd_line, ">")) {
        return new RedirectionCommand(cmd_line, RedirectionCommand::TRUNCATE);
    }

    //if nothing else is matched, we treat as external command.
    return new ExternalCommand(cmd_line);
    //meow

    return nullptr;
}

void SmallShell::executeCommand(char *cmd_line) {
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
        Command* cmd = CreateCommand(const_cast<char *>(newCmd.c_str()));
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
    if(job == nullptr || job->cmd == NULL)
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
    for (auto it = jobsList.begin(); it != jobsList.end(); ) {
        if (isFinished(*it) && (*it)->cmd != NULL) {
            job_map.erase((*it)->jobId);
            auto temp_it = it;
            it = jobsList.erase(it);  // erase returns the next valid iterator
            if (max_id == (*temp_it)->jobId) {
                jobsList.sort();
                max_id = jobsList.empty() ? -1 : jobsList.back()->jobId;
            }
        } else {
            ++it;
        }
    }
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
    removeFinishedJobs();
    if(job_map.find(jobId) == job_map.end()){
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
                "kill", "unalias", "alias", "unsetenv", "watchproc", "du", "netinfo"};
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

void createSegments( char* cmd_line, vector<string>& segments)
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

BuiltInCommand::BuiltInCommand( char *cmd_line): Command(cmd_line){
    // Ignore background command
    if(_isBackgroundComamnd(cmd_line)) {
        _removeBackgroundSign(cmd_line);
    }
    createSegments(cmd_line, cmd_segments);
    //TODO: need to add check for whether command is background
}

ChpromptCommand::ChpromptCommand( char* cmd_line) : BuiltInCommand(cmd_line), newSmashPrompt("smash") {}

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
ShowPidCommand::ShowPidCommand( char *cmd_line): BuiltInCommand(cmd_line){}

void ShowPidCommand::execute() {
    SmallShell &shell = SmallShell::getInstance();
    cout << "smash pid is " << shell.pid << endl;
    return;
}

/* cd command */
ChangeDirCommand::ChangeDirCommand( char *cmd_line, char **plastPwd): BuiltInCommand(cmd_line),
                                                                           plastPwd(plastPwd){}
void ChangeDirCommand::execute() {
    char** args = init_args();
    int num_of_args = _parseCommandLine(this->cmd_line, args);
    if(!args){
        // We can don't check due to the assumption that malloc doesn't fail?
    }
    if(num_of_args > 2){
        cerr << "smash error: cd: too many arguments" << endl;
    } else if(num_of_args == 1){
    } else {
        string path = args[1];
        char* current_path = getcwd(NULL, 0);

        if(path == "-"){
            if(!(*plastPwd)){
                cerr << "smash error: cd: OLDPWD not set" << endl;
                free(current_path);
            } else {
                if(chdir(*plastPwd) == -1){
                    perror("smash error: cd: chdir failure");
                    free(current_path);
                    return;
                } else {
                    *plastPwd = current_path; // Should free previous path?
                }
            }
        } else {
            if(chdir(args[1]) == -1){
                perror("smash error: cd: chdir failure");
                free(current_path);
            } else {
                *plastPwd = current_path; //should free previous path?
            }
        }
    }
    free_args(args, num_of_args);
}

// Alias command
AliasCommand::AliasCommand( char *cmd_line): BuiltInCommand(cmd_line) {}

void AliasCommand::execute() {

    if (cmd_segments.size() == 1) {
        vector<string> aliases;
        SmallShell::getInstance().getAllAlias(aliases);
        for (const string& alias : aliases) {
            cout << alias << std::endl;
        }
        return;
    }



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
        cerr << "smash error: alias: invalid alias format" << std::endl;
        return;
    }

    // Extract alias name and command from regex matches
    string aliasName = matches[1];
    string aliasCommand = matches[2];

    // Check if alias already exists
    if (SmallShell::getInstance().getAlias(aliasName).compare("") != 0) {
        cerr << "smash error: alias: " << aliasName << " already exists or is a reserved command" << std::endl;
        return;
    }

    // Check if alias name is a reserved command
    if (SmallShell::getInstance().validCommand(aliasName)) {
        cerr << "smash error: alias: " << aliasName << " already exists or is a reserved command" << std::endl;
        return;
    }

    // Create the alias
    SmallShell::getInstance().setAlias(aliasName, aliasCommand);
}

// Unalias command
UnAliasCommand::UnAliasCommand( char *cmd_line): BuiltInCommand(cmd_line) {}

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
        cerr << "smash error: unalias: not enough arguments" << endl;
        free_args(args, num_of_args);
        return;
    }

    for(int i = 1; i < num_of_args; i++){
        string current_alias(args[i]);

        // Check whether alias exist
        if(!SmallShell::getInstance().removeAlias(current_alias)){
            cerr << "smash error: unalias: " << current_alias << " alias does not exist" << endl;
            free_args(args, num_of_args);
            return;
        }
    }
    free_args(args, num_of_args);
}

// Quit command
QuitCommand::QuitCommand( char *cmd_line, JobsList *jobs):BuiltInCommand(cmd_line),jobs(jobs){}

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
GetCurrDirCommand::GetCurrDirCommand( char *cmd_line): BuiltInCommand(cmd_line) {}

void GetCurrDirCommand::execute() {
    cout << SmallShell::getInstance().getCurrWorkingDir() << endl;
}

char* SmallShell::getCurrWorkingDir()  const{
    char* current_path = getcwd(NULL, 0);
    return current_path;
}

// Jobs command
JobsCommand::JobsCommand( char* cmd_line): BuiltInCommand(cmd_line){}

// Unsetenv command
UnSetEnvCommand::UnSetEnvCommand(char *cmd_line)
    : BuiltInCommand(cmd_line) { }

void UnSetEnvCommand::execute() {
    extern char **environ;

    // If no variable names were provided → error
    if (cmd_segments.size() < 2) {
        cerr << "smash error: unsetenv: not enough arguments" << endl;
        return;
    }

    // Iterate over each requested variable
    for (size_t i = 1; i < cmd_segments.size(); ++i) {
        const string &var = cmd_segments[i];
        const string prefix = var + "=";
        int idx = 0;

        // Search for an entry in environ that begins with "VAR="
        while (environ[idx] != nullptr) {
            if (strncmp(environ[idx], prefix.c_str(), prefix.size()) == 0) {
                break;  // found at environ[idx]
            }
            ++idx;
        }

        // If we reached the end without finding it → error and stop
        if (environ[idx] == nullptr) {
            cerr << "smash error: unsetenv: " << var << " does not exist" << endl;
            return;
        }

        // Shift all subsequent entries one slot back to remove environ[idx]
        do {
            environ[idx] = environ[idx + 1];
            ++idx;
        } while (environ[idx] != nullptr);
    }
}

// Jobs command
void JobsCommand::execute() {
    SmallShell::getInstance().getJobs()->printJobsList();
}

// ForeGround command
ForegroundCommand::ForegroundCommand( char *cmd_line): BuiltInCommand(cmd_line) {}

void ForegroundCommand::execute() {
    SmallShell::getInstance().getJobs()->removeFinishedJobs();
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

/// helper functions for watchproc command

static bool read_cpu_times(pid_t pid, long &utime, long &stime) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return false;
    char buf[2048];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return false;
    buf[n] = '\0';

    // Skip pid
    char *p = strchr(buf, ' ');
    if (!p) return false;
    // Skip comm (up to ") ")
    p = strchr(p + 1, ')');
    if (!p) return false;
    p += 2;  // skip ") "
    // Skip state (field 3)
    p = strchr(p, ' ');
    if (!p) return false;
    // Skip fields 4–13
    for (int i = 4; i <= 13; ++i) {
        p = strchr(p + 1, ' ');
        if (!p) return false;
    }
    // Field 14 = utime
    utime = atol(p + 1);
    // Skip utime
    p = strchr(p + 1, ' ');
    if (!p) return false;
    // Field 15 = stime
    stime = atol(p + 1);
    return true;
}

static bool read_rss_kb(pid_t pid, long &rss_kb) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return false;
    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return false;
    buf[n] = '\0';

    char *line = strtok(buf, "\n");
    while (line) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            char *q = line + 6;
            while (*q == ' ' || *q == '\t') ++q;
            rss_kb = atol(q);
            return true;
        }
        line = strtok(nullptr, "\n");
    }
    return false;
}

// Watchproc command
WatchProcCommand::WatchProcCommand( char *cmd_line): BuiltInCommand(cmd_line) {}

void WatchProcCommand::execute() {
    // parse args
    char **args = init_args();
    int num_args = _parseCommandLine(cmd_line, args);
    if (!args) {
        std::cerr << "smash error: watchproc: memory allocation\n";
        return;
    }
    if (num_args != 2 || !is_legit_num(args[1])) {
        std::cerr << "smash error: watchproc: invalid arguments\n";
        free_args(args, num_args);
        return;
    }
    pid_t pid = static_cast<pid_t>(atoi(args[1]));
    free_args(args, num_args);

    // first sample
    long ut1, st1, rss1;
    if (!read_cpu_times(pid, ut1, st1) || !read_rss_kb(pid, rss1)) {
        std::cerr << "smash error: watchproc: pid " << pid << " does not exist or unreadable\n";
        return;
    }

    // sleep 1 second
    struct timespec req = {1, 0};
    nanosleep(&req, nullptr);

    // second sample
    long ut2, st2, rss2;
    if (!read_cpu_times(pid, ut2, st2) || !read_rss_kb(pid, rss2)) {
        std::cerr << "smash error: watchproc: pid " << pid << " terminated\n";
        return;
    }

    // compute CPU% over 1 sec and memory in MB
    long ticks = sysconf(_SC_CLK_TCK);
    long delta_ticks = (ut2 + st2) - (ut1 + st1);
    double cpu_percent = (delta_ticks / static_cast<double>(ticks)) * 100.0;
    double mem_mb = rss2 / 1024.0;

    // print with cout
    std::cout << "PID: " << pid
              << " | CPU Usage: " << std::fixed << std::setprecision(2) << cpu_percent << "%"
              << " | Memory Usage: " << std::fixed << std::setprecision(2) << mem_mb << " MB"
              << std::endl;
} // Need to check calculation of CPU time

// Kill command
KillCommand::KillCommand( char *cmd_line): BuiltInCommand(cmd_line) {}

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
        cerr << "smash error: kill: invalid arguments" << endl;
        free_args(args, num_of_args);
        return;
    }

    int job_id = stoi(args[2]);
    JobsList::JobEntry* job_to_kill = smash.getJobs()->getJobById(job_id);

    // Check whether job exists
    if(!job_to_kill) {
        cerr << "smash error: kill: job-id " << job_id << " does not exist" << endl;
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

ExternalCommand::ExternalCommand(char *cmd_line) : Command(cmd_line) {
    // Store the original command line
    backGround = _isBackgroundComamnd(cmd_line);

    // Create a clean version of the command for segments
    std::string clean_cmd_line = cmd_line;
    //todo check this
    if (backGround) {
        full_cmd = string(cmd_line);
        _removeBackgroundSign(cmd_line);
        // Create a modifiable copy of the command line
        char* cmd_copy = strdup(cmd_line);
        if (cmd_copy) {
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
            if (waitpid(pid, nullptr, 0) == -1)  {
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

// Complex external command
ComplexExternalCommand::ComplexExternalCommand(char *cmd_line): Command(cmd_line), bash_args() {
    backGround = _isBackgroundComamnd(cmd_line);

    if(backGround) {
        _removeBackgroundSign(cmd_line);
    }

    bash_args[0] = (char*)"/bin/bash";
    bash_args[1] = (char*)"-c";
    bash_args[2] = (char*)cmd_line;
    bash_args[3] = nullptr;

}

void ComplexExternalCommand::execute() {
    pid_t pid = fork();

    if(pid == -1) {
        perror("smash error: fork failed");
        return;
    }
    // Child process
    if(pid == 0) {

        if(setpgrp() == -1) {
            perror("smash error: setpgrp failed");
            return;
        }

        execv("/bin/bash", bash_args);

        perror("smash error: execv failed");
        exit(1);
    } else {
        // Parent process
        SmallShell& smash = SmallShell::getInstance();

        if(!backGround) {
            smash.current_process = pid;
            int status;
            // Run in background
            if(waitpid(pid, &status, 0) == -1) {
                perror("smash error: waitpid failed");
                return;
            }
            smash.current_process = -1;
        } else {
            // Add to joblist
            smash.getJobs()->addJob(this, pid, false);
        }
    }
}
/////////////////////////////--------------Special commands-------//////////////////////////////

// Redirection command
RedirectionCommand::RedirectionCommand( char *cmd_line, command_type type):
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

DiskUsageCommand::DiskUsageCommand( char *cmd_line) : Command(cmd_line) {
    createSegments(cmd_line, cmd_segments);
}


void DiskUsageCommand::execute() {
    // Check number of arguments
    if (cmd_segments.size() > 2) {
        std::cerr << "smash error: du: too many arguments" << std::endl;
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
        std::cerr << "smash error: du: directory " << dir_path << " does not exist" << std::endl;
        return;
    }
    closedir(dir);

    // Clear the inode tracking set before calculation
    counted_inodes.clear();

    // Calculate total size
    long size_in_bytes = calculate_dir_size(dir_path);
    if (size_in_bytes < 0) {
        std::cerr << "smash error: du: failed to calculate disk usage" << std::endl;
        return;
    }

    // Convert to KB (1024 bytes = 1 KB)
    long kb_size = (size_in_bytes + 1023) / 1024; // Round up to nearest KB

    // Standard du format: just the size followed by the path
    cout << "Total disk usage: " << kb_size << std::endl;
}

WhoAmICommand::WhoAmICommand( char *cmd_line) : Command(cmd_line) {
    // No special initialization needed since arguments will be ignored
}

void WhoAmICommand::execute() {
    // Get current user ID
    uid_t uid = getuid();
    std::string uid_str = std::to_string(uid);

    // Use environment variables if available (simplest approach)
    char* username_env = getenv("USER");
    char* home_env = getenv("HOME");

    if (username_env && home_env) {
        std::cout << username_env << " " << home_env << std::endl;
        return;
    }

    // If environment variables aren't available, read directly from /etc/passwd
    int fd = open("/etc/passwd", O_RDONLY);
    if (fd == -1) {
        std::cerr << "smash error: whoami: cannot open passwd file" << std::endl;
        return;
    }

    char buffer[4096] = {0};
    read(fd, buffer, sizeof(buffer) - 1);
    close(fd);

    char* line = strtok(buffer, "\n");
    while (line) {
        std::string entry(line);
        size_t pos = 0;
        std::string username;
        std::string home_dir;

        // Get username (field 1)
        pos = entry.find(':');
        if (pos == std::string::npos) {
            line = strtok(NULL, "\n");
            continue;
        }
        username = entry.substr(0, pos);

        // Skip password field (field 2)
        entry = entry.substr(pos + 1);
        pos = entry.find(':');
        if (pos == std::string::npos) {
            line = strtok(NULL, "\n");
            continue;
        }
        entry = entry.substr(pos + 1);

        // Get UID (field 3)
        pos = entry.find(':');
        if (pos == std::string::npos) {
            line = strtok(NULL, "\n");
            continue;
        }
        std::string uid_field = entry.substr(0, pos);
        if (uid_field == uid_str) {
            // Found our user, now get home directory
            entry = entry.substr(pos + 1);

            // Skip GID and GECOS fields (fields 4 and 5)
            for (int i = 0; i < 2; i++) {
                pos = entry.find(':');
                if (pos == std::string::npos) break;
                entry = entry.substr(pos + 1);
            }

            // Get home directory (field 6)
            pos = entry.find(':');
            if (pos != std::string::npos) {
                home_dir = entry.substr(0, pos);
                std::cout << username << " " << home_dir << std::endl;
                return;
            }
        }

        line = strtok(NULL, "\n");
    }

    std::cerr << "smash error: whoami: user not found" << std::endl;
}

// Pipe command
PipeCommand::PipeCommand( char *cmd_line, Type command_type): Command(cmd_line), command_type(command_type) {
    string cmd1, cmd2;
    size_t index;
    string input(cmd_line);
    // Look for the delimiter
    if(command_type == STDOUT){
        index = input.find('|');
    } else {
        index = input.find("|&");
    }

    // Retrieving the first command
    cmd1 = _trim(input.substr(0, index));
    command1 = (char*) malloc(sizeof (char) * (cmd1.length() + 1));

    // Check whether malloc succeed
    if(!command1) {
        perror("smash error: malloc failed");
        throw bad_alloc();
    }

    strcpy(command1, cmd1.c_str());

    // Retrieving the second command

    cmd2 = _trim(input.substr(index + command_type));
    command2 = (char*) malloc(sizeof(char) * (cmd2.length() + 1));

    // Check whether malloc succeed
    if(!command2){
        perror("smash error: malloc failed");
        free(command1);
        throw bad_alloc();
    }

    strcpy(command2, cmd2.c_str());
}

void PipeCommand::execute() {
    int fd[2];
    if (pipe(fd) == -1) {
        perror("smash error: pipe failed");
        return;
    }

    pid_t pid1 = fork();
    // Check whether fork succeed
    if(pid1 == -1){
        perror("smash error: fork failed");
        close_pipe(fd);
        return;
    }

    // Writer process
    if(pid1 == 0){
        // Set gid to pid
        if(setpgrp() == -1){
            perror("smash error: setpgrp failed");
            close_pipe(fd);
            return;
        }
        // Redirect output
        if(command_type == STDOUT) {
            if(dup2(fd[1], 1) == -1) {
                perror("smash error: dup2 failed");
                close_pipe(fd);
                return;
            }
        } else {
            if(dup2(fd[1], 2) == -1) {
                perror("smash error: dup2 failed");
                close_pipe(fd);
                return;
            }
        }
        if(!close_pipe(fd)) return;
        SmallShell::getInstance().executeCommand(command1);
        exit(1);
    }

    pid_t pid2 = fork();
    if(pid2 == -1) {
        perror("smash error: fork failed");
        close_pipe(fd);
        return;
    }

    // Reader process
    if(pid2 == 0){
        // Change gid to pid
        if(setpgrp() == -1) {
            perror("smash error: setpgrp failed");
            close_pipe(fd);
            return;
        }

        // Redirect input
        if(dup2(fd[0], 0) == -1) {
            perror("smash error: dup2 failed");
            close_pipe(fd);
            return;
        }
        if (!close_pipe(fd)) return;
        SmallShell::getInstance().executeCommand(command2);
        exit(1);
    }

    // Closing pipe
    if(!close_pipe(fd)) return;

    // Wait child processes

    if (waitpid(pid1, nullptr, WUNTRACED) == -1) {
        perror("smash error: waitpid failed");
        return;
    }

    if (waitpid(pid2, nullptr, WUNTRACED) == -1) {
        perror("smash error: waitpid failed");
        return;
    }


}

bool PipeCommand::close_pipe(int *fd) {
    bool ret = true;
    if (close(fd[0]) == -1){
        perror("smash error: close failed");
        ret = false;
    }
    if (close(fd[1]) == -1){
        perror("smash error: close failed");
        ret = false;
    }
    return ret;
} // Helper function to close both file descriptors of the pipe

PipeCommand::~PipeCommand() {
    free(command1);
    free(command2);
}

// NetInfo command
NetInfo::NetInfo(char *cmd_line) : Command(cmd_line) {}

void NetInfo::execute() {
    char** args = init_args();
    int num_args = _parseCommandLine(cmd_line, args);
    if(!args) {
        cerr << "smash error: watchproc: malloc failed" << endl;
        return;
    }
    if(num_args == 1) {
        cerr << "smash error: netinfo: interface not specified" << endl;
        free_args(args, num_args);
        return;
    }
    string interface_info[3];
    char* interface = args[1];
    // Check if interface exists, and obtain ip and subnet
    if(get_ip_and_subnet(interface, interface_info) == -1) {
        cerr << "smash error: netinfo: interface " << interface << " does not exists" << endl;
        free_args(args, num_args);
        return;
    }
    get_default_gateway(interface_info);
    vector<string> dns_servers;
    get_DNS_servers(dns_servers);

    // Print info
    cout << "IP Address: " << interface_info[0] << endl;
    cout << "Subnet Mask: " << interface_info[1] << endl;
    cout << "Default Gateway: " << interface_info[2] << endl;
    cout << "DNS Servers: ";
    for(int i = 0; i < dns_servers.size() - 1; i++){
        cout << dns_servers[i] << ", ";
    }
    cout << dns_servers[dns_servers.size() - 1] << endl;
}

int get_ip_and_subnet(const char* interface, string to_fill[]) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        //perror("smash error: NetInfo: socket creation failed");
        return -1;
    }

    // Set the name to the given
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);
    // Obtain IP address
    if(ioctl(sock, SIOCGIFADDR, &ifr) == -1) {
        //perror("smash error: NetInfo: ioctl failed");
        return -1;
    }
    struct sockaddr_in* ipaddr = (struct sockaddr_in*)&ifr.ifr_addr;
    to_fill[0] = inet_ntoa(ipaddr->sin_addr);

    // Obtain subnet mask
    if(ioctl(sock, SIOCGIFNETMASK, &ifr) == -1) {
        //perror("smash error: NetInfo: ioctl failed");
        return -1;
    }
    struct sockaddr_in* netmask = (struct sockaddr_in*)&ifr.ifr_addr;
    to_fill[1] = inet_ntoa(netmask->sin_addr);
    close(sock);
    return 0;
}

int get_default_gateway(string to_fill[]) {
    int fd = open("/proc/net/route", O_RDONLY);
    if(fd < 0) {
        perror("smash error: open failed");
        return -1;
    }
    // Read from the file to the buffer
    char buffer[4096];
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if(bytes_read <= 0) {
        perror("smash error: read failed");
        close(fd);
        return -1;
    }
    buffer[bytes_read] = '\0';

    //Split the buffer into lines
    char* line = strtok(buffer, "\n");
    while(line) {
        char iface[16], destination[16], gateway[16];

        if(sscanf(line, "%15s %15s %15s", iface, destination, gateway) == 3) {
            if (strcmp(destination, "00000000") == 0) {
                unsigned long gw = strtoul(gateway, nullptr, 16);
                struct in_addr addr;
                addr.s_addr = gw;
                to_fill[2] = inet_ntoa(addr);
            }
        }
        line = strtok(nullptr, "\n");
    }
    close(fd);
    return 0;
}

int get_DNS_servers(vector<string>& dns_servers) {
    int fd = open("/etc/resolv.conf", O_RDONLY);
    if(fd < 0) {
        perror("smash error: open failed");
        return -1;
    }

    // Read from the file to the buffer
    char buffer[4096];
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if(bytes_read <= 0) {
        perror("smash error: read failed");
        close(fd);
        return -1;
    }
    buffer[bytes_read] = '\0';

    char* line = strtok(buffer, "\n");
    while(line) {
        if(strncmp(line, "nameserver", 10) == 0 && (line[10] == ' ' || line[10] == '\t')) {
            dns_servers.emplace_back(line + 11);
        }
        line = strtok(nullptr, "\n");
    }
    close(fd);
    return 0;
}