// Ver: 10-4-2025
#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_
using namespace std;
#include <vector>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <vector>
#include <string>
#include <unordered_set>
#include <dirent.h>
#include <sys/stat.h>
#include <iostream>
#include <cstring>
#include <limits.h>


#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

/////////////////////////////--------------Class declarations-------//////////////////////////////

class Command {
    // TODO: Add your data members
protected:
    bool backGround;
    vector<string> cmd_segments;
    const char* cmd_line;
public:
    explicit Command( char *cmd_line): cmd_line(cmd_line){};

    virtual ~Command() = default;

    virtual void execute() = 0;

    virtual string getCommandS();
    string printCommand();
    bool hasAlias();
    void setAlias(string command);
};

class JobsList {
public:
    class JobEntry {
    public:
        Command* cmd;
        bool isStopped;
        int jobId;
        pid_t pid;
        string command;
        JobEntry(Command* cmd, bool isStopped, int jobId, int pid, string command) :
                cmd(cmd), isStopped(isStopped), jobId(jobId), pid(pid), command(command) {} //maybe pass by reference the command
        ~JobEntry() = default; //maybe delete cmd?
        bool operator<(const JobEntry& other) const;
    };
    list<JobEntry*> jobsList;

    std::unordered_map<int, JobEntry*> job_map; //I think we need to add it for more efficient search

    int max_id;

    JobsList();

    ~JobsList() = default;

    void addJob(Command *cmd, pid_t pid, bool isStopped = false);

    void printJobsList();

    void killAllJobs();

    void removeFinishedJobs();

    JobEntry *getJobById(int jobId);

    void removeJobById(int jobId);

    JobEntry *getLastJob(int *lastJobId);

    JobEntry *getLastStoppedJob(int *jobId);

    void printJobsBeforeQuit();

    // TODO: Add extra methods or modify exisitng ones as needed
};

class SmallShell {
private:
    map<string,string> aliasMap;
    vector<string> sortedAlias;
    string prompt;
    char* prevWorkingDir;
    JobsList* jobList;
    vector<string> commands;
    SmallShell();

public:
    pid_t current_process;
    pid_t pid;
    Command *CreateCommand(char* cmd_line);

    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    ~SmallShell();

    void executeCommand( char *cmd_line);


    // TODO: add extra methods as needed
    void setPrompt(string newPrompt);
    string getPrompt() const;
    char* getCurrWorkingDir() const;
    void setCurrWorkingDir(string newDir);
    string getPrevWorkingDir() const;
    void setPrevWorkingDir(string newDir);
    JobsList* getJobs();
    void getAllAlias(std::vector<std::string>& aliases);
    string getAlias(string name);
    bool validCommand(string name);
    void createCommandVector();
    void setAlias(string name, string command);
    bool removeAlias(string name);
};

/////////////////////////////--------------Built-in commands-------//////////////////////////////

class BuiltInCommand : public Command {
public:
    explicit BuiltInCommand( char *cmd_line);

    virtual ~BuiltInCommand() {
    }
}; // TODO: Maybe remove '&' in constructor?

class ChpromptCommand : public BuiltInCommand {
public:
    string newSmashPrompt;
    ChpromptCommand( char* cmd_line);
    virtual ~ChpromptCommand() {}
    void execute() override;
}; // DONE

class ChangeDirCommand : public BuiltInCommand {
public:
    // TODO: Add your data members public:
    char** plastPwd;
    ChangeDirCommand( char *cmd_line, char **plastPwd);

    virtual ~ChangeDirCommand() = default;

    void execute() override;
}; // DONE

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand( char *cmd_line);

    virtual ~GetCurrDirCommand() {
    }

    void execute() override;
}; // DONE

class ShowPidCommand : public BuiltInCommand {
public:
    explicit ShowPidCommand( char *cmd_line);

    virtual ~ShowPidCommand() = default;

    void execute() override;
}; // DONE

class QuitCommand : public BuiltInCommand {
    // TODO: Add your data members
public:
    JobsList* jobs;

    QuitCommand( char *cmd_line, JobsList *jobs);

    virtual ~QuitCommand() {
    }

    void execute() override;
}; // DONE

