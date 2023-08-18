#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <limits.h>
#include <iomanip>
#include "Commands.h"
#include <signal.h>
#include <fstream>

using std::list;
using std::endl;
using std::cerr;
using std::cout;
using std::vector;

const std::string WHITESPACE = " \n\r\t\f\v";
const std::string AND = "&";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif


// **** start of Aux Functions **** ///

string _ltrim(const std::string& s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _rtrimAndSign(const std::string& s)
{
  size_t end = s.find_last_not_of(AND);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
  return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
  FUNC_ENTRY()
  int i = 0;
  std::istringstream iss(_trim(string(cmd_line)).c_str());
  for(std::string s; iss >> s; ) {
    args[i] = (char*)malloc(s.length()+1);
    memset(args[i], 0, s.length()+1);
    strcpy(args[i], s.c_str());
    args[++i] = NULL;
  }
  return i;

  FUNC_EXIT()
}

bool _isBackgroundComamnd(const char* cmd_line) {
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}

string _removeBackgroundSign(const char* cmd_line)
{
  const string str(cmd_line);
    string to_ret=str;
  // find last character other than spaces
  unsigned int idx = str.find_last_not_of(WHITESPACE);
  // if all characters are spaces then return
  if (idx == string::npos)
      return to_ret;
    
  // if the command line does not end with & then return
  if (cmd_line[idx] != '&')
    return to_ret;
    
  // replace the & (background sign) with space and then remove all tailing spaces.
  to_ret[idx] = ' ';
  // truncate the command line string up to the last non-space character
  to_ret[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
    return to_ret;
}

// **** End of Aux Functions **** ///


// **** Start of Shell Class Functions **** ///


SmallShell::SmallShell()  : default_name("smash> "), shell_name(default_name), last_dir(""), smash_pid(getpid()), jobs(), curr_cmd(""), running_pid(-1), is_timeout_cmd(false) {}

SmallShell::~SmallShell()
{
    
}

// **** Our Aux Functions **** //
void freeAllocatedStrings(char** words, int num_of_args)
{
    for(int i=0; i<num_of_args; i++)
        free(words[i]);
}


/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {
    
    string cmd_s = _trim(string(cmd_line));
    SmallShell& shell = SmallShell::getInstance();
    
    if(is_timeout_cmd)
    {
        char* words[COMMAND_MAX_ARGS + 1];
        int num_of_args = _parseCommandLine(cmd_line, words);
        string time = string(words[1]);
        size_t time_pos = string(cmd_line).find(time);
        string line = string(cmd_line).substr(time_pos + time.size());
        cmd_s = _trim(line);
        freeAllocatedStrings(words, num_of_args);
    }
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
    
    // Special Commands
    if(cmd_s.find(">") != string::npos)
        return new RedirectionCommand(cmd_line);
    else if(cmd_s.find("|") != string::npos)
        return new PipeCommand(cmd_line);
    
    // Built-in Commands
    else if (firstWord.compare("pwd") == 0 || firstWord.compare("pwd&") == 0)
        return new GetCurrDirCommand(cmd_line);
    else if (firstWord.compare("showpid") == 0 || firstWord.compare("showpid&") == 0)
        return new ShowPidCommand(cmd_line);
    else if (firstWord.compare("chprompt") == 0 || firstWord.compare("chprompt&") == 0)
        return new ChPromptCommand(cmd_line);
    else if (firstWord.compare("cd") == 0)
        return new ChangeDirCommand(cmd_line, &(last_dir));
    else if (firstWord.compare("jobs") == 0 || firstWord.compare("jobs&") == 0)
        return new JobsCommand(cmd_line, &(shell.jobs));
    else if (firstWord.compare("fg") == 0 || firstWord.compare("fg&") == 0)
        return new ForegroundCommand(cmd_line, &(shell.jobs));
    else if (firstWord.compare("bg") == 0 || firstWord.compare("bg&") == 0)
        return new BackgroundCommand(cmd_line, &(shell.jobs));
    else if(firstWord.compare("quit") == 0 || firstWord.compare("quit&") == 0)
        return new QuitCommand(cmd_line, &(shell.jobs));
    else if(firstWord.compare("kill") == 0)
        return new KillCommand(cmd_line, &(shell.jobs));
    
    else if(firstWord.compare("fare") == 0)
        return new FareCommand(cmd_line);
    if(firstWord.compare("timeout") == 0)
        return new TimeoutCommand(cmd_line);
    else if (cmd_s == "")
        return nullptr;
    else
        return new ExternalCommand(cmd_line);
}

void SmallShell::executeCommand(const char *cmd_line)
{
    Command* cmd = CreateCommand(cmd_line);
    if (cmd == nullptr)
        return;
    SmallShell& shell = SmallShell::getInstance();
    shell.jobs.removeFinishedJobs();
    cmd->execute();
}

string SmallShell::getName() const
{
    return shell_name;
}

void SmallShell::setName(string to_set)
{
    shell_name = (to_set+"> ");
}

void SmallShell::resetName()
{
    shell_name = default_name;
}


string SmallShell::getlastDir() const
{
    return last_dir;
}

void SmallShell::setLastDir(std::string to_set)
{
    last_dir=to_set;
}

// **** End of Shell Class Functions **** ///





// ********** JobsList methods ********** //

JobsList::JobEntry* findJob(std::list<JobsList::JobEntry*>& jobs, pid_t process_id)
{
    for(list<JobsList::JobEntry*>::iterator it = jobs.begin(); it != jobs.end(); it++)
    {
        if((*it)->process_id != process_id)
            continue;
        return *it;
    }
    return nullptr;
}

void JobsList::addJob(Command* cmd, pid_t process_id, STATUS status)
{
    SmallShell& shell = SmallShell::getInstance();
    std::list<JobEntry*>& jobs = shell.jobs.job_list;
    
    // TODO: CEHCK THE REMOVE FINISHED JOBS TASK
    
    int size = (int) jobs.size();
    if(size == 0)
    {
        // shell.jobs.removeFinishedJobs();
        JobEntry* new_job = new JobEntry(1, process_id, cmd->cmd_line, status);
        shell.jobs.job_list.push_back(new_job);
        return;
    }
    
    JobEntry* job = findJob(jobs, process_id);
    if(job == nullptr)
    {
        shell.jobs.removeFinishedJobs();
        size = (int) jobs.size();
        int to_give = (size == 0 ? 1 : (jobs.back()->job_id + 1));
        JobEntry* new_job = new JobEntry(to_give, process_id, cmd->cmd_line, status);
        shell.jobs.job_list.push_back(new_job);
        return;
    }
    else
        job->status = status;
}


void JobsList::removeFinishedJobs()
{
    SmallShell& shell = SmallShell::getInstance();
    std::list<JobEntry*>& jobs = shell.jobs.job_list;
    JobEntry** not_finished = new JobEntry*[jobs.size()];
    
    int  i=0;
    for(list<JobsList::JobEntry*>::iterator it = jobs.begin(); it != jobs.end(); it++)
    {
        int res = waitpid((*it)->process_id, nullptr, WNOHANG);
        if(res == -1 && errno == ECHILD)
			continue;
        else if(res == -1)
            perror("smash error: waitpid failed");
        else if(res == 0)
            not_finished[i++] = *it;
    }
    jobs.clear();
    for(int j=0; j<i; j++)
        jobs.push_back(not_finished[j]);
    
    delete[] not_finished;
}

void JobsList::printJobsList()
{
    SmallShell& shell = SmallShell::getInstance();
    std::list<JobEntry*>& jobs = shell.jobs.job_list;
    for(list<JobsList::JobEntry*>::iterator it = jobs.begin(); it != jobs.end(); it++)
    {
        JobEntry* job = (*it);
        cout << "["<< job->job_id << "] " << job->cmd_line;
        time_t time_diff = difftime(time(nullptr), job->insert_time);
        cout << " : " << job->process_id << " " << time_diff << " secs";
        if(job->status == STOPPED)
            cout << " (stopped)";
        cout << endl;
    }
}

JobsList::JobEntry*  JobsList::getLastJob(int* lastJobId)
{
    SmallShell& shell = SmallShell::getInstance();
    std::list<JobEntry*>& jobs = shell.jobs.job_list;
    if(jobs.back() == nullptr)
        return nullptr;
    *lastJobId = jobs.back()->job_id;
    return jobs.back();
}

JobsList::JobEntry* JobsList::getLastStoppedJob(int *jobId)
{
    SmallShell& shell = SmallShell::getInstance();
    std::list<JobEntry*>& jobs = shell.jobs.job_list;
    JobsList::JobEntry* to_ret = nullptr;
    for(list<JobsList::JobEntry*>::iterator it = jobs.begin(); it != jobs.end(); it++)
        if((*it)->status == STOPPED)
        {
            *jobId = (*it)->job_id;
            to_ret = *it;
        }
    return to_ret;
}

void JobsList::removeJobById(int jobId)
{
    SmallShell& shell = SmallShell::getInstance();
    std::list<JobEntry*>& jobs = shell.jobs.job_list;
    for(list<JobsList::JobEntry*>::iterator it = jobs.begin(); it != jobs.end(); it++)
        if((*it)->job_id == jobId)
        {
            jobs.erase(it);
            return;
        }
}

JobsList::JobEntry* JobsList::getJobById(int jobId)
{
    SmallShell& shell = SmallShell::getInstance();
    std::list<JobEntry*>& jobs = shell.jobs.job_list;
    for(list<JobsList::JobEntry*>::iterator it = jobs.begin(); it != jobs.end(); it++)
        if((*it)->job_id == jobId)
            return *it;
    return nullptr;
}

// ********** END of JobsList methods ********** //



// ***************************************** //
// *                                       * //
// *                                       * //
// ******* Built In Commands Classes ******* //
// *                                       * //
// *                                       * //
// ***************************************** //


//  1.  // **** ChPrompt Command **** //     //
ChPromptCommand::ChPromptCommand(const char* cmd_line) : BuiltInCommand(cmd_line)
{
    char* words[COMMAND_MAX_ARGS + 1];
    int num_of_args = _parseCommandLine(cmd_line,words);
    
    if(num_of_args == 1)
        new_name = "";
    else
        new_name = string(words[1]);
    
    freeAllocatedStrings(words, num_of_args);
}

void ChPromptCommand::execute()
{
    SmallShell& shell = (SmallShell::getInstance());
    if(new_name == "")
        shell.resetName();
    else
        shell.setName(new_name);
}
//  1.  // **** End Of ChPrompt Command **** //     //


//  2.  // **** showPid Command **** //     /
void ShowPidCommand::execute()
{
    SmallShell& shell = SmallShell::getInstance();
    std::cout << "smash pid is " << shell.smash_pid << std::endl;
}
//  2.  // **** END OF showPid Command **** //     //


//  3.  // **** PWD Command **** //     //
void GetCurrDirCommand::execute()
{
    char buffer[PATH_MAX];
    if(getcwd(buffer, PATH_MAX) != NULL) 
    {
        std::cout << buffer << endl;
        return;
    }
    perror("smash error: getcwd failed");
}
//  3.  // **** END OF PWD Command **** //     //


//  4.  // **** CD Command **** //     //
void ChangeDirCommand::changeDir(const char* to_change_to)
{
    SmallShell* shell = &(SmallShell::getInstance());
    char buffer[PATH_MAX];
    if(getcwd(buffer, PATH_MAX) == NULL) 
    {
        perror("smash error: getcwd failed");
        return;
    }
    if(chdir(to_change_to) != 0) // chdir returns 0 on success , -1 otherwise
        perror("smash error: chdir failed");
    else
        shell->setLastDir(buffer); // changing last dir to the one that we were in
}

ChangeDirCommand::ChangeDirCommand(const char* cmd_line,  string* plastPwd) : BuiltInCommand(cmd_line), pointer_to_last_dir(plastPwd), num_of_args(0)
{
     char* words[COMMAND_MAX_ARGS + 1];
     num_of_args = _parseCommandLine(cmd_line,words);
     if(num_of_args != 1)
         given_argument = words[1];
    
    freeAllocatedStrings(words, num_of_args);
}

void ChangeDirCommand::execute()
{
  if(num_of_args == 1)
      cerr << "smash error: > " << "\"" << cmd_line<< "\"" << endl; // TODO: complete
  else if( num_of_args > 2 )
      cerr << "smash error: cd: too many arguments" << endl;
  else if (given_argument == "-")
  {
      if((*pointer_to_last_dir) == "")
          cerr << "smash error: cd: OLDPWD not set" << endl;
      else
          changeDir((pointer_to_last_dir)->c_str());
  }
  else
      changeDir(given_argument.c_str());
}
//  4.  // **** END of CD Command **** //     //


// 5.   // **** Jobs Command **** //        //
void JobsCommand::execute()
{
    jobs->removeFinishedJobs();
    jobs->printJobsList();
}
// 5.   // **** End of Jobs Command **** //        //


// 6.   // **** FG Command **** //        //
ForegroundCommand::ForegroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line, jobs), error_flag(false) {
    
    char* words[COMMAND_MAX_ARGS + 1];
    int num_of_args = _parseCommandLine(cmd_line, words);
    
    if(num_of_args == 1)
        job_id = 0;
    else if(num_of_args == 2 )
    {
        string str = words[1];
        try{
            job_id = std::stoi(str);
        } catch (...)
        {
            cerr << "smash error: fg: invalid arguments" << endl;
            error_flag = true;
        }
    }
    else if(num_of_args > 2 )
    {
        cerr << "smash error: fg: invalid arguments" << endl;
        error_flag = true;
    }
    
    freeAllocatedStrings(words, num_of_args);
}

void ForegroundCommand::execute()
{
    JobsList::JobEntry* job = nullptr;
    if(error_flag)
        return;
    else if(job_id == 0)
        job = jobs->getLastJob(&job_id);
    else
        job = jobs->getJobById(job_id);
    
    if( !job && !job_id )
    {
        cerr << "smash error: fg: jobs list is empty" << endl;
        return;
    }
    else if ( !job )
    {
        cerr << "smash error: fg: job-id "<< job_id << " does not exist" << endl;
        return;
    }
    
    
    cout << job->cmd_line << " : " << job->process_id << endl;
    // sigcont
    if(job->status == JobsList::STOPPED && kill(job->process_id, SIGCONT) == -1)
    {
        perror("smash error: kill failed");
        return;
    }
    job->status = JobsList::FG;
    
    SmallShell& shell = SmallShell::getInstance();
    shell.curr_cmd = job->cmd_line;
    shell.running_pid = job->process_id;
    int status;
    // jobs->removeJobById(job_id);
    if(waitpid(job->process_id, &status, WUNTRACED)  == -1)
    {
		if(WIFSIGNALED(status) != true)
			perror("smash error: waitpid failed");
        return;
    }
    
    shell.running_pid = -1;
    shell.curr_cmd = "";
    
}
// 6.   // **** End of FG Command **** //        //



// 7.   // **** BG Command **** //        //
BackgroundCommand::BackgroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line, jobs), error_flag(false)
{
    char* words[COMMAND_MAX_ARGS + 1];
    int num_of_args = _parseCommandLine(cmd_line, words);
    if(num_of_args == 1)
        job_id = 0;
    else if(num_of_args == 2)
    {
        string s = words[1];
        try{
            job_id = std::stoi(s);
        } catch (...)
        {
            cerr << "smash error: bg: invalid arguments" << endl;
            error_flag = true;
        }
    }
    else if(num_of_args > 2 )
    {
        cerr << "smash error: bg: invalid arguments" << endl;
        error_flag = true;
    }
    
    freeAllocatedStrings(words, num_of_args);
}

