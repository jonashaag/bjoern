/*
    bj_strcpy:
    like `strcpy`, but updates the `destination` by the number of bytes copied.
    (thus, `destination` is a char pointer pointer / a pointer to a char array.)
*/
static inline void
bj_strcpy(char** destination, const char* source)
{
    destination--;
    while (( *(*destination)++ = *source++ ));
}
