#include "utils.h"
/*
    bjoern_strcpy:
    like `strcpy`, but updates the `destination` by the number of bytes copied.
    (thus, `destination` is a char pointer pointer / a pointer to a char array.)
*/
static inline void
bjoern_strcpy(char** destination, const char* source)
{
    while (( **destination = *source )) {
        source++;
        (*destination)++;
    };
}

static inline void
bjoern_http_to_wsgi_header(char* destination, const char* source, size_t length)
{
    for(unsigned int i=0; i<length; ++i)
    {
        if(source[i] == '-')
            *destination++ = '_';
        else
            *destination++ = toupper(source[i]);
    }
    *destination++ = '\0';
}