void BackgroundCommand::execute()
{
    JobsList::JobEntry* job = nullptr;

    if(error_flag)
        return;
    else if(job_id == 0)
    {
        job = jobs->getLastStoppedJob(&job_id);
        if(job == nullptr)
        {
            cerr << "smash error: bg: there is no stopped jobs to resume" << endl;
            return;
        }
    }
    else
    {
        job = jobs->getJobById(job_id);
        if(job == nullptr)
        {
            cerr << "smash error: bg: job-id "<< job_id << " does not exist" << endl;
            return;
        }
        if(job->status == JobsList::BG)
        {
            cerr << "smash error: bg: job-id "<< job_id << " is already running in the background" << endl;
            return;
        }
    }
    
    cout << job->cmd_line << " : " << job->process_id << endl;
    
    if(kill(job->process_id, SIGCONT) == -1)
    {
        perror("smash error: kill failed");
        return;
    }
    job->status = JobsList::BG;


}
// 7.   // **** End of BG Command **** //        //


// 8.   // **** Quit Command **** //        //
QuitCommand::QuitCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line, jobs), print_jobs(false)
{
    char* words[COMMAND_MAX_ARGS + 1];
    int num_of_args = _parseCommandLine(cmd_line, words);
    if(num_of_args >= 2 && string(words[1]) == "kill")
    {
        print_jobs = true;
    }
    freeAllocatedStrings(words, num_of_args);
}

void QuitCommand::execute()
{
    // SmallShell& shell = SmallShell::getInstance();
    std::list<JobsList::JobEntry*>& jobs = (BuiltInCommand::jobs)->job_list;
    (BuiltInCommand::jobs)->removeFinishedJobs();
    int jobs_num = (int) jobs.size();
    if(print_jobs)
    {
        cout << "smash: sending SIGKILL signal to " << jobs_num << " jobs:" << endl;
        for(list<JobsList::JobEntry*>::iterator it = jobs.begin(); it != jobs.end(); it++)
        {
            cout << (*it)->process_id << ": " << (*it)->cmd_line << endl;
            kill((*it)->process_id, SIGKILL);
        }
    }
    exit(0);
}
// 8.   // **** End of Quit Command **** //        //


// 9.   // **** Kill Command **** //        //
KillCommand::KillCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line, jobs), error_flag(false)
{
    char* words[COMMAND_MAX_ARGS + 1];
    int num_of_args = _parseCommandLine(cmd_line, words);
    if(num_of_args != 3 || words[1][0] != '-')
    {
        cerr << "smash error: kill: invalid arguments" << endl;
        error_flag = true;
        return;
    }
    
    try
    {
        signum = std::stoi(string(words[1]+1));
        jobId = std::stoi(string(words[2]));
        if(signum < 1 || signum > 31)
        {
            cerr << "smash error: kill: invalid arguments" << endl;
            error_flag = true;
            return;
        }
    } catch (...)
    {
        cerr << "smash error: kill: invalid arguments" << endl;
        error_flag = true;
        return;
    }
    if(jobs->getJobById(jobId) == nullptr)
    {
        cerr << "smash error: kill: job-id " << jobId << " does not exist" << endl;
        error_flag = true;
        return;
    }
}

void KillCommand::execute()
{
    if(error_flag)
        return;
    JobsList::JobEntry* job = jobs->getJobById(jobId);
    if(job == nullptr)
	{
        cerr << "smash error: kill: job-id " << jobId << " does not exist" << endl;
        error_flag = true;
        return;
    }
    if(kill(job->process_id, signum) == -1)
    {
        perror("smash error: kill failed");
        return;
    }
    if(signum == SIGCONT)
    {
        if(job->status == JobsList::STOPPED)
            job->status = JobsList::BG;
    }
    cout << "signal number " << signum << " was sent to pid " << job->process_id << endl;
    
}
// 9.   // **** End of Kill Command **** //        //


// ***************************************** //
// *                                       * //
// *                                       * //
// ******* External Commands Classes ******* //
// *                                       * //
// *                                       * //
// ***************************************** //


// TODO: ASK MOHAMMAD
void ExternalCommand::execSimpleCommand(const char* cmd_line)
{
    bool isBG = _isBackgroundComamnd(cmd_line);
    string cmd = _removeBackgroundSign(cmd_line);
    
    char* words[COMMAND_MAX_ARGS + 1];
    int num_of_args = _parseCommandLine(cmd.c_str(), words);
    
    SmallShell& shell = SmallShell::getInstance();
        
    pid_t pid = fork();

    if(pid == -1 )
    {
        perror("smash error: fork failed");
    }
    else if (pid == 0)
    {
        setpgrp();
        // char* argv[] = {words[0], words};
        int result = execvp(words[0], words);
        if(result == -1)
        {
            perror("smash error: execv failed");
            exit(0); // TODO: WHY ???
        }
        exit(0);
    }
    else // father
    {
        if(shell.is_timeout_cmd)
        {
            for(auto& it : shell.TimesSet)
            {
                if( it.pid == -1)
                {
                    (it).pid = pid;
                    break;
                }
            }
        }
        
        if(isBG)
        {
            shell.jobs.removeFinishedJobs();
            shell.jobs.addJob(this, pid, JobsList::BG);
        }
        else
        {
            shell.curr_cmd = cmd_line;
            shell.running_pid = pid;
            
            if(waitpid(pid, NULL, WUNTRACED) == -1)
                perror("smash error: waitpid failed"); // TODO: WAIT / WAITPID
            
            shell.curr_cmd = "";
            shell.running_pid = -1;
        }
    }
    freeAllocatedStrings(words, num_of_args);
}

