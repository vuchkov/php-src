/*
   +----------------------------------------------------------------------+
   | Copyright (c) The PHP Group                                          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | https://www.php.net/license/3_01.txt                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Jason Greene <jason@inetgurus.net>                           |
   +----------------------------------------------------------------------+
 */

#define PCNTL_DEBUG 0

#if PCNTL_DEBUG
#define DEBUG_OUT printf("DEBUG: ");printf
#define IF_DEBUG(z) z
#else
#define IF_DEBUG(z)
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_pcntl.h"
#include "pcntl_arginfo.h"
#include "php_signal.h"
#include "php_ticks.h"

#if defined(HAVE_GETPRIORITY) || defined(HAVE_SETPRIORITY) || defined(HAVE_WAIT3)
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif

#include <errno.h>
#ifdef HAVE_UNSHARE
#include <sched.h>
#endif

#ifdef HAVE_FORKX
#include <sys/fork.h>
#endif

#ifndef NSIG
# define NSIG 32
#endif

ZEND_DECLARE_MODULE_GLOBALS(pcntl)
static PHP_GINIT_FUNCTION(pcntl);

zend_module_entry pcntl_module_entry = {
	STANDARD_MODULE_HEADER,
	"pcntl",
	ext_functions,
	PHP_MINIT(pcntl),
	PHP_MSHUTDOWN(pcntl),
	PHP_RINIT(pcntl),
	PHP_RSHUTDOWN(pcntl),
	PHP_MINFO(pcntl),
	PHP_PCNTL_VERSION,
	PHP_MODULE_GLOBALS(pcntl),
	PHP_GINIT(pcntl),
	NULL,
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};

#ifdef COMPILE_DL_PCNTL
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(pcntl)
#endif

static void (*orig_interrupt_function)(zend_execute_data *execute_data);

#ifdef HAVE_STRUCT_SIGINFO_T
static void pcntl_signal_handler(int, siginfo_t*, void*);
static void pcntl_siginfo_to_zval(int, siginfo_t*, zval*);
#else
static void pcntl_signal_handler(int);
#endif
static void pcntl_signal_dispatch(void);
static void pcntl_signal_dispatch_tick_function(int dummy_int, void *dummy_pointer);
static void pcntl_interrupt_function(zend_execute_data *execute_data);

