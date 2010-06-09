#include "getmimetype.h"

const char*
get_mimetype(const char* filename_)
{
    size_t len = strlen(filename_);

    if(filename_[len-1] == 'l')
        return "text/html";
    if(filename_[len-2] == 'j')
        return "text/javascript";
    if(filename_[len-1] == 'g')
        return "image/png";
    else
        return "text/css";

    char* mimetype = NULL;
    char* filename = NULL;
   
    size_t filename_length = strlen(filename_) + strlen(FILE_CMD) + 1; 
    filename = malloc(filename_length);
    if(filename == NULL)
        return NULL;
    strcpy(filename, FILE_CMD);
    filename += strlen(FILE_CMD);
    strcpy(filename, filename_);
    filename -= strlen(FILE_CMD);
    filename[filename_length] = '\0';
    FILE* file_output = popen(filename, "r");
    if(!file_output)
        return NULL;
    char buf[100] = "";
    size_t bytes_read = fread(buf, sizeof(buf)-1, 1, file_output);
    if(buf[0] == '\0')
        /* nothing read */
        return NULL;
    char* bufptr = buf;
    while(1) {
        if(*bufptr == '\n') {
            *(bufptr+1) = '\0';
            break;
        }
        ++bufptr;
    }
    DEBUG("Buff %s", buf);
    char* ignore;
    if(!sscanf(buf, "%s %s\n", ignore, mimetype))
        return NULL;
    fclose(file_output);
    DEBUG("Mimetype: %s", mimetype);
    return mimetype;
}