void ExternalCommand::execComplexCommand(const char* cmd_line)
{
    bool isBG = _isBackgroundComamnd(cmd_line);
    string cmd = _removeBackgroundSign(cmd_line);
    
    char* words[COMMAND_MAX_ARGS + 1];
    int num_of_args = _parseCommandLine(cmd.c_str(), words);
    
    SmallShell& shell = SmallShell::getInstance();
        
    pid_t pid = fork();
    char* path = (char*)("/bin/bash");
    
    if(pid == -1 )
    {
        perror("smash error: fork failed");
    }
    else if (pid == 0)
    {
        setpgrp();
        char* argv[] = {path, (char*) "-c", (char*)cmd.c_str(), NULL};
        int result = execv(path, argv);
        if(result == -1)
        {
            perror("smash error: execv failed");
            exit(0); // TODO: WHY ???
        }
        exit(0);
    }
    else // father
    {
        if(shell.is_timeout_cmd)
        {
            for(auto& it : shell.TimesSet)
            {
                if( it.pid == -1)
                {
                    (it).pid = pid;
                    break;
                }
            }
        }
        if(isBG)
        {
            shell.jobs.removeFinishedJobs();
            shell.jobs.addJob(this, pid, JobsList::BG);
        }
        else
        {
            shell.curr_cmd = cmd_line;
            shell.running_pid = pid;
            
            if(waitpid(pid, NULL, WUNTRACED) == -1)
                perror("smash error: waitpid failed");
            
            shell.curr_cmd = "";
            shell.running_pid = -1;
        }
    }
    freeAllocatedStrings(words, num_of_args);
}

