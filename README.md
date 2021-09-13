# COP4610 Project 1: Implementing a Shell

Implementing a basic shell that supports input/output, redirection, and piping.

## Team Members

- Tomas Munoz-Moxey
- Andrew Perez-Napan
- John Washer

## Division of Labor

### Core Features
- Part 1: Parsing - **All**
- Part 2: Environmental Variables - **John**
- Part 3: Prompt - **Tomas**
- Part 4: Tilde Expansion - **Andrew**
- Part 5: $PATH Search - **Tomas**
- Part 6: External Command Execution - **John, Tomas**
- Part 7: I/O Redirection - **John**
- Part 8: Piping - **Andrew**

### Git Log
See screenshot [here](https://www.dropbox.com/s/1r5hd6xdnquupv4/git_log.png?dl=0).  
Please note that due to differing experiences with source control, the git log may not accurately reflect the division of labor outlined above. Additionally one final commit by John will not be present, but will contain only the final updates to this file.

## Archive Contents
- `README.md` contains a brief breakdown of the project and relevant information for grading.
- `makefile` creates an executable `shell`.
- `shell.c` contains the source code for the project.

## Compilation and Execution
To compile on `cs.linprog.fsu.edu` type `make`.  
To execute the program type `shell`.
For cleanup type `make clean`.

## Known Bugs and Unfinished Portions

### Bugs
- **[Runtime] External command failure on start:** Typing an external command too quickly upon initial execution of `shell` can result in the external command not being properly executed and what seems like two instances of `shell` running. For example, you must enter the `exit` command twice for the `shell` to terminate. This was introduced after the implementation of the `$PATH` search and external command execution. Tried to resolve by refactoring our `fork()` and the way we handle an `exit` call, but still occurs occasionally.

- **[Runtime] Piping two commands successfully causes early exit:** We think this is likely due to not forking correctly in our pipe implementation. This was introduced after the implementation of piping. We attempted to alter methods by which we fork as well as the way in which pipe commands are fetched and stored, but the behavior still occurs.

- **[Runtime] Piping two commands unsuccessfully causes a stall:** The user must manually exit and restart the `shell`. This was introduced after the implementation of piping. We used the same strategy to try to resolve this issue as for the one above, but the behavior still occurs.

### Unfinished Portions
- Part 8 has been partially completed. Works itermittently for piping two commands, but not for piping 3 commands.
- Part 9 has not been attempted.
- Part 10 has not been attempted.