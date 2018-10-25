#ifndef MAKEDIR_H
#define MAKEDIR_H

/* makeDir(): wrapper for Windows' CreateDirectory() function.
** Why not use CreateDirectory() directly?
** Simply because windows.h defines some data types that might conflict with user-defined data types
** (WORD and DWORD in my case), also to keep some sort of abstraction from the used platform and
** perform error-handling without bloating the program's flow.
*/
void makeDir(const char *path);
#endif /* MAKEDIR_H */