void ExternalCommand::execute()
{
    string cmd(cmd_line);
    SmallShell& shell = SmallShell::getInstance();
    if(shell.is_timeout_cmd)
    {
        char* words[COMMAND_MAX_ARGS + 1];
        int num_of_args = _parseCommandLine(cmd_line, words);
        string time = string(words[1]);
        size_t time_pos = string(cmd_line).find(time);
        string line = string(cmd_line).substr(time_pos + time.size());
        cmd = _trim(line);
        freeAllocatedStrings(words, num_of_args);
    }
    if( cmd.find("*") == string::npos && cmd.find("?") == string::npos) // simple command
        execSimpleCommand(cmd.c_str());
    else
        execComplexCommand(cmd.c_str());
}







// ***************************************** //
// *                                       * //
// *                                       * //
// ******** Special Commands Classes ******* //
// *                                       * //
// *                                       * //
// ***************************************** //


// 1.   // **** Redirection Command **** //        //
void RedirectionCommand::prepare(int status)
{
    // TODO: CHECK &
    stdout_fd = dup(1);
    if(stdout_fd == -1)
    {
        perror("smash error: dup failed");
        error_flag = true;
        return;
    }
    if( (fd = open(file_name.c_str(), O_CREAT|status|O_RDWR, 0655)) == -1)
    {
        perror("smash error: open failed");
        error_flag = true;
        return;
    }
    if( dup2(fd, 1) == -1)
    {
        perror("smash error: dup2 failed");
        error_flag = true;
        return;
    }
}

void RedirectionCommand::cleanup()
{
    if(close(fd) == -1)
    {
        perror("smash error: close failed");
        error_flag = true;
        return;
    }
    if(dup2(stdout_fd, 1) == -1)
    {
        perror("smash error: dup2 failed");
        error_flag = true;
        return;
    }
    if(close(stdout_fd) == -1)
    {
        perror("smash error: close failed");
        error_flag = true;
        return;
    }
}

void RedirectionCommand::execute()
{
    string cmd_line(Command::cmd_line);
    size_t index = cmd_line.find(">");
    cmd = cmd_line.substr(0, index);
    cmd = _removeBackgroundSign(cmd.c_str());
    
    if( cmd_line[index+1] == '>' )
    {
        string temp = _trim(cmd_line.substr(index+2));
        file_name = _removeBackgroundSign(temp.c_str());
        prepare(O_APPEND);
    }
    else
    {
        string temp = _trim(cmd_line.substr(index+1));
        file_name = _removeBackgroundSign(temp.c_str());
        prepare(O_TRUNC);
    }
    if(error_flag == true)
		return;
    SmallShell& shell = SmallShell::getInstance();
    Command* new_cmd = shell.CreateCommand(cmd.c_str());
    new_cmd->execute();
    cleanup();
    delete new_cmd;
}

// 1.   // **** End of Redirection Command **** //        //


// 2.   // **** Pipe Command **** //        //
void PipeCommand::prepare()
{
    string cmd_line(Command::cmd_line);
    size_t index = cmd_line.find("|");
    cmd1 = _removeBackgroundSign(cmd_line.substr(0, index).c_str());
    
    if( cmd_line[index+1] == '&' )
    {
        string temp = cmd_line.substr(index+2);
        cmd2 = _removeBackgroundSign(temp.c_str());
        status = 2;
    }
    else
    {
        string temp = cmd_line.substr(index+1);
        cmd2 = _removeBackgroundSign(temp.c_str());
        status = 1;
    }
}


void PipeCommand::FdCloseEntries(int* fd, Status pipe_status, int fd_id)
{
    if( dup2(fd[pipe_status], fd_id) == -1)
    {
        perror("smash error: dup2 failed");
        error_flag = true;
        return;
    }
    if(close(fd[WRITE]) == -1)
    {
        perror("smash error: close failed");
        error_flag = true;
        return;
    }
    if(close(fd[READ]) == -1)
    {
        perror("smash error: close failed");
        error_flag = true;
        return;
    }
}

void PipeCommand::execute()
{
    
    SmallShell& shell = SmallShell::getInstance();
    prepare();

	
    pid_t Ppid = fork();
    if(Ppid == -1)
    {
        perror("smash error: fork failed");
        return; 
    }
    else if(Ppid == 0) // son = pipe
    {
		int fd[2];
		if(pipe(fd) == -1)
		{
			perror("smash error: pipe failed");
			return;
		}
		
        setpgrp();
        pid_t left_pid = fork();
        if(left_pid == -1)
        {
            perror("smash error: fork failed");
            return;
        }
        else if(left_pid > 0) // father, which is right
        {
            if ( waitpid(left_pid,nullptr, WUNTRACED) == -1)
            {
                perror("smash error: waitpid failed");
                exit(0);
            }
            FdCloseEntries(fd, READ, 0);
            if(error_flag)
				exit(0);
            Command* new_cmd = shell.CreateCommand(cmd2.c_str());
            new_cmd->execute();
            delete new_cmd;
            exit(0);
        }
        else if(left_pid == 0)
        {
            setpgrp();
            FdCloseEntries(fd, WRITE, status);
            if(error_flag)
				exit(0);
            Command* new_cmd = shell.CreateCommand(cmd1.c_str());
            new_cmd->execute();
            delete new_cmd;
            exit(0);
        }
    }
    else if(Ppid > 0) // smash
    {
        if(waitpid(Ppid, NULL, WUNTRACED) == -1)
        {
            perror("smash error: waitpid failed");
            return;
        }
    }
}
// 2.   // **** End of Pipe Command **** //        //

// Fare:

FareCommand::FareCommand(const char* cmd_line) : BuiltInCommand(cmd_line),error(false)
{
    char* words[COMMAND_MAX_ARGS + 1];
    int num_of_args = _parseCommandLine(cmd_line,words);
    if(num_of_args != 4)
    {
        cerr << "smash error: fare: invalid arguments" << endl;
        error=true;
        return;
    }
    file_name=words[1];
    to_change=words[2];
    to_change_to=words[3];
    freeAllocatedStrings(words, num_of_args);
    file_fd=open(file_name.c_str(),O_RDONLY);
    if(file_fd == -1)
    {
        perror("smash error: open failed");
        error=true;
        return;
    }

}