class JobsCommand : public BuiltInCommand {
    //JobsList* jobs;
public:
    JobsCommand( char *cmd_line);

    virtual ~JobsCommand() {
    }

    void execute() override;
}; // DONE

class KillCommand : public BuiltInCommand {
    // TODO: Add your data members
public:
    KillCommand( char *cmd_line);

    virtual ~KillCommand() {
    }

    void execute() override;
}; // DONE

class ForegroundCommand : public BuiltInCommand {
    // TODO: Add your data members
public:
    ForegroundCommand( char *cmd_line);

    virtual ~ForegroundCommand() {
    }

    void execute() override;
}; // DONE

class AliasCommand : public BuiltInCommand {
public:
    AliasCommand( char *cmd_line);

    virtual ~AliasCommand() {
    }

    void execute() override;
}; // DONE

class UnAliasCommand : public BuiltInCommand {
public:
    UnAliasCommand( char *cmd_line);

    virtual ~UnAliasCommand() {
    }

    void execute() override;
}; // DONE

class UnSetEnvCommand : public BuiltInCommand {
public:
    UnSetEnvCommand( char *cmd_line);

    virtual ~UnSetEnvCommand() {
    }

    void execute() override;
}; // DONE

class WatchProcCommand : public BuiltInCommand {
public:
    WatchProcCommand( char *cmd_line);

    virtual ~WatchProcCommand() {
    }

    void execute() override;
};   // DONE

/////////////////////////////--------------Special commands-------//////////////////////////////

class RedirectionCommand : public Command {
    char* command;
    char* file_name;
    int stdout_copy;
public:
    enum command_type {
        TRUNCATE = 1,
        CONCAT = 2
    };

    command_type type;

    explicit RedirectionCommand( char *cmd_line, command_type type);

    virtual ~RedirectionCommand();

    void execute() override;
}; // DONE

class PipeCommand : public Command {
    // TODO: Add your data members
public:

    char* command1;
    char* command2;

    enum Type{
        STDOUT = 1,
        STDERR = 2
    };

    Type command_type;

    PipeCommand( char *cmd_line, Type command_type);

    virtual ~PipeCommand();

    bool close_pipe(int* fd);

    void execute() override;
};

class DiskUsageCommand : public Command {
private:
    std::unordered_set<ino_t> counted_inodes;
    long calculate_dir_size(const char* path) {
        struct stat st;
        if (lstat(path, &st) != 0) {
            return 0;  // can't stat this entry
        }

        // Only count each inode once (prevents double-counting hard links)
        if (counted_inodes.insert(st.st_ino).second == false) {
            return 0;
        }

        // Start with this entry's blocks
        long size = st.st_blocks * 512;

        // If it's a directory (and not a symlink) recurse into it
        if (S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode)) {
            DIR* dir = opendir(path);
            if (!dir) {
                return size;
            }

            struct dirent* entry;
            while ((entry = readdir(dir)) != NULL) {
                if (strcmp(entry->d_name, ".") == 0 ||
                    strcmp(entry->d_name, "..") == 0) {
                    continue;
                    }

                char child[PATH_MAX];
                snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
                size += calculate_dir_size(child);
            }
            closedir(dir);
        }

        return size;
    }

public:
    DiskUsageCommand( char *cmd_line);

    virtual ~DiskUsageCommand() {
    }

    void execute() override;
};

class WhoAmICommand : public Command {
public:
    WhoAmICommand( char *cmd_line);

    virtual ~WhoAmICommand() {
    }

    void execute() override;
};

class NetInfo : public Command {
    // TODO: Add your data members **BONUS: 10 Points**
public:
    NetInfo( char *cmd_line);

    virtual ~NetInfo() {
    }

    void execute() override;
};

class ExternalCommand : public Command {
public:
    string full_cmd;
    ExternalCommand(char *cmd_line);

    virtual ~ExternalCommand() {
    }
    string getCommandS() override;
    void execute() override;
}; // DONE

class ComplexExternalCommand : public Command {
public:
    char* bash_args[4];
    ComplexExternalCommand(char *cmd_line);

    virtual ~ComplexExternalCommand() {
    }

    void execute() override;
}; // DONE

void removeBackgroundSignFromString(std::string& cmd_line);

#endif //SMASH_COMMAND_H_