#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"
#include <unistd.h>

using namespace std;

void ctrlZHandler(int sig_num)
{
    // TODO: Add your implementation
    cout << "smash: got ctrl-Z" << endl;
    SmallShell& shell = SmallShell::getInstance();
    int pid = shell.running_pid;
    if(pid == -1)
        return;
    if(kill(pid, SIGSTOP) == -1)
    {
        perror("smash error: kill failed");
        return;
    }
    //  = shell.CreateCommand(shell.curr_cmd.c_str());
    Command* cmd = new JobsCommand(shell.curr_cmd.c_str(), nullptr);
    shell.jobs.addJob(cmd, pid, JobsList::STOPPED);
    delete cmd;
    
    shell.running_pid = -1;
    shell.curr_cmd = "";
    
    cout << "smash: process " << pid << " was stopped" << endl;
}

void ctrlCHandler(int sig_num)
{
  // TODO: Add your implementation
    cout << "smash: got ctrl-C" << endl;
    SmallShell& shell = SmallShell::getInstance();
    int pid = shell.running_pid;
    if(pid == -1)
        return;
    if(kill(pid, SIGKILL) == -1)
    {
        perror("smash error: kill failed");
        return;
    }
    shell.running_pid = -1;
    shell.curr_cmd = "";
    
    cout << "smash: process " << pid << " was killed" << endl;
}

void alarmHandler(int sig_num) {
    SmallShell &smash = SmallShell::getInstance();
    smash.jobs.removeFinishedJobs();
    list<Timeout>& set = (smash.TimesSet);
    cout << "smash: got an alarm" << endl;
    string cmd_line = (set.begin())->cmd_line;
    pid_t pid = (set.begin())->pid;
    if(kill(pid,0) != -1) {
        if(kill(pid, SIGKILL) != 0){
            perror("smash error: kill failed");
            return;
        }
        cout<<"smash: "+cmd_line +" timed out!"<<endl;
    }
    (set.erase(set.begin()));
    if(set.begin() != set.end())
     {
        time_t alarm_time = (set.begin()->duration + set.begin()->timestamps) - time(nullptr);
        alarm(alarm_time);
    }


}
