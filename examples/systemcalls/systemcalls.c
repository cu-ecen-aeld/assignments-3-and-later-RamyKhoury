#include "systemcalls.h"
#include <stdlib.h>
#include <syslog.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
 */
bool do_system(const char *cmd)
{

    /*
     * TODO  add your code here
     *  Call the system() function with the command set in the cmd
     *   and return a boolean true if the system() call completed with success
     *   or false() if it returned a failure
     */
    int ret = system(cmd);

    if (ret == -1)
    {
        openlog(NULL, 0, LOG_USER);
        const int err = errno;
        syslog(LOG_ERR, "ERROR: Could not run command %s\nReason: %d", cmd, err);
        closelog();
        return false;
    }
    else
        return true;
}

/**
 * @param count -The numbers of variables passed to the function. The variables are command to execute.
 *   followed by arguments to pass to the command
 *   Since exec() does not perform path expansion, the command to execute needs
 *   to be an absolute path.
 * @param ... - A list of 1 or more arguments after the @param count argument.
 *   The first is always the full path to the command to execute with execv()
 *   The remaining arguments are a list of arguments to pass to the command in execv()
 * @return true if the command @param ... with arguments @param arguments were executed successfully
 *   using the execv() call, false if an error occurred, either in invocation of the
 *   fork, waitpid, or execv() command, or if a non-zero return value was returned
 *   by the command issued in @param arguments with the specified arguments.
 */

bool do_exec(int count, ...)
{
    openlog(NULL, 0, LOG_USER);
    va_list args;
    va_start(args, count);
    char *command[count + 1];
    int i;
    for (i = 0; i < count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];
    /*
     * TODO:
     *   Execute a system command by calling fork, execv(),
     *   and wait instead of system (see LSP page 161).
     *   Use the command[0] as the full path to the command to execute
     *   (first argument to execv), and use the remaining arguments
     *   as second argument to the execv() command.
     *
     */
    va_end(args);

    pid_t child_p = fork();
    switch (child_p)
    {
    case -1: // Error while forking the process
    {
        const int err = errno;
        syslog(LOG_ERR, "ERROR: Could not create child process.\nReason: %d", err);
        break;
    }
    case 0:
    {
        execv(command[0], command);

        // only runs if exec fails
        const int err = errno;
        syslog(LOG_ERR, "ERROR: Could not start child process.\nReason: %d", err);
        closelog();
        exit(1);
    }
    default:
    {
        int status;
        waitpid(child_p, &status, 0);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
            syslog(LOG_ERR, "ERROR: child terminated abnormally\n");
        else
        {
            closelog();
            return true;
        }
    }
    }

    closelog();
    return false;
}

/**
 * @param outputfile - The full path to the file to write with command output.
 *   This file will be closed at completion of the function call.
 * All other parameters, see do_exec above
 */
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    openlog(NULL, 0, LOG_USER);
    va_list args;
    va_start(args, count);
    char *command[count + 1];
    int i;
    for (i = 0; i < count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];

    /*
     * TODO
     *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
     *   redirect standard out to a file specified by outputfile.
     *   The rest of the behaviour is same as do_exec()
     *
     */

    va_end(args);

    pid_t child_p = fork();
    switch (child_p)
    {
    case -1: // Error while forking the process
    {
        const int err = errno;
        syslog(LOG_ERR, "ERROR: Could not create child process.\nReason: %d", err);
        break;
    }
    case 0:
    {
        // Redirect output
        int fd = open(outputfile, O_WRONLY | O_TRUNC | O_CREAT, 0644);
        if (fd < 0)
        {
            const int err = errno;
            syslog(LOG_ERR, "ERROR: Could not open redirection file %s.\nReason: %d", outputfile, err);
            break;
        }
        if (dup2(fd, STDOUT_FILENO) < 0)
        {
            const int err = errno;
            syslog(LOG_ERR, "ERROR: Could not duplicate redirection file %s.\nReason: %d", outputfile, err);
            break;
        }
        close(fd);

        execv(command[0], command);
        // only runs if exec fails
        const int err = errno;
        syslog(LOG_ERR, "ERROR: Could not start child process.\nReason: %d", err);
        closelog();
        exit(1);
    }
    default:
    {
        int status = 0;
        waitpid(child_p, &status, 0);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
            syslog(LOG_ERR, "ERROR: child terminated abnormally\n");
        else
        {
            closelog();
            return true;
        }
    }
    }

    closelog();
    return false;
}
