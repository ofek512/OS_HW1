// Ver: 10-4-2025
#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_
using namespace std;
#include <vector>
#include <list>
#include <unordered_map>
#include <map>

#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class Command {
    // TODO: Add your data members
protected:
    bool backGround;
    vector<string> cmd_segments;
    const char* cmd_line;
public:
    explicit Command(const char *cmd_line): cmd_line(cmd_line){};

    virtual ~Command() = default;

    virtual void execute() = 0;

    //virtual void prepare();
    //virtual void cleanup();
    // TODO: Add your extra methods if needed
    /*string getPath();
    void setPath(string path);*/
    string printCommand();
    bool hasAlias();
    void setAlias(string command);
};

class BuiltInCommand : public Command {
public:
    explicit BuiltInCommand(const char *cmd_line);

    virtual ~BuiltInCommand() {
    }
};

//chprompt - ofek

class ChpromptCommand : public BuiltInCommand {
public:
    string newSmashPrompt;
    ChpromptCommand(const char* cmd_line);
    virtual ~ChpromptCommand() {}
    void execute() override;
}; // DONE

class ExternalCommand : public Command {
public:
    ExternalCommand(const char *cmd_line);

    virtual ~ExternalCommand() {
    }

    void execute() override;
};


class RedirectionCommand : public Command {
    // TODO: Add your data members
public:
    explicit RedirectionCommand(const char *cmd_line);

    virtual ~RedirectionCommand() {
    }

    void execute() override;
};

class PipeCommand : public Command {
    // TODO: Add your data members
public:
    PipeCommand(const char *cmd_line);

    virtual ~PipeCommand() {
    }

    void execute() override;
};

class DiskUsageCommand : public Command {
public:
    DiskUsageCommand(const char *cmd_line);

    virtual ~DiskUsageCommand() {
    }

    void execute() override;
};

class WhoAmICommand : public Command {
public:
    WhoAmICommand(const char *cmd_line);

    virtual ~WhoAmICommand() {
    }

    void execute() override;
};

class NetInfo : public Command {
    // TODO: Add your data members **BONUS: 10 Points**
public:
    NetInfo(const char *cmd_line);

    virtual ~NetInfo() {
    }

    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
public:
    // TODO: Add your data members public:
    char** plastPwd;
    ChangeDirCommand(const char *cmd_line, char **plastPwd);

    virtual ~ChangeDirCommand() = default;

    void execute() override;
}; // DONE

// pwd - ofek

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char *cmd_line);

    virtual ~GetCurrDirCommand() {
    }

    void execute() override;
}; // DONE

class ShowPidCommand : public BuiltInCommand {
public:
    explicit ShowPidCommand(const char *cmd_line);

    virtual ~ShowPidCommand() = default;

    void execute() override;
}; // DONE

class JobsList;

class QuitCommand : public BuiltInCommand {
    // TODO: Add your data members
public:
    JobsList* jobs;

    QuitCommand(const char *cmd_line, JobsList *jobs);

    virtual ~QuitCommand() {
    }

    void execute() override;
}; // DONE


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

//jobs - ofek

class JobsCommand : public BuiltInCommand {
    //JobsList* jobs;
public:
    JobsCommand(const char *cmd_line);

    virtual ~JobsCommand() {
    }

    void execute() override;
}; // DONE

class KillCommand : public BuiltInCommand {
    // TODO: Add your data members
public:
    KillCommand(const char *cmd_line, JobsList *jobs);

    virtual ~KillCommand() {
    }

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    // TODO: Add your data members
public:
    ForegroundCommand(const char *cmd_line, JobsList *jobs);

    virtual ~ForegroundCommand() {
    }

    void execute() override;
};

class AliasCommand : public BuiltInCommand {
public:
    AliasCommand(const char *cmd_line);

    virtual ~AliasCommand() {
    }

    void execute() override;
}; // DONE

class UnAliasCommand : public BuiltInCommand {
public:
    UnAliasCommand(const char *cmd_line);

    virtual ~UnAliasCommand() {
    }

    void execute() override;
};

class UnSetEnvCommand : public BuiltInCommand {
public:
    UnSetEnvCommand(const char *cmd_line);

    virtual ~UnSetEnvCommand() {
    }

    void execute() override;
};

class WatchProcCommand : public BuiltInCommand {
public:
    WatchProcCommand(const char *cmd_line);

    virtual ~WatchProcCommand() {
    }

    void execute() override;
};

class SmallShell {
private:
    // TODO: Add your data members
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
    Command *CreateCommand(const char *cmd_line);

    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    ~SmallShell();

    void executeCommand(const char *cmd_line);


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
};


void removeBackgroundSignFromString(std::string& cmd_line);

#endif //SMASH_COMMAND_H_
