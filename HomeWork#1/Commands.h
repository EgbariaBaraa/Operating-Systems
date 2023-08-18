#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_


#include <string>
#include <time.h>
#include <list>
#include <vector>

using std::string;
using std::list;


#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class Command
{
public:
    const char* cmd_line;
 
    Command(const char* cmd_line) : cmd_line(cmd_line) {}
    virtual ~Command() {}
    virtual void execute() = 0;
    //virtual void prepare();
    //virtual void cleanup(&);
};

class JobsList
{
 public:
    enum STATUS {BG,FG,STOPPED};
    class JobEntry
    {
    public:
       
        int job_id;
        int process_id;
        string cmd_line;
        STATUS status;
        time_t insert_time;
        
        // JobEntry() = default;
        JobEntry(int job_id, int process_id, const char* cmd, STATUS status, time_t insert_time = time(nullptr)) : job_id(job_id), process_id(process_id), cmd_line(cmd), status(status),  insert_time(insert_time) {};
    };
    
    list<JobEntry*> job_list;
    
 public:
    JobsList() = default;
    ~JobsList() = default;
    void addJob(Command* cmd, pid_t process_id, STATUS status);
    void printJobsList();
    void killAllJobs();
    void removeFinishedJobs();
    JobEntry * getJobById(int jobId);
    void removeJobById(int jobId);
    JobEntry * getLastJob(int* lastJobId);
    JobEntry *getLastStoppedJob(int *jobId);
    
};

class Timeout {
public:
    std::string cmd_line;
    time_t duration;
    time_t timestamps;
    pid_t pid;
    
    Timeout() = default;
    Timeout(std::string cmd_line, time_t duration, time_t timestamps, pid_t pid ): cmd_line(cmd_line), duration(duration), timestamps(timestamps), pid(pid) {};
    ~Timeout(){}
    
    bool operator>(const Timeout& cmd1) const;
    bool operator<(const Timeout& cmd1) const;
    bool operator==(const Timeout& cmd1) const;
    
};


class SmallShell
{
 private:
    
    SmallShell();
 public:
    string default_name;
    string shell_name;
    string last_dir;
    pid_t smash_pid;
    
    JobsList jobs;
    list<Timeout> TimesSet;
    
    string curr_cmd;
    pid_t running_pid;
    
    bool is_timeout_cmd;
    
    Command *CreateCommand(const char* cmd_line);
    SmallShell(SmallShell const&)      = delete; // disable copy ctor
    void operator=(SmallShell const&)  = delete; // disable = operator
    static SmallShell& getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }
    ~SmallShell();
    
    void executeCommand(const char* cmd_line);
    string getlastDir() const;
    void setLastDir(string to_set);

  // TODO: add extra methods as needed
    string getName() const;
    void setName(string to_set);
    void resetName();
};



// ***************************************** //
// *                                       * //
// *                                       * //
// ******* Built In Commands Classes ******* //
// *                                       * //
// *                                       * //
// ***************************************** //

class BuiltInCommand : public Command
{
 public:
    JobsList* jobs;
    BuiltInCommand(const char* cmd_line, JobsList* jobs = nullptr) : Command(cmd_line), jobs(jobs) {}
    virtual ~BuiltInCommand() {}
};

class ChPromptCommand : public BuiltInCommand
{
    string new_name;
public:
    ChPromptCommand(const char* cmd_line);
    virtual ~ChPromptCommand() {}
    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
 public:
    ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};
    virtual ~ShowPidCommand() {}
    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
 public:
    GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};
    virtual ~GetCurrDirCommand() {}
    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand
{
    string* pointer_to_last_dir;
    string given_argument;
    int num_of_args;
    static void changeDir(const char* to_change_to);
    
 public:
    ChangeDirCommand(const char* cmd_line, string* plastPwd);
    virtual ~ChangeDirCommand() {}
    void execute() override;
};

class JobsCommand : public BuiltInCommand
{
 public:
    JobsCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line, jobs) {};
    virtual ~JobsCommand() {}
    void execute() override;
};

class ForegroundCommand : public BuiltInCommand
{
    int job_id;
    bool error_flag;
 public:
    ForegroundCommand(const char* cmd_line, JobsList* jobs);
    virtual ~ForegroundCommand() {}
    void execute() override;
};

class BackgroundCommand : public BuiltInCommand
{
    int job_id;
    bool error_flag;
 public:
    BackgroundCommand(const char* cmd_line, JobsList* jobs);
    virtual ~BackgroundCommand() {}
    void execute() override;
};

class QuitCommand : public BuiltInCommand
{
    bool print_jobs;
public:
    QuitCommand(const char* cmd_line, JobsList* jobs);
    virtual ~QuitCommand() {}
    void execute() override;
};

class KillCommand : public BuiltInCommand {
 // TODO: Add your data members
    
    int signum;
    int jobId;
    bool error_flag;
 public:
    KillCommand(const char* cmd_line, JobsList* jobs);
  virtual ~KillCommand() {}
  void execute() override;
};


// ***************************************** //
// *                                       * //
// *                                       * //
// ******* External Commands Classes ******* //
// *                                       * //
// *                                       * //
// ***************************************** //

class ExternalCommand : public Command
{
 public:
    ExternalCommand(const char* cmd_line) : Command(cmd_line) {};
    virtual ~ExternalCommand() {}
    void execute() override;
    void execSimpleCommand(const char* cmd_line);
    void execComplexCommand(const char* cmd_line);
};


// ***************************************** //
// *                                       * //
// *                                       * //
// ******** Special Commands Classes ******* //
// *                                       * //
// *                                       * //
// ***************************************** //

class PipeCommand : public Command
{
    bool error_flag;
    string cmd1;
    string cmd2;
    int status;
    int to_write;
 public:
    enum Status {READ, WRITE};
    PipeCommand(const char* cmd_line) : Command(cmd_line), error_flag(false) {};
    virtual ~PipeCommand() {}
    void execute() override;
    void prepare();
    // void cleanup();
    void FdCloseEntries(int* fd, Status pipe_status, int fd_id);
};

class RedirectionCommand : public Command {
    bool error_flag;
    string cmd;
    string file_name;
    int stdout_fd;
    int fd;
 public:
    explicit RedirectionCommand(const char* cmd_line) : Command(cmd_line), error_flag(false) {};
    virtual ~RedirectionCommand() {}
    void execute() override;
    void prepare(int fd_id);
    void cleanup();
};



// **** Optional Commands **** //
 
class TimeoutCommand : public BuiltInCommand {
 public:
    bool error_flag;
    string to_exec;
    explicit TimeoutCommand(const char* cmd_line);
    virtual ~TimeoutCommand() {}
    void execute() override;
    void prepare();
    void cleanup();
};

class FareCommand : public BuiltInCommand
{
  string file_name;
  string to_change;
  string to_change_to;
  bool error;
  int   file_fd;
  int number_of_times;
 public:
  FareCommand(const char* cmd_line);
  virtual ~FareCommand() {}
  void execute() override;
};

class SetcoreCommand : public BuiltInCommand
{
 public:
  SetcoreCommand(const char* cmd_line);
  virtual ~SetcoreCommand() {}
  void execute() override;
};



#endif //SMASH_COMMAND_H_
