#include <stdio.h>
#include <syslog.h>
#include <errno.h>

int main(int argc, char **argv)
{
    openlog(NULL, 0, LOG_USER);

    // Validate arguments
    if (argc != 3)
    {
        syslog(LOG_ERR, "Please specify all the parameters:");
        syslog(LOG_ERR, "./writer [writefile] [writestr]");
        syslog(LOG_ERR, "[writefile]: file full path + filename");
        syslog(LOG_ERR, "[writestr]: string to write inside writefile");
        return 1;
    }

    const char *writefile = argv[1];
    const char *writestr = argv[2];

    FILE *file = fopen(writefile, "w");

    if (file == NULL)
    {
        const int file_open_err = errno; // Since errno is a singleton and
                                         // changes values on every new error...
        syslog(LOG_ERR, "ERROR %d: could not open file %s !", file_open_err, argv[1]);
        return file_open_err;
    }

    syslog(LOG_DEBUG, "Writing %s to %ss", writestr, writefile);

    const int write_result = fprintf(file, "%s", writestr);
    if (write_result < 0)
    {
        const int file_write_err = errno; // Since errno is a singleton and
                                          // changes values on every new error...
        syslog(LOG_ERR, "ERROR %d: could not write to file %s !", file_write_err, argv[1]);
        return file_write_err;
    }

    // Closing file
    fclose(file);

    return 0;
}