#ifndef NONCANMODE_H
#define NONCANMODE_H 

#include <termios.h>

void ResetCanonicalMode(int fd, struct termios *savedattributes);
void SetNonCanonicalMode(int fd, struct termios *savedattributes);

#endif
