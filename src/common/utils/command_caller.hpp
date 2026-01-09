#pragma once

#include <iostream>
#include <cstdio>
#include <array>
#include <string>
#include <thread>
#include <future>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>

#include "common/common.hpp"
#include "common/log.hpp"


class GWUtilCommandCaller {
 public:
    /*!
     *  \brief  execute a specified command and obtain its result (synchronously)
     *  \param  cmd             the command to execute
     *  \param  result          the result of the executed command
     *  \param  ignore_error    whether to ignore command execution error (exit_code != 0)
     *  \param  print_stdout    dynamically printing stdout
     *  \param  print_stderr    dynamically printing stderr
     *  \todo   this function should support timeout option
     *  \return GW_SUCCESS once the command is successfully executed
     *          GW_FAILED if failed
     */
    static inline gw_retval_t exec_sync(
        std::string& cmd,
        std::string& result, 
        bool ignore_error = false,
        bool print_stdout = false,
        bool print_stderr = false
    ){
        gw_retval_t retval = GW_SUCCESS;
        std::array<char, 8192> buffer;
        int exit_code = -1;
        FILE *pipe;

        if(print_stderr){
            print_stdout = true;
            cmd = cmd + std::string(" 2>&1");
        }

        pipe = popen(cmd.c_str(), "r");
        if (unlikely(pipe == nullptr)) {
            GW_WARN("failed to open pipe for executing command %s", cmd.c_str());
            retval = GW_FAILED;
            goto exit;
        }

        result.clear();
        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            result += buffer.data();
            if(print_stdout){ std::cout << buffer.data(); }
        }

        // remove \n and \r
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
            result.pop_back();
        }

        exit_code = WEXITSTATUS(pclose(pipe));
        if(unlikely(exit_code != 0) && ignore_error == false){
            GW_WARN("failed execution of command %s: exit_code(%d)", cmd.c_str(), exit_code);
            retval = GW_FAILED;
            goto exit;
        }

    exit:
        return retval;
    }


    static inline gw_retval_t exec_sync(
        std::vector<std::string>& cmd_vec,
        std::string& result, 
        bool ignore_error = false,
        bool print_stdout = false,
        bool print_stderr = false
    ){
        std::ostringstream oss;
        std::string cmd;
        uint64_t i;
        for(i=0; i<cmd_vec.size(); i++) {
            if (i > 0) oss << " ";
            oss << cmd_vec[i];
        }
        cmd = oss.str();
        return GWUtilCommandCaller::exec_sync(cmd, result, ignore_error, print_stdout, print_stderr);
    }


    /*!
     *  \brief  execute a specified command and obtain its result (asynchronously)
     *  \param  cmd             the command to execute
     *  \param  async_thread    thread handle of the async command execution
     *  \param  thread_promise  return value of the async thread
     *  \param  pipe            pipe handle of the async command execution  
     *  \param  pid             pid of the async command execution
     *  \param  result          the result of the executed command
     *  \param  ignore_error    whether to ignore command execution error (exit_code != 0)
     *  \param  print_stdout    dynamically printing stdout
     *  \param  print_stderr    dynamically printing stderr
     *  \todo   this function should support timeout option
     *  \return GW_SUCCESS once the command is successfully executed
     *          GW_FAILED if failed
     */
    static inline gw_retval_t exec_async(
        std::string& cmd,
        std::thread& async_thread,
        std::promise<gw_retval_t>& thread_promise,
        FILE **pipe,
        pid_t &pid,
        std::string& result,
        bool ignore_error = false,
        bool print_stdout = false,
        bool print_stderr = false
    ){
        gw_retval_t retval = GW_SUCCESS;

        auto __exec_async = [&](FILE **_pipe){
            std::array<char, 8192> buffer;
            int exit_code = -1;

            result.clear();
            while (fgets(buffer.data(), buffer.size(), *_pipe) != nullptr) {
                result += buffer.data();
                if(print_stdout){ std::cout << buffer.data(); }
            }

            // remove \n and \r
            while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
                result.pop_back();
            }

            exit_code = WEXITSTATUS(pclose(*_pipe));
            *_pipe = nullptr;
            if(unlikely(exit_code != 0) && ignore_error == false){
                // GW_WARN("failed execution of command %s: exit_code(%d)", cmd.c_str(), exit_code);
                thread_promise.set_value(GW_FAILED);
            } else {
                thread_promise.set_value(GW_SUCCESS);
            }
        };

        GW_CHECK_POINTER(pipe);

        if(print_stderr){
            print_stdout = true;
            cmd = cmd + std::string(" 2>&1");
        }

        *pipe = GWUtilCommandCaller::popen2(cmd, "r", pid);
        if (unlikely(*pipe == nullptr)) {
            GW_WARN("failed to open pipe for executing command %s", cmd.c_str());
            retval = GW_FAILED;
            goto exit;
        }

        async_thread = std::thread(__exec_async, pipe);
        async_thread.detach();

    exit:
        return retval;
    }


    static inline gw_retval_t exec_async(
        std::vector<std::string>& cmd_vec,
        std::thread& async_thread,
        std::promise<gw_retval_t>& thread_promise,
        FILE **pipe,
        pid_t &pid,
        std::string& result,
        bool ignore_error = false,
        bool print_stdout = false,
        bool print_stderr = false
    ){
        std::ostringstream oss;
        std::string cmd;
        uint64_t i;
        for(i=0; i<cmd_vec.size(); i++) {
            if (i > 0) oss << " ";
            oss << cmd_vec[i];
        }
        cmd = oss.str();
        return GWUtilCommandCaller::exec_async(
            cmd, async_thread, thread_promise, pipe, pid, result, ignore_error, print_stdout, print_stderr
        );
    }


    #define READ   0
    #define WRITE  1

    static FILE* popen2(std::string command, std::string type, int & pid){
        pid_t child_pid;
        int fd[2];
        pipe(fd);

        if((child_pid = fork()) == -1)
        {
            perror("fork");
            exit(1);
        }

        /* child process */
        if (child_pid == 0)
        {
            if (type == "r")
            {
                close(fd[READ]);    //Close the READ end of the pipe since the child's fd is write-only
                dup2(fd[WRITE], 1); //Redirect stdout to pipe
            }
            else
            {
                close(fd[WRITE]);    //Close the WRITE end of the pipe since the child's fd is read-only
                dup2(fd[READ], 0);   //Redirect stdin to pipe
            }

            setpgid(child_pid, child_pid); //Needed so negative PIDs can kill children of /bin/sh
            execl("/bin/sh", "/bin/sh", "-c", command.c_str(), NULL);
            _exit(0);
        }
        else
        {
            if (type == "r")
            {
                close(fd[WRITE]); //Close the WRITE end of the pipe since parent's fd is read-only
            }
            else
            {
                close(fd[READ]); //Close the READ end of the pipe since parent's fd is write-only
            }
        }

        pid = child_pid;

        if (type == "r")
        {
            return fdopen(fd[READ], "r");
        }

        return fdopen(fd[WRITE], "w");
    }

    static int pclose2(FILE * fp, pid_t pid, bool wait = false){
        int stat = 0;

        if(fp != nullptr)
            fclose(fp);

        if(wait){
            while (waitpid(pid, &stat, 0) == -1){
                if (errno != EINTR){
                    stat = -1;
                    break;
                }
            }
        }

        return stat;
    }
};
