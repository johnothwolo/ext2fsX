/*
* Copyright 2003 Brian Bergstrand.
*
* Redistribution and use in source and binary forms, with or without modification, 
* are permitted provided that the following conditions are met:
*
* 1.	Redistributions of source code must retain the above copyright notice, this list of
*     conditions and the following disclaimer.
* 2.	Redistributions in binary form must reproduce the above copyright notice, this list of
*     conditions and the following disclaimer in the documentation and/or other materials provided
*     with the distribution.
* 3.	The name of the author may not be used to endorse or promote products derived from this
*     software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
* AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
* THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/

static const char spawn_whatid[] __attribute__ ((unused)) =
"@(#) $Id: spawn.c,v 1.1 2003/12/21 17:32:16 bbergstrand Exp $";

static void safe_execv(char *args[])
{
	int		pid;
	union wait	status;

	pid = vfork();
	if (pid == 0) {
		(void)execv(args[0], args);
		fprintf(stderr, "%s: execv %s failed, %s\n", progname, args[0],
			strerror(errno));
		_exit(1);
	}
	if (pid == -1) {
		fprintf(stderr, "%s: fork failed, %s\n", progname,
			strerror(errno));
		exit(1);
	}
	if (wait4(pid, (int *)&status, 0, NULL) != pid) {
		fprintf(stderr, "%s: BUG executing %s command\n", progname,
			args[0]);
		exit(1);
	} else if (!WIFEXITED(status)) {
		fprintf(stderr, "%s: %s command aborted by signal %d\n",
			progname, args[0], WTERMSIG(status));
		exit(1);
	} else if (WEXITSTATUS(status)) {
   #ifndef EXT_SPAWN_SUPRESS_FAILURES
		fprintf(stderr, "%s: %s command failed, exit status %d: %s\n",
			progname, args[0], WEXITSTATUS(status),
			strerror(WEXITSTATUS(status)));
		exit(1);
   #else
      exit(WEXITSTATUS(status));
   #endif
	}
}