void php_register_signal_constants(INIT_FUNC_ARGS)
{

	/* Wait Constants */
#ifdef WNOHANG
	REGISTER_LONG_CONSTANT("WNOHANG",  (zend_long) WNOHANG, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef WUNTRACED
	REGISTER_LONG_CONSTANT("WUNTRACED",  (zend_long) WUNTRACED, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef HAVE_WCONTINUED
	REGISTER_LONG_CONSTANT("WCONTINUED",  (zend_long) WCONTINUED, CONST_CS | CONST_PERSISTENT);
#endif

	/* Signal Constants */
	REGISTER_LONG_CONSTANT("SIG_IGN",  (zend_long) SIG_IGN, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIG_DFL",  (zend_long) SIG_DFL, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIG_ERR",  (zend_long) SIG_ERR, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGHUP",   (zend_long) SIGHUP,  CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGINT",   (zend_long) SIGINT,  CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGQUIT",  (zend_long) SIGQUIT, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGILL",   (zend_long) SIGILL,  CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGTRAP",  (zend_long) SIGTRAP, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGABRT",  (zend_long) SIGABRT, CONST_CS | CONST_PERSISTENT);
#ifdef SIGIOT
	REGISTER_LONG_CONSTANT("SIGIOT",   (zend_long) SIGIOT,  CONST_CS | CONST_PERSISTENT);
#endif
	REGISTER_LONG_CONSTANT("SIGBUS",   (zend_long) SIGBUS,  CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGFPE",   (zend_long) SIGFPE,  CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGKILL",  (zend_long) SIGKILL, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGUSR1",  (zend_long) SIGUSR1, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGSEGV",  (zend_long) SIGSEGV, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGUSR2",  (zend_long) SIGUSR2, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGPIPE",  (zend_long) SIGPIPE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGALRM",  (zend_long) SIGALRM, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGTERM",  (zend_long) SIGTERM, CONST_CS | CONST_PERSISTENT);
#ifdef SIGSTKFLT
	REGISTER_LONG_CONSTANT("SIGSTKFLT",(zend_long) SIGSTKFLT, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef SIGCLD
	REGISTER_LONG_CONSTANT("SIGCLD",   (zend_long) SIGCLD, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef SIGCHLD
	REGISTER_LONG_CONSTANT("SIGCHLD",  (zend_long) SIGCHLD, CONST_CS | CONST_PERSISTENT);
#endif
	REGISTER_LONG_CONSTANT("SIGCONT",  (zend_long) SIGCONT, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGSTOP",  (zend_long) SIGSTOP, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGTSTP",  (zend_long) SIGTSTP, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGTTIN",  (zend_long) SIGTTIN, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGTTOU",  (zend_long) SIGTTOU, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGURG",   (zend_long) SIGURG , CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGXCPU",  (zend_long) SIGXCPU, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGXFSZ",  (zend_long) SIGXFSZ, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGVTALRM",(zend_long) SIGVTALRM, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGPROF",  (zend_long) SIGPROF, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGWINCH", (zend_long) SIGWINCH, CONST_CS | CONST_PERSISTENT);
#ifdef SIGPOLL
	REGISTER_LONG_CONSTANT("SIGPOLL",  (zend_long) SIGPOLL, CONST_CS | CONST_PERSISTENT);
#endif
	REGISTER_LONG_CONSTANT("SIGIO",    (zend_long) SIGIO, CONST_CS | CONST_PERSISTENT);
#ifdef SIGPWR
	REGISTER_LONG_CONSTANT("SIGPWR",   (zend_long) SIGPWR, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef SIGSYS
	REGISTER_LONG_CONSTANT("SIGSYS",   (zend_long) SIGSYS, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIGBABY",  (zend_long) SIGSYS, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef SIGRTMIN
	REGISTER_LONG_CONSTANT("SIGRTMIN", (zend_long) SIGRTMIN, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef SIGRTMAX
	REGISTER_LONG_CONSTANT("SIGRTMAX", (zend_long) SIGRTMAX, CONST_CS | CONST_PERSISTENT);
#endif

#if defined(HAVE_GETPRIORITY) || defined(HAVE_SETPRIORITY)
	REGISTER_LONG_CONSTANT("PRIO_PGRP", PRIO_PGRP, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("PRIO_USER", PRIO_USER, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("PRIO_PROCESS", PRIO_PROCESS, CONST_CS | CONST_PERSISTENT);
#if defined(PRIO_DARWIN_BG)
	REGISTER_LONG_CONSTANT("PRIO_DARWIN_BG", PRIO_DARWIN_BG, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("PRIO_DARWIN_THREAD", PRIO_DARWIN_THREAD, CONST_CS | CONST_PERSISTENT);
#endif
#endif

	/* {{{ "how" argument for sigprocmask */
#ifdef HAVE_SIGPROCMASK
	REGISTER_LONG_CONSTANT("SIG_BLOCK",   SIG_BLOCK, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIG_UNBLOCK", SIG_UNBLOCK, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SIG_SETMASK", SIG_SETMASK, CONST_CS | CONST_PERSISTENT);
#endif
	/* }}} */

	/* {{{ si_code */
#if defined(HAVE_SIGWAITINFO) && defined(HAVE_SIGTIMEDWAIT)
	REGISTER_LONG_CONSTANT("SI_USER",    SI_USER,    CONST_CS | CONST_PERSISTENT);
#ifdef SI_NOINFO
	REGISTER_LONG_CONSTANT("SI_NOINFO",  SI_NOINFO,  CONST_CS | CONST_PERSISTENT);
#endif
#ifdef SI_KERNEL
	REGISTER_LONG_CONSTANT("SI_KERNEL",  SI_KERNEL,  CONST_CS | CONST_PERSISTENT);
#endif
	REGISTER_LONG_CONSTANT("SI_QUEUE",   SI_QUEUE,   CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SI_TIMER",   SI_TIMER,   CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SI_MESGQ",   SI_MESGQ,   CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SI_ASYNCIO", SI_ASYNCIO, CONST_CS | CONST_PERSISTENT);
#ifdef SI_SIGIO
	REGISTER_LONG_CONSTANT("SI_SIGIO",   SI_SIGIO,   CONST_CS | CONST_PERSISTENT);
#endif
#ifdef SI_TKILL
	REGISTER_LONG_CONSTANT("SI_TKILL",   SI_TKILL,   CONST_CS | CONST_PERSISTENT);
#endif

	/* si_code for SIGCHILD */
#ifdef CLD_EXITED
	REGISTER_LONG_CONSTANT("CLD_EXITED",    CLD_EXITED,    CONST_CS | CONST_PERSISTENT);
#endif
#ifdef CLD_KILLED
	REGISTER_LONG_CONSTANT("CLD_KILLED",    CLD_KILLED,    CONST_CS | CONST_PERSISTENT);
#endif
#ifdef CLD_DUMPED
	REGISTER_LONG_CONSTANT("CLD_DUMPED",    CLD_DUMPED,    CONST_CS | CONST_PERSISTENT);
#endif
#ifdef CLD_TRAPPED
	REGISTER_LONG_CONSTANT("CLD_TRAPPED",   CLD_TRAPPED,   CONST_CS | CONST_PERSISTENT);
#endif
#ifdef CLD_STOPPED
	REGISTER_LONG_CONSTANT("CLD_STOPPED",   CLD_STOPPED,   CONST_CS | CONST_PERSISTENT);
#endif
#ifdef CLD_CONTINUED
	REGISTER_LONG_CONSTANT("CLD_CONTINUED", CLD_CONTINUED, CONST_CS | CONST_PERSISTENT);
#endif

	/* si_code for SIGTRAP */
#ifdef TRAP_BRKPT
	REGISTER_LONG_CONSTANT("TRAP_BRKPT", TRAP_BRKPT, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef TRAP_TRACE
	REGISTER_LONG_CONSTANT("TRAP_TRACE", TRAP_TRACE, CONST_CS | CONST_PERSISTENT);
#endif

	/* si_code for SIGPOLL */
#ifdef POLL_IN
	REGISTER_LONG_CONSTANT("POLL_IN",  POLL_IN,  CONST_CS | CONST_PERSISTENT);
#endif
#ifdef POLL_OUT
	REGISTER_LONG_CONSTANT("POLL_OUT", POLL_OUT, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef POLL_MSG
	REGISTER_LONG_CONSTANT("POLL_MSG", POLL_MSG, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef POLL_ERR
	REGISTER_LONG_CONSTANT("POLL_ERR", POLL_ERR, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef POLL_PRI
	REGISTER_LONG_CONSTANT("POLL_PRI", POLL_PRI, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef POLL_HUP
	REGISTER_LONG_CONSTANT("POLL_HUP", POLL_HUP, CONST_CS | CONST_PERSISTENT);
#endif

#ifdef ILL_ILLOPC
	REGISTER_LONG_CONSTANT("ILL_ILLOPC", ILL_ILLOPC, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ILL_ILLOPN
	REGISTER_LONG_CONSTANT("ILL_ILLOPN", ILL_ILLOPN, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ILL_ILLADR
	REGISTER_LONG_CONSTANT("ILL_ILLADR", ILL_ILLADR, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ILL_ILLTRP
	REGISTER_LONG_CONSTANT("ILL_ILLTRP", ILL_ILLTRP, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ILL_PRVOPC
	REGISTER_LONG_CONSTANT("ILL_PRVOPC", ILL_PRVOPC, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ILL_PRVREG
	REGISTER_LONG_CONSTANT("ILL_PRVREG", ILL_PRVREG, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ILL_COPROC
	REGISTER_LONG_CONSTANT("ILL_COPROC", ILL_COPROC, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef ILL_BADSTK
	REGISTER_LONG_CONSTANT("ILL_BADSTK", ILL_BADSTK, CONST_CS | CONST_PERSISTENT);
#endif

#ifdef FPE_INTDIV
	REGISTER_LONG_CONSTANT("FPE_INTDIV", FPE_INTDIV, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef FPE_INTOVF
	REGISTER_LONG_CONSTANT("FPE_INTOVF", FPE_INTOVF, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef FPE_FLTDIV
	REGISTER_LONG_CONSTANT("FPE_FLTDIV", FPE_FLTDIV, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef FPE_FLTOVF
	REGISTER_LONG_CONSTANT("FPE_FLTOVF", FPE_FLTOVF, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef FPE_FLTUND
	REGISTER_LONG_CONSTANT("FPE_FLTUND", FPE_FLTINV, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef FPE_FLTRES
	REGISTER_LONG_CONSTANT("FPE_FLTRES", FPE_FLTRES, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef FPE_FLTINV
	REGISTER_LONG_CONSTANT("FPE_FLTINV", FPE_FLTINV, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef FPE_FLTSUB
	REGISTER_LONG_CONSTANT("FPE_FLTSUB", FPE_FLTSUB, CONST_CS | CONST_PERSISTENT);
#endif

#ifdef SEGV_MAPERR
	REGISTER_LONG_CONSTANT("SEGV_MAPERR", SEGV_MAPERR, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef SEGV_ACCERR
	REGISTER_LONG_CONSTANT("SEGV_ACCERR", SEGV_ACCERR, CONST_CS | CONST_PERSISTENT);
#endif

#ifdef BUS_ADRALN
	REGISTER_LONG_CONSTANT("BUS_ADRALN", BUS_ADRALN, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef BUS_ADRERR
	REGISTER_LONG_CONSTANT("BUS_ADRERR", BUS_ADRERR, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef BUS_OBJERR
	REGISTER_LONG_CONSTANT("BUS_OBJERR", BUS_OBJERR, CONST_CS | CONST_PERSISTENT);
#endif
#endif /* defined(HAVE_SIGWAITINFO) && defined(HAVE_SIGTIMEDWAIT) */
	/* }}} */

	/* unshare(/clone) constants */
#ifdef HAVE_UNSHARE
	REGISTER_LONG_CONSTANT("CLONE_NEWNS",		CLONE_NEWNS, CONST_CS | CONST_PERSISTENT);
#ifdef CLONE_NEWIPC
	REGISTER_LONG_CONSTANT("CLONE_NEWIPC",		CLONE_NEWIPC, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef CLONE_NEWUTS
	REGISTER_LONG_CONSTANT("CLONE_NEWUTS",		CLONE_NEWUTS, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef CLONE_NEWNET
	REGISTER_LONG_CONSTANT("CLONE_NEWNET",		CLONE_NEWNET, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef CLONE_NEWPID
	REGISTER_LONG_CONSTANT("CLONE_NEWPID",		CLONE_NEWPID, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef CLONE_NEWUSER
	REGISTER_LONG_CONSTANT("CLONE_NEWUSER",		CLONE_NEWUSER, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef CLONE_NEWCGROUP
	REGISTER_LONG_CONSTANT("CLONE_NEWCGROUP",	CLONE_NEWCGROUP, CONST_CS | CONST_PERSISTENT);
#endif
#endif

#ifdef HAVE_RFORK
#ifdef RFPROC
	REGISTER_LONG_CONSTANT("RFPROC",	RFPROC, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef RFNOWAIT
	REGISTER_LONG_CONSTANT("RFNOWAIT",	RFNOWAIT, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef RFCFDG
	REGISTER_LONG_CONSTANT("RFCFDG",	RFCFDG, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef RFFDG
	REGISTER_LONG_CONSTANT("RFFDG",	RFFDG, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef RFLINUXTHPN
	REGISTER_LONG_CONSTANT("RFLINUXTHPN",	RFLINUXTHPN, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef RFTSIGZMB
	REGISTER_LONG_CONSTANT("RFTSIGZMB",	RFTSIGZMB, CONST_CS | CONST_PERSISTENT);
#endif
#ifdef RFTHREAD
	REGISTER_LONG_CONSTANT("RFTHREAD",	RFTHREAD, CONST_CS | CONST_PERSISTENT);
#endif
#endif

#ifdef HAVE_FORKX
	REGISTER_LONG_CONSTANT("FORK_NOSIGCHLD", FORK_NOSIGCHLD, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FORK_WAITPID", FORK_WAITPID, CONST_CS | CONST_PERSISTENT);
#endif
}

static void php_pcntl_register_errno_constants(INIT_FUNC_ARGS)
{
#ifdef EINTR
	REGISTER_PCNTL_ERRNO_CONSTANT(EINTR);
#endif
#ifdef ECHILD
	REGISTER_PCNTL_ERRNO_CONSTANT(ECHILD);
#endif
#ifdef EINVAL
	REGISTER_PCNTL_ERRNO_CONSTANT(EINVAL);
#endif
#ifdef EAGAIN
	REGISTER_PCNTL_ERRNO_CONSTANT(EAGAIN);
#endif
#ifdef ESRCH
	REGISTER_PCNTL_ERRNO_CONSTANT(ESRCH);
#endif
#ifdef EACCES
	REGISTER_PCNTL_ERRNO_CONSTANT(EACCES);
#endif
#ifdef EPERM
	REGISTER_PCNTL_ERRNO_CONSTANT(EPERM);
#endif
#ifdef ENOMEM
	REGISTER_PCNTL_ERRNO_CONSTANT(ENOMEM);
#endif
#ifdef E2BIG
	REGISTER_PCNTL_ERRNO_CONSTANT(E2BIG);
#endif
#ifdef EFAULT
	REGISTER_PCNTL_ERRNO_CONSTANT(EFAULT);
#endif
#ifdef EIO
	REGISTER_PCNTL_ERRNO_CONSTANT(EIO);
#endif
#ifdef EISDIR
	REGISTER_PCNTL_ERRNO_CONSTANT(EISDIR);
#endif
#ifdef ELIBBAD
	REGISTER_PCNTL_ERRNO_CONSTANT(ELIBBAD);
#endif
#ifdef ELOOP
	REGISTER_PCNTL_ERRNO_CONSTANT(ELOOP);
#endif
#ifdef EMFILE
	REGISTER_PCNTL_ERRNO_CONSTANT(EMFILE);
#endif
#ifdef ENAMETOOLONG
	REGISTER_PCNTL_ERRNO_CONSTANT(ENAMETOOLONG);
#endif
#ifdef ENFILE
	REGISTER_PCNTL_ERRNO_CONSTANT(ENFILE);
#endif
#ifdef ENOENT
	REGISTER_PCNTL_ERRNO_CONSTANT(ENOENT);
#endif
#ifdef ENOEXEC
	REGISTER_PCNTL_ERRNO_CONSTANT(ENOEXEC);
#endif
#ifdef ENOTDIR
	REGISTER_PCNTL_ERRNO_CONSTANT(ENOTDIR);
#endif
#ifdef ETXTBSY
	REGISTER_PCNTL_ERRNO_CONSTANT(ETXTBSY);
#endif
#ifdef ENOSPC
	REGISTER_PCNTL_ERRNO_CONSTANT(ENOSPC);
#endif
#ifdef EUSERS
	REGISTER_PCNTL_ERRNO_CONSTANT(EUSERS);
#endif
}

static PHP_GINIT_FUNCTION(pcntl)
{
#if defined(COMPILE_DL_PCNTL) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	memset(pcntl_globals, 0, sizeof(*pcntl_globals));
}

PHP_RINIT_FUNCTION(pcntl)
{
	php_add_tick_function(pcntl_signal_dispatch_tick_function, NULL);
	zend_hash_init(&PCNTL_G(php_signal_table), 16, NULL, ZVAL_PTR_DTOR, 0);
	PCNTL_G(head) = PCNTL_G(tail) = PCNTL_G(spares) = NULL;
	PCNTL_G(async_signals) = 0;
	PCNTL_G(last_error) = 0;
	PCNTL_G(num_signals) = NSIG;
#ifdef SIGRTMAX
	/* At least FreeBSD reports an incorrecrt NSIG that does not include realtime signals.
	 * As SIGRTMAX may be a dynamic value, adjust the value in INIT. */
	if (NSIG < SIGRTMAX + 1) {
		PCNTL_G(num_signals) = SIGRTMAX + 1;
	}
#endif
	return SUCCESS;
}

PHP_MINIT_FUNCTION(pcntl)
{
	php_register_signal_constants(INIT_FUNC_ARGS_PASSTHRU);
	php_pcntl_register_errno_constants(INIT_FUNC_ARGS_PASSTHRU);
	orig_interrupt_function = zend_interrupt_function;
	zend_interrupt_function = pcntl_interrupt_function;

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(pcntl)
{
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(pcntl)
{
	struct php_pcntl_pending_signal *sig;

	/* FIXME: if a signal is delivered after this point, things will go pear shaped;
	 * need to remove signal handlers */
	zend_hash_destroy(&PCNTL_G(php_signal_table));
	while (PCNTL_G(head)) {
		sig = PCNTL_G(head);
		PCNTL_G(head) = sig->next;
		efree(sig);
	}
	while (PCNTL_G(spares)) {
		sig = PCNTL_G(spares);
		PCNTL_G(spares) = sig->next;
		efree(sig);
	}
	return SUCCESS;
}

PHP_MINFO_FUNCTION(pcntl)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "pcntl support", "enabled");
	php_info_print_table_end();
}

/* {{{ Forks the currently running process following the same behavior as the UNIX fork() system call*/
PHP_FUNCTION(pcntl_fork)
{
	pid_t id;

	if (zend_parse_parameters_none() == FAILURE) {
		RETURN_THROWS();
	}

	id = fork();
	if (id == -1) {
		PCNTL_G(last_error) = errno;
		php_error_docref(NULL, E_WARNING, "Error %d", errno);
	}

	RETURN_LONG((zend_long) id);
}
/* }}} */

/* {{{ Set an alarm clock for delivery of a signal*/
PHP_FUNCTION(pcntl_alarm)
{
	zend_long seconds;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &seconds) == FAILURE) {
		RETURN_THROWS();
	}

	RETURN_LONG((zend_long) alarm(seconds));
}
/* }}} */

#define PHP_RUSAGE_PARA(from, to, field) \
	add_assoc_long(to, #field, from.field)
#ifndef _OSD_POSIX
	#define PHP_RUSAGE_SPECIAL(from, to) \
		PHP_RUSAGE_PARA(from, to, ru_oublock); \
		PHP_RUSAGE_PARA(from, to, ru_inblock); \
		PHP_RUSAGE_PARA(from, to, ru_msgsnd); \
		PHP_RUSAGE_PARA(from, to, ru_msgrcv); \
		PHP_RUSAGE_PARA(from, to, ru_maxrss); \
		PHP_RUSAGE_PARA(from, to, ru_ixrss); \
		PHP_RUSAGE_PARA(from, to, ru_idrss); \
		PHP_RUSAGE_PARA(from, to, ru_minflt); \
		PHP_RUSAGE_PARA(from, to, ru_majflt); \
		PHP_RUSAGE_PARA(from, to, ru_nsignals); \
		PHP_RUSAGE_PARA(from, to, ru_nvcsw); \
		PHP_RUSAGE_PARA(from, to, ru_nivcsw); \
		PHP_RUSAGE_PARA(from, to, ru_nswap);
#else /*_OSD_POSIX*/
	#define PHP_RUSAGE_SPECIAL(from, to)
#endif

#define PHP_RUSAGE_COMMON(from ,to) \
	PHP_RUSAGE_PARA(from, to, ru_utime.tv_usec); \
	PHP_RUSAGE_PARA(from, to, ru_utime.tv_sec); \
	PHP_RUSAGE_PARA(from, to, ru_stime.tv_usec); \
	PHP_RUSAGE_PARA(from, to, ru_stime.tv_sec);

#define PHP_RUSAGE_TO_ARRAY(from, to) \
	if (to) { \
		PHP_RUSAGE_SPECIAL(from, to) \
		PHP_RUSAGE_COMMON(from, to); \
	}

/* {{{ Waits on or returns the status of a forked child as defined by the waitpid() system call */
PHP_FUNCTION(pcntl_waitpid)
{
	zend_long pid, options = 0;
	zval *z_status = NULL, *z_rusage = NULL;
	int status;
	pid_t child_id;
#ifdef HAVE_WAIT4
	struct rusage rusage;
#endif

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "lz|lz", &pid, &z_status, &options, &z_rusage) == FAILURE) {
		RETURN_THROWS();
	}

	status = zval_get_long(z_status);

#ifdef HAVE_WAIT4
	if (z_rusage) {
		z_rusage = zend_try_array_init(z_rusage);
		if (!z_rusage) {
			RETURN_THROWS();
		}

		memset(&rusage, 0, sizeof(struct rusage));
		child_id = wait4((pid_t) pid, &status, options, &rusage);
	} else {
		child_id = waitpid((pid_t) pid, &status, options);
	}
#else
	child_id = waitpid((pid_t) pid, &status, options);
#endif

	if (child_id < 0) {
		PCNTL_G(last_error) = errno;
	}

#ifdef HAVE_WAIT4
	if (child_id > 0) {
		PHP_RUSAGE_TO_ARRAY(rusage, z_rusage);
	}
#endif

	ZEND_TRY_ASSIGN_REF_LONG(z_status, status);

	RETURN_LONG((zend_long) child_id);
}
/* }}} */

/* {{{ Waits on or returns the status of a forked child as defined by the waitpid() system call */
PHP_FUNCTION(pcntl_wait)
{
	zend_long options = 0;
	zval *z_status = NULL, *z_rusage = NULL;
	int status;
	pid_t child_id;
#ifdef HAVE_WAIT3
	struct rusage rusage;
#endif

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z|lz", &z_status, &options, &z_rusage) == FAILURE) {
		RETURN_THROWS();
	}

	status = zval_get_long(z_status);
#ifdef HAVE_WAIT3
	if (z_rusage) {
		z_rusage = zend_try_array_init(z_rusage);
		if (!z_rusage) {
			RETURN_THROWS();
		}

		memset(&rusage, 0, sizeof(struct rusage));
		child_id = wait3(&status, options, &rusage);
	} else if (options) {
		child_id = wait3(&status, options, NULL);
	} else {
		child_id = wait(&status);
	}
#else
	child_id = wait(&status);
#endif
	if (child_id < 0) {
		PCNTL_G(last_error) = errno;
	}

#ifdef HAVE_WAIT3
	if (child_id > 0) {
		PHP_RUSAGE_TO_ARRAY(rusage, z_rusage);
	}
#endif

	ZEND_TRY_ASSIGN_REF_LONG(z_status, status);

	RETURN_LONG((zend_long) child_id);
}
/* }}} */

#undef PHP_RUSAGE_PARA
#undef PHP_RUSAGE_SPECIAL
#undef PHP_RUSAGE_COMMON
#undef PHP_RUSAGE_TO_ARRAY

/* {{{ Returns true if the child status code represents a successful exit */
PHP_FUNCTION(pcntl_wifexited)
{
	zend_long status_word;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &status_word) == FAILURE) {
		RETURN_THROWS();
	}

#ifdef WIFEXITED
	int int_status_word = (int) status_word;
	if (WIFEXITED(int_status_word)) {
		RETURN_TRUE;
	}
#endif

	RETURN_FALSE;
}
/* }}} */

/* {{{ Returns true if the child status code represents a stopped process (WUNTRACED must have been used with waitpid) */
PHP_FUNCTION(pcntl_wifstopped)
{
	zend_long status_word;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &status_word) == FAILURE) {
		RETURN_THROWS();
	}

#ifdef WIFSTOPPED
	int int_status_word = (int) status_word;
	if (WIFSTOPPED(int_status_word)) {
		RETURN_TRUE;
	}
#endif

	RETURN_FALSE;
}
/* }}} */

/* {{{ Returns true if the child status code represents a process that was terminated due to a signal */
PHP_FUNCTION(pcntl_wifsignaled)
{
	zend_long status_word;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &status_word) == FAILURE) {
		RETURN_THROWS();
	}

#ifdef WIFSIGNALED
	int int_status_word = (int) status_word;
	if (WIFSIGNALED(int_status_word)) {
		RETURN_TRUE;
	}
#endif

	RETURN_FALSE;
}
/* }}} */
/* {{{ Returns true if the child status code represents a process that was resumed due to a SIGCONT signal */
PHP_FUNCTION(pcntl_wifcontinued)
{
	zend_long status_word;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &status_word) == FAILURE) {
		RETURN_THROWS();
	}

#ifdef HAVE_WCONTINUED
	int int_status_word = (int) status_word;
	if (WIFCONTINUED(int_status_word)) {
		RETURN_TRUE;
	}
#endif
	RETURN_FALSE;
}
/* }}} */


/* {{{ Returns the status code of a child's exit */
PHP_FUNCTION(pcntl_wexitstatus)
{
	zend_long status_word;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &status_word) == FAILURE) {
		RETURN_THROWS();
	}

#ifdef WEXITSTATUS
	int int_status_word = (int) status_word;
	RETURN_LONG(WEXITSTATUS(int_status_word));
#else
	RETURN_FALSE;
#endif
}
/* }}} */

/* {{{ Returns the number of the signal that terminated the process who's status code is passed  */
PHP_FUNCTION(pcntl_wtermsig)
{
	zend_long status_word;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &status_word) == FAILURE) {
		RETURN_THROWS();
	}

#ifdef WTERMSIG
	int int_status_word = (int) status_word;
	RETURN_LONG(WTERMSIG(int_status_word));
#else
	RETURN_FALSE;
#endif
}
/* }}} */

/* {{{ Returns the number of the signal that caused the process to stop who's status code is passed */
PHP_FUNCTION(pcntl_wstopsig)
{
	zend_long status_word;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &status_word) == FAILURE) {
		RETURN_THROWS();
	}

#ifdef WSTOPSIG
	int int_status_word = (int) status_word;
	RETURN_LONG(WSTOPSIG(int_status_word));
#else
	RETURN_FALSE;
#endif
}
/* }}} */

/* {{{ Executes specified program in current process space as defined by exec(2) */
PHP_FUNCTION(pcntl_exec)
{
	zval *args = NULL, *envs = NULL;
	zval *element;
	HashTable *args_hash, *envs_hash;
	int argc = 0, argi = 0;
	int envc = 0, envi = 0;
	char **argv = NULL, **envp = NULL;
	char **current_arg, **pair;
	size_t pair_length;
	zend_string *key;
	char *path;
	size_t path_len;
	zend_ulong key_num;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "p|aa", &path, &path_len, &args, &envs) == FAILURE) {
		RETURN_THROWS();
	}

	if (ZEND_NUM_ARGS() > 1) {
		/* Build argument list */
		SEPARATE_ARRAY(args);
		args_hash = Z_ARRVAL_P(args);
		argc = zend_hash_num_elements(args_hash);

		argv = safe_emalloc((argc + 2), sizeof(char *), 0);
		*argv = path;
		current_arg = argv+1;
		ZEND_HASH_FOREACH_VAL(args_hash, element) {
			if (argi >= argc) break;
			if (!try_convert_to_string(element)) {
				efree(argv);
				RETURN_THROWS();
			}

			*current_arg = Z_STRVAL_P(element);
			argi++;
			current_arg++;
		} ZEND_HASH_FOREACH_END();
		*current_arg = NULL;
	} else {
		argv = emalloc(2 * sizeof(char *));
		argv[0] = path;
		argv[1] = NULL;
	}

	if ( ZEND_NUM_ARGS() == 3 ) {
		/* Build environment pair list */
		SEPARATE_ARRAY(envs);
		envs_hash = Z_ARRVAL_P(envs);
		envc = zend_hash_num_elements(envs_hash);

		pair = envp = safe_emalloc((envc + 1), sizeof(char *), 0);
		ZEND_HASH_FOREACH_KEY_VAL(envs_hash, key_num, key, element) {
			if (envi >= envc) break;
			if (!key) {
				key = zend_long_to_str(key_num);
			} else {
				zend_string_addref(key);
			}

			if (!try_convert_to_string(element)) {
				zend_string_release(key);
				efree(argv);
				efree(envp);
				RETURN_THROWS();
			}

			/* Length of element + equal sign + length of key + null */
			ZEND_ASSERT(Z_STRLEN_P(element) < SIZE_MAX && ZSTR_LEN(key) < SIZE_MAX);
			*pair = safe_emalloc(Z_STRLEN_P(element) + 1, sizeof(char), ZSTR_LEN(key) + 1);
			pair_length = Z_STRLEN_P(element) + ZSTR_LEN(key) + 2;
			strlcpy(*pair, ZSTR_VAL(key), ZSTR_LEN(key) + 1);
			strlcat(*pair, "=", pair_length);
			strlcat(*pair, Z_STRVAL_P(element), pair_length);

			/* Cleanup */
			zend_string_release_ex(key, 0);
			envi++;
			pair++;
		} ZEND_HASH_FOREACH_END();
		*(pair) = NULL;

		if (execve(path, argv, envp) == -1) {
			PCNTL_G(last_error) = errno;
			php_error_docref(NULL, E_WARNING, "Error has occurred: (errno %d) %s", errno, strerror(errno));
		}

		/* Cleanup */
		for (pair = envp; *pair != NULL; pair++) efree(*pair);
		efree(envp);
	} else {

		if (execv(path, argv) == -1) {
			PCNTL_G(last_error) = errno;
			php_error_docref(NULL, E_WARNING, "Error has occurred: (errno %d) %s", errno, strerror(errno));
		}
	}

	efree(argv);

	RETURN_FALSE;
}
/* }}} */

/* {{{ Assigns a system signal handler to a PHP function */
PHP_FUNCTION(pcntl_signal)
{
	zval *handle;
	zend_long signo;
	bool restart_syscalls = 1;
	bool restart_syscalls_is_null = 1;
	char *error = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "lz|b!", &signo, &handle, &restart_syscalls, &restart_syscalls_is_null) == FAILURE) {
		RETURN_THROWS();
	}

	if (signo < 1) {
		zend_argument_value_error(1, "must be greater than or equal to 1");
		RETURN_THROWS();
	}

	if (signo >= PCNTL_G(num_signals)) {
		zend_argument_value_error(1, "must be less than %d", PCNTL_G(num_signals));
		RETURN_THROWS();
	}

	if (!PCNTL_G(spares)) {
		/* since calling malloc() from within a signal handler is not portable,
		 * pre-allocate a few records for recording signals */
		int i;
		for (i = 0; i < PCNTL_G(num_signals); i++) {
			struct php_pcntl_pending_signal *psig;

			psig = emalloc(sizeof(*psig));
			psig->next = PCNTL_G(spares);
			PCNTL_G(spares) = psig;
		}
	}

	/* If restart_syscalls was not explicitly specified and the signal is SIGALRM, then default
	 * restart_syscalls to false. PHP used to enforce that restart_syscalls is false for SIGALRM,
	 * so we keep this differing default to reduce the degree of BC breakage. */
	if (restart_syscalls_is_null && signo == SIGALRM) {
		restart_syscalls = 0;
	}

	/* Special long value case for SIG_DFL and SIG_IGN */
	if (Z_TYPE_P(handle) == IS_LONG) {
		if (Z_LVAL_P(handle) != (zend_long) SIG_DFL && Z_LVAL_P(handle) != (zend_long) SIG_IGN) {
			zend_argument_value_error(2, "must be either SIG_DFL or SIG_IGN when an integer value is given");
			RETURN_THROWS();
		}
		if (php_signal(signo, (Sigfunc *) Z_LVAL_P(handle), (int) restart_syscalls) == (void *)SIG_ERR) {
			PCNTL_G(last_error) = errno;
			php_error_docref(NULL, E_WARNING, "Error assigning signal");
			RETURN_FALSE;
		}
		zend_hash_index_update(&PCNTL_G(php_signal_table), signo, handle);
		RETURN_TRUE;
	}

	if (!zend_is_callable_ex(handle, NULL, 0, NULL, NULL, &error)) {
		zend_string *func_name = zend_get_callable_name(handle);
		PCNTL_G(last_error) = EINVAL;

		zend_argument_type_error(2, "must be of type callable|int, %s given", zend_zval_type_name(handle));
		zend_string_release_ex(func_name, 0);
		efree(error);
		RETURN_THROWS();
	}
	ZEND_ASSERT(!error);

	/* Add the function name to our signal table */
	handle = zend_hash_index_update(&PCNTL_G(php_signal_table), signo, handle);
	Z_TRY_ADDREF_P(handle);

	if (php_signal4(signo, pcntl_signal_handler, (int) restart_syscalls, 1) == (void *)SIG_ERR) {
		PCNTL_G(last_error) = errno;
		php_error_docref(NULL, E_WARNING, "Error assigning signal");
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ Gets signal handler */
PHP_FUNCTION(pcntl_signal_get_handler)
{
	zval *prev_handle;
	zend_long signo;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &signo) == FAILURE) {
		RETURN_THROWS();
	}

	if (signo < 1 || signo > 32) {
		zend_argument_value_error(1, "must be between 1 and 32");
		RETURN_THROWS();
	}

	if ((prev_handle = zend_hash_index_find(&PCNTL_G(php_signal_table), signo)) != NULL) {
		RETURN_COPY(prev_handle);
	} else {
		RETURN_LONG((zend_long)SIG_DFL);
	}
}

/* {{{ Dispatch signals to signal handlers */
PHP_FUNCTION(pcntl_signal_dispatch)
{
	if (zend_parse_parameters_none() == FAILURE) {
		RETURN_THROWS();
	}

	pcntl_signal_dispatch();
	RETURN_TRUE;
}
/* }}} */

#ifdef HAVE_SIGPROCMASK
/* {{{ Examine and change blocked signals */
PHP_FUNCTION(pcntl_sigprocmask)
{
	zend_long          how, signo;
	zval         *user_set, *user_oldset = NULL, *user_signo;
	sigset_t      set, oldset;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "la|z", &how, &user_set, &user_oldset) == FAILURE) {
		RETURN_THROWS();
	}

	if (sigemptyset(&set) != 0 || sigemptyset(&oldset) != 0) {
		PCNTL_G(last_error) = errno;
		php_error_docref(NULL, E_WARNING, "%s", strerror(errno));
		RETURN_FALSE;
	}

	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(user_set), user_signo) {
		signo = zval_get_long(user_signo);
		if (sigaddset(&set, signo) != 0) {
			PCNTL_G(last_error) = errno;
			php_error_docref(NULL, E_WARNING, "%s", strerror(errno));
			RETURN_FALSE;
		}
	} ZEND_HASH_FOREACH_END();

	if (sigprocmask(how, &set, &oldset) != 0) {
		PCNTL_G(last_error) = errno;
		php_error_docref(NULL, E_WARNING, "%s", strerror(errno));
		RETURN_FALSE;
	}

	if (user_oldset != NULL) {
		user_oldset = zend_try_array_init(user_oldset);
		if (!user_oldset) {
			RETURN_THROWS();
		}

		for (signo = 1; signo < PCNTL_G(num_signals); ++signo) {
			if (sigismember(&oldset, signo) != 1) {
				continue;
			}
			add_next_index_long(user_oldset, signo);
		}
	}

	RETURN_TRUE;
}
/* }}} */
#endif

#ifdef HAVE_STRUCT_SIGINFO_T
# if defined(HAVE_SIGWAITINFO) && defined(HAVE_SIGTIMEDWAIT)
static void pcntl_sigwaitinfo(INTERNAL_FUNCTION_PARAMETERS, int timedwait) /* {{{ */
{
	zval            *user_set, *user_signo, *user_siginfo = NULL;
	zend_long             tv_sec = 0, tv_nsec = 0;
	sigset_t         set;
	int              signo;
	siginfo_t        siginfo;
	struct timespec  timeout;

	if (timedwait) {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "a|zll", &user_set, &user_siginfo, &tv_sec, &tv_nsec) == FAILURE) {
			RETURN_THROWS();
		}
	} else {
		if (zend_parse_parameters(ZEND_NUM_ARGS(), "a|z", &user_set, &user_siginfo) == FAILURE) {
			RETURN_THROWS();
		}
	}

	if (sigemptyset(&set) != 0) {
		PCNTL_G(last_error) = errno;
		php_error_docref(NULL, E_WARNING, "%s", strerror(errno));
		RETURN_FALSE;
	}

	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(user_set), user_signo) {
		signo = zval_get_long(user_signo);
		if (sigaddset(&set, signo) != 0) {
			PCNTL_G(last_error) = errno;
			php_error_docref(NULL, E_WARNING, "%s", strerror(errno));
			RETURN_FALSE;
		}
	} ZEND_HASH_FOREACH_END();

	if (timedwait) {
		timeout.tv_sec  = (time_t) tv_sec;
		timeout.tv_nsec = tv_nsec;
		signo = sigtimedwait(&set, &siginfo, &timeout);
	} else {
		signo = sigwaitinfo(&set, &siginfo);
	}
	if (signo == -1 && errno != EAGAIN) {
		PCNTL_G(last_error) = errno;
		php_error_docref(NULL, E_WARNING, "%s", strerror(errno));
	}

	/*
	 * sigtimedwait and sigwaitinfo can return 0 on success on some
	 * platforms, e.g. NetBSD
	 */
	if (!signo && siginfo.si_signo) {
		signo = siginfo.si_signo;
	}
	pcntl_siginfo_to_zval(signo, &siginfo, user_siginfo);
	RETURN_LONG(signo);
}
/* }}} */

/* {{{ Synchronously wait for queued signals */
PHP_FUNCTION(pcntl_sigwaitinfo)
{
	pcntl_sigwaitinfo(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ Wait for queued signals */
PHP_FUNCTION(pcntl_sigtimedwait)
{
	pcntl_sigwaitinfo(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */
# endif

static void pcntl_siginfo_to_zval(int signo, siginfo_t *siginfo, zval *user_siginfo) /* {{{ */
{
	if (signo > 0 && user_siginfo) {
		user_siginfo = zend_try_array_init(user_siginfo);
		if (!user_siginfo) {
			return;
		}

		add_assoc_long_ex(user_siginfo, "signo", sizeof("signo")-1, siginfo->si_signo);
		add_assoc_long_ex(user_siginfo, "errno", sizeof("errno")-1, siginfo->si_errno);
		add_assoc_long_ex(user_siginfo, "code",  sizeof("code")-1,  siginfo->si_code);
		switch(signo) {
#ifdef SIGCHLD
			case SIGCHLD:
				add_assoc_long_ex(user_siginfo,   "status", sizeof("status")-1, siginfo->si_status);
# ifdef si_utime
				add_assoc_double_ex(user_siginfo, "utime",  sizeof("utime")-1,  siginfo->si_utime);
# endif
# ifdef si_stime
				add_assoc_double_ex(user_siginfo, "stime",  sizeof("stime")-1,  siginfo->si_stime);
# endif
				add_assoc_long_ex(user_siginfo,   "pid",    sizeof("pid")-1,    siginfo->si_pid);
				add_assoc_long_ex(user_siginfo,   "uid",    sizeof("uid")-1,    siginfo->si_uid);
				break;
			case SIGUSR1:
			case SIGUSR2:
				add_assoc_long_ex(user_siginfo,   "pid",    sizeof("pid")-1,    siginfo->si_pid);
				add_assoc_long_ex(user_siginfo,   "uid",    sizeof("uid")-1,    siginfo->si_uid);
				break;
#endif
			case SIGILL:
			case SIGFPE:
			case SIGSEGV:
			case SIGBUS:
				add_assoc_double_ex(user_siginfo, "addr", sizeof("addr")-1, (zend_long)siginfo->si_addr);
				break;
#ifdef SIGPOLL
			case SIGPOLL:
				add_assoc_long_ex(user_siginfo, "band", sizeof("band")-1, siginfo->si_band);
# ifdef si_fd
				add_assoc_long_ex(user_siginfo, "fd",   sizeof("fd")-1,   siginfo->si_fd);
# endif
				break;
#endif
		}
#if defined(SIGRTMIN) && defined(SIGRTMAX)
		if (SIGRTMIN <= signo && signo <= SIGRTMAX) {
			add_assoc_long_ex(user_siginfo, "pid", sizeof("pid")-1, siginfo->si_pid);
			add_assoc_long_ex(user_siginfo, "uid", sizeof("uid")-1, siginfo->si_uid);
		}
#endif
	}
}
/* }}} */
#endif

#ifdef HAVE_GETPRIORITY
/* {{{ Get the priority of any process */
PHP_FUNCTION(pcntl_getpriority)
{
	zend_long who = PRIO_PROCESS;
	zend_long pid;
	bool pid_is_null = 1;
	int pri;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l!l", &pid, &pid_is_null, &who) == FAILURE) {
		RETURN_THROWS();
	}

	/* needs to be cleared, since any returned value is valid */
	errno = 0;

	pri = getpriority(who, pid_is_null ? getpid() : pid);

	if (errno) {
		PCNTL_G(last_error) = errno;
		switch (errno) {
			case ESRCH:
				php_error_docref(NULL, E_WARNING, "Error %d: No process was located using the given parameters", errno);
				break;
			case EINVAL:
				zend_argument_value_error(2, "must be one of PRIO_PGRP, PRIO_USER, or PRIO_PROCESS");
				RETURN_THROWS();
			default:
				php_error_docref(NULL, E_WARNING, "Unknown error %d has occurred", errno);
				break;
		}
		RETURN_FALSE;
	}

	RETURN_LONG(pri);
}
/* }}} */
#endif

#ifdef HAVE_SETPRIORITY
/* {{{ Change the priority of any process */
PHP_FUNCTION(pcntl_setpriority)
{
	zend_long who = PRIO_PROCESS;
	zend_long pid;
	bool pid_is_null = 1;
	zend_long pri;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l|l!l", &pri, &pid, &pid_is_null, &who) == FAILURE) {
		RETURN_THROWS();
	}

	if (setpriority(who, pid_is_null ? getpid() : pid, pri)) {
		PCNTL_G(last_error) = errno;
		switch (errno) {
			case ESRCH:
				php_error_docref(NULL, E_WARNING, "Error %d: No process was located using the given parameters", errno);
				break;
			case EINVAL:
				zend_argument_value_error(3, "must be one of PRIO_PGRP, PRIO_USER, or PRIO_PROCESS");
				RETURN_THROWS();
			case EPERM:
				php_error_docref(NULL, E_WARNING, "Error %d: A process was located, but neither its effective nor real user ID matched the effective user ID of the caller", errno);
				break;
			case EACCES:
				php_error_docref(NULL, E_WARNING, "Error %d: Only a super user may attempt to increase the process priority", errno);
				break;
			default:
				php_error_docref(NULL, E_WARNING, "Unknown error %d has occurred", errno);
				break;
		}
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */
#endif

/* {{{ Retrieve the error number set by the last pcntl function which failed. */
PHP_FUNCTION(pcntl_get_last_error)
{
	if (zend_parse_parameters_none() == FAILURE) {
		RETURN_THROWS();
	}

	RETURN_LONG(PCNTL_G(last_error));
}
/* }}} */

/* {{{ Retrieve the system error message associated with the given errno. */
PHP_FUNCTION(pcntl_strerror)
{
	zend_long error;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "l", &error) == FAILURE) {
		RETURN_THROWS();
	}

	RETURN_STRING(strerror(error));
}
/* }}} */

/* Our custom signal handler that calls the appropriate php_function */
#ifdef HAVE_STRUCT_SIGINFO_T
static void pcntl_signal_handler(int signo, siginfo_t *siginfo, void *context)
#else
static void pcntl_signal_handler(int signo)
#endif
{
	struct php_pcntl_pending_signal *psig;

	psig = PCNTL_G(spares);
	if (!psig) {
		/* oops, too many signals for us to track, so we'll forget about this one */
		return;
	}
	PCNTL_G(spares) = psig->next;

	psig->signo = signo;
	psig->next = NULL;

#ifdef HAVE_STRUCT_SIGINFO_T
	psig->siginfo = *siginfo;
#endif

	/* the head check is important, as the tick handler cannot atomically clear both
	 * the head and tail */
	if (PCNTL_G(head) && PCNTL_G(tail)) {
		PCNTL_G(tail)->next = psig;
	} else {
		PCNTL_G(head) = psig;
	}
	PCNTL_G(tail) = psig;
	PCNTL_G(pending_signals) = 1;
	if (PCNTL_G(async_signals)) {
		EG(vm_interrupt) = 1;
	}
}

void pcntl_signal_dispatch()
{
	zval params[2], *handle, retval;
	struct php_pcntl_pending_signal *queue, *next;
	sigset_t mask;
	sigset_t old_mask;

	if(!PCNTL_G(pending_signals)) {
		return;
	}

	/* Mask all signals */
	sigfillset(&mask);
	sigprocmask(SIG_BLOCK, &mask, &old_mask);

	/* Bail if the queue is empty or if we are already playing the queue */
	if (!PCNTL_G(head) || PCNTL_G(processing_signal_queue)) {
		sigprocmask(SIG_SETMASK, &old_mask, NULL);
		return;
	}

	/* Prevent reentrant handler calls */
	PCNTL_G(processing_signal_queue) = 1;

	queue = PCNTL_G(head);
	PCNTL_G(head) = NULL; /* simple stores are atomic */

	/* Allocate */
	while (queue) {
		if ((handle = zend_hash_index_find(&PCNTL_G(php_signal_table), queue->signo)) != NULL) {
			if (Z_TYPE_P(handle) != IS_LONG) {
				ZVAL_NULL(&retval);
				ZVAL_LONG(&params[0], queue->signo);
#ifdef HAVE_STRUCT_SIGINFO_T
				array_init(&params[1]);
				pcntl_siginfo_to_zval(queue->signo, &queue->siginfo, &params[1]);
#else
				ZVAL_NULL(&params[1]);
#endif

				/* Call php signal handler - Note that we do not report errors, and we ignore the return value */
				/* FIXME: this is probably broken when multiple signals are handled in this while loop (retval) */
				call_user_function(NULL, NULL, handle, &retval, 2, params);
				zval_ptr_dtor(&retval);
#ifdef HAVE_STRUCT_SIGINFO_T
				zval_ptr_dtor(&params[1]);
#endif
			}
		}

		next = queue->next;
		queue->next = PCNTL_G(spares);
		PCNTL_G(spares) = queue;
		queue = next;
	}

	PCNTL_G(pending_signals) = 0;

	/* Re-enable queue */
	PCNTL_G(processing_signal_queue) = 0;

	/* return signal mask to previous state */
	sigprocmask(SIG_SETMASK, &old_mask, NULL);
}

static void pcntl_signal_dispatch_tick_function(int dummy_int, void *dummy_pointer)
{
	return pcntl_signal_dispatch();
}

/* {{{ Enable/disable asynchronous signal handling and return the old setting. */
PHP_FUNCTION(pcntl_async_signals)
{
	bool on, on_is_null = 1;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|b!", &on, &on_is_null) == FAILURE) {
		RETURN_THROWS();
	}

	if (on_is_null) {
		RETURN_BOOL(PCNTL_G(async_signals));
	}

	RETVAL_BOOL(PCNTL_G(async_signals));
	PCNTL_G(async_signals) = on;
}
/* }}} */

#ifdef HAVE_UNSHARE
/* {{{ disassociate parts of the process execution context */
PHP_FUNCTION(pcntl_unshare)
{
	zend_long flags;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(flags)
	ZEND_PARSE_PARAMETERS_END();

	if (unshare(flags) == -1) {
		PCNTL_G(last_error) = errno;
		switch (errno) {
#ifdef EINVAL
			case EINVAL:
				zend_argument_value_error(1, "must be a combination of CLONE_* flags");
				RETURN_THROWS();
				break;
#endif
#ifdef ENOMEM
			case ENOMEM:
				php_error_docref(NULL, E_WARNING, "Error %d: Insufficient memory for unshare", errno);
				break;
#endif
#ifdef EPERM
			case EPERM:
				php_error_docref(NULL, E_WARNING, "Error %d: No privilege to use these flags", errno);
				break;
#endif
#ifdef ENOSPC
			case ENOSPC:
				php_error_docref(NULL, E_WARNING, "Error %d: Reached the maximum nesting limit for one of the specified namespaces", errno);
				break;
#endif
#ifdef EUSERS
			case EUSERS:
				php_error_docref(NULL, E_WARNING, "Error %d: Reached the maximum nesting limit for the user namespace", errno);
				break;
#endif
			default:
				php_error_docref(NULL, E_WARNING, "Unknown error %d has occurred", errno);
				break;
		}
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */
#endif

#ifdef HAVE_RFORK
/* {{{ proto bool pcntl_rfork(int flags [, int signal])
   More control over the process creation is given over fork/vfork. */
PHP_FUNCTION(pcntl_rfork)
{
	zend_long flags;
	zend_long csignal = 0;
	pid_t pid;

	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_LONG(flags)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG(csignal)
	ZEND_PARSE_PARAMETERS_END();

	/* This is a flag to use with great caution in general, preferably not within PHP */
	if ((flags & RFMEM) != 0) {
		zend_argument_value_error(1, "must not include RFMEM value, not allowed within this context");
		RETURN_THROWS();
	}

	if ((flags & RFSIGSHARE) != 0) {
		zend_argument_value_error(1, "must not include RFSIGSHARE value, not allowed within this context");
		RETURN_THROWS();
	}

	if ((flags & (RFFDG | RFCFDG)) == (RFFDG | RFCFDG)) {
		zend_argument_value_error(1, "must not include both RFFDG and RFCFDG, because these flags are mutually exclusive");
		RETURN_THROWS();
	}

	/* A new pid is required */
	if (!(flags & (RFPROC))) {
		flags |= RFPROC;
	}

	if ((flags & RFTSIGZMB) != 0) {
		flags |= RFTSIGFLAGS(csignal);
	}

	pid = rfork(flags);

	if (pid == -1) {
		PCNTL_G(last_error) = errno;
		switch (errno) {
			case EAGAIN:
			php_error_docref(NULL, E_WARNING, "Maximum process creations limit reached\n");
		break;

		default:
			php_error_docref(NULL, E_WARNING, "Error %d", errno);
		}
	}

	RETURN_LONG((zend_long) pid);
}
#endif
/* }}} */

#ifdef HAVE_FORKX
/* {{{ proto bool pcntl_forkx(int flags)
   More elaborated version of fork with the following settings.
   FORK_WAITPID: forbid the parent process to wait for multiple pid but one only
   FORK_NOSIGCHLD: SIGCHLD signal ignored when the child terminates */
PHP_FUNCTION(pcntl_forkx)
{
	zend_long flags;
	pid_t pid;

	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_LONG(flags)
	ZEND_PARSE_PARAMETERS_END();

	if (flags < FORK_NOSIGCHLD || flags > FORK_WAITPID) {
		zend_argument_value_error(1, "must be FORK_NOSIGCHLD or FORK_WAITPID");
		RETURN_THROWS();
	}

	pid = forkx(flags);

	if (pid == -1) {
		PCNTL_G(last_error) = errno;
		switch (errno) {
			case EAGAIN:
			php_error_docref(NULL, E_WARNING, "Maximum process creations limit reached\n");
		break;
			case EPERM:
			php_error_docref(NULL, E_WARNING, "Calling process not having the proper privileges\n");
		break;
			case ENOMEM:
			php_error_docref(NULL, E_WARNING, "No swap space left\n");
		break;
		default:
			php_error_docref(NULL, E_WARNING, "Error %d", errno);
		}
	}

	RETURN_LONG((zend_long) pid);
}
#endif
/* }}} */

static void pcntl_interrupt_function(zend_execute_data *execute_data)
{
	pcntl_signal_dispatch();
	if (orig_interrupt_function) {
		orig_interrupt_function(execute_data);
	}
}