void FareCommand::execute()
{
   if(error)
		return;
   vector<string> vector,old_vector;
   number_of_times=0;
   bool did_change=false,to_normal_state=false;
   char buffer='\0';  
   string buff="";
   string tmp_buff="";
   string name=file_name;
   size_t index;
   while (read(file_fd, &buffer, 1) != 0)
   {
       buff+=buffer;
       while ((index = buff.find(to_change)) != string::npos)
       {
           number_of_times++;
           old_vector.push_back(buff);
           buff.erase(index, to_change.length());
           buff.insert(index, to_change_to);
           tmp_buff += buff.substr(0, index + to_change_to.length());
           buff.erase(0, index + to_change_to.length());
           did_change = true;
       }
       if (did_change)
       {
           buff.insert(0, tmp_buff);
           vector.push_back(buff);
           buff="";
           tmp_buff="";
           did_change=false;
       }
   }
   if (buff != "")
   {
        vector.push_back(buff);
        old_vector.push_back(buff);
   }
    
   if( close(file_fd) == -1)
   {
       perror("smash error: close failed");
       //close(tmp_file_fd);
       return;
   }
   file_fd=open(name.c_str(),O_WRONLY | O_TRUNC);
   if( file_fd == -1 )
   {
       perror("smash error: open failed");
       //close(tmp_file_fd);
       return;
   }
   for (string it : vector)
   {
       char *to_put = new char[it.size()];
       strcpy(to_put, it.c_str());
       if (write(file_fd, to_put, it.size()) == -1)
       {
           perror("smash error: write failed");
           close(file_fd);
           delete[] to_put;
           to_normal_state = true;
           
       }
       delete[] to_put;
   }
   if( close(file_fd) == -1)
   {
       perror("smash error: close failed");
       return;
   }
   if (to_normal_state)
   {
       file_fd = open(name.c_str(), O_WRONLY | O_TRUNC);
       if (file_fd == -1)
       {
           perror("smash error: open failed");
           // close(tmp_file_fd);
           return;
       }
       for (string it : old_vector)
       {
           char *to_put = new char[it.size()];
           strcpy(to_put, it.c_str());
           if (write(file_fd, to_put, it.size()) == -1)
           {
               perror("smash error: write failed");
               close(file_fd);
               delete[] to_put;
               return;
           }
           delete[] to_put;
       }
       if (close(file_fd) == -1)
       {
           perror("smash error: close failed");
           return;
       }
   }
   cout << "replaced " << number_of_times<<" instances of the string \"" << to_change <<"\"" << endl;
}



bool Timeout::operator>(const Timeout& cmd1) const {
    return (duration + timestamps ) > (cmd1.duration + cmd1.timestamps );
}

bool Timeout::operator<(const Timeout& cmd1) const{
    return (duration + timestamps ) < (cmd1.duration + cmd1.timestamps );
}

bool Timeout::operator==(const Timeout& cmd1) const{
    return (duration + timestamps ) == (cmd1.duration + cmd1.timestamps );
}

TimeoutCommand::TimeoutCommand(const char* cmd_line) : BuiltInCommand(cmd_line), error_flag(false)
{

}

void TimeoutCommand::prepare()
{
    char* words[COMMAND_MAX_ARGS + 1];
    int num_of_args = _parseCommandLine(cmd_line, words);
    if(num_of_args < 3)
    {
        cerr << "smash error: timeout: invalid arguments" << endl;
        error_flag = true;
        return;
    }
    string duration1 = string(words[1]);
    int pos = (int) string(cmd_line).find(duration1);
    string cmd_l = string(cmd_line).substr(pos + duration1.size());
    to_exec = _trim(cmd_l);
    int duration = -1;
    try{
        duration = stoi(duration1);
    } catch(...)
    {
        error_flag = true;
        return;
    }
    if(duration < 0)
    {
        cerr << "smash error: timeout: invalid arguments" << endl;
        error_flag = true;
        return;
    }
    
    Timeout timeout_cmd(cmd_line, duration, time(nullptr), -1);
    SmallShell& smash = SmallShell::getInstance();
    smash.TimesSet.push_back(timeout_cmd);
    smash.TimesSet.sort();
}

void TimeoutCommand::execute()
{
    prepare();
    if(error_flag)
        return;
    
    SmallShell& smash = SmallShell::getInstance();
    list<Timeout>& set = (smash.TimesSet);
    smash.is_timeout_cmd = true;
    Command* cmd = smash.CreateCommand(cmd_line);
    time_t alarm_time = (set.begin()->duration + set.begin()->timestamps) - time(nullptr);
    alarm((unsigned int) alarm_time);
    cmd->execute();
    smash.is_timeout_cmd = false;
    delete cmd;
}

