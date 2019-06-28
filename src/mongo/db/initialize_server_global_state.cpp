/**
 *    Copyright (C) 2018-present MongoDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 *
 *    You should have received a copy of the Server Side Public License
 *    along with this program. If not, see
 *    <http://www.mongerdb.com/licensing/server-side-public-license>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the Server Side Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#define MONGO_LOG_DEFAULT_COMPONENT ::monger::logger::LogComponent::kControl

#include "monger/platform/basic.h"

#include "monger/db/initialize_server_global_state.h"
#include "monger/db/initialize_server_global_state_gen.h"

#include <boost/filesystem/operations.hpp>
#include <iostream>
#include <memory>
#include <signal.h>

#ifndef _WIN32
#include <sys/stat.h>
#include <sys/wait.h>
#include <syslog.h>
#endif

#include "monger/base/init.h"
#include "monger/config.h"
#include "monger/db/server_options.h"
#include "monger/logger/console_appender.h"
#include "monger/logger/logger.h"
#include "monger/logger/message_event.h"
#include "monger/logger/message_event_utf8_encoder.h"
#include "monger/logger/ramlog.h"
#include "monger/logger/rotatable_file_appender.h"
#include "monger/logger/rotatable_file_manager.h"
#include "monger/logger/rotatable_file_writer.h"
#include "monger/logger/syslog_appender.h"
#include "monger/platform/process_id.h"
#include "monger/util/log.h"
#include "monger/util/processinfo.h"
#include "monger/util/quick_exit.h"
#include "monger/util/signal_handlers_synchronous.h"
#include "monger/util/str.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace fs = boost::filesystem;

namespace monger {

using std::cerr;
using std::cout;
using std::endl;

#ifndef _WIN32
// support for exit value propagation with fork
void launchSignal(int sig) {
    if (sig == SIGUSR2) {
        ProcessId cur = ProcessId::getCurrent();

        if (cur == serverGlobalParams.parentProc || cur == serverGlobalParams.leaderProc) {
            // signal indicates successful start allowing us to exit
            quickExit(0);
        }
    }
}

void signalForkSuccess() {
    if (serverGlobalParams.doFork) {
        // killing leader will propagate to parent
        verify(kill(serverGlobalParams.leaderProc.toNative(), SIGUSR2) == 0);
    }
}
#endif


static bool forkServer() {
#if !defined(_WIN32) && !(defined(__APPLE__) && TARGET_OS_TV)
    if (serverGlobalParams.doFork) {
        fassert(16447, !serverGlobalParams.logpath.empty() || serverGlobalParams.logWithSyslog);

        cout.flush();
        cerr.flush();

        serverGlobalParams.parentProc = ProcessId::getCurrent();

        // clear signal mask so that SIGUSR2 will always be caught and we can clean up the original
        // parent process
        clearSignalMask();

        // facilitate clean exit when child starts successfully
        verify(signal(SIGUSR2, launchSignal) != SIG_ERR);

        cout << "about to fork child process, waiting until server is ready for connections."
             << endl;

        pid_t child1 = fork();
        if (child1 == -1) {
            cout << "ERROR: stage 1 fork() failed: " << errnoWithDescription();
            quickExit(EXIT_ABRUPT);
        } else if (child1) {
            // this is run in the original parent process
            int pstat;
            if (waitpid(child1, &pstat, 0) == pid_t{-1}) {
                perror("waitpid");
                quickExit(-1);
            }

            if (WIFEXITED(pstat)) {
                if (WEXITSTATUS(pstat)) {
                    cout << "ERROR: child process failed, exited with error number "
                         << WEXITSTATUS(pstat) << endl
                         << "To see additional information in this output, start without "
                         << "the \"--fork\" option." << endl;
                } else {
                    cout << "child process started successfully, parent exiting" << endl;
                }

                quickExit(WEXITSTATUS(pstat));
            }

            quickExit(50);
        }

        if (chdir("/") < 0) {
            cout << "Cant chdir() while forking server process: " << strerror(errno) << endl;
            quickExit(-1);
        }
        setsid();

        serverGlobalParams.leaderProc = ProcessId::getCurrent();

        pid_t child2 = fork();
        if (child2 == -1) {
            cout << "ERROR: stage 2 fork() failed: " << errnoWithDescription();
            quickExit(EXIT_ABRUPT);
        } else if (child2) {
            // this is run in the middle process
            int pstat;
            cout << "forked process: " << child2 << endl;
            if (waitpid(child2, &pstat, 0) == pid_t{-1}) {
                perror("waitpid");
                quickExit(-1);
            }

            if (WIFEXITED(pstat)) {
                quickExit(WEXITSTATUS(pstat));
            }

            quickExit(51);
        }

        // this is run in the final child process (the server)

        FILE* f = freopen("/dev/null", "w", stdout);
        if (f == nullptr) {
            cout << "Cant reassign stdout while forking server process: " << strerror(errno)
                 << endl;
            return false;
        }

        f = freopen("/dev/null", "w", stderr);
        if (f == nullptr) {
            cout << "Cant reassign stderr while forking server process: " << strerror(errno)
                 << endl;
            return false;
        }

        f = freopen("/dev/null", "r", stdin);
        if (f == nullptr) {
            cout << "Cant reassign stdin while forking server process: " << strerror(errno) << endl;
            return false;
        }
    }
#endif  // !defined(_WIN32)
    return true;
}

void forkServerOrDie() {
    if (!forkServer())
        quickExit(EXIT_FAILURE);
}

// On POSIX platforms we need to set our umask before opening any log files, so this
// should depend on MungeUmask above, but not on Windows.
MONGO_INITIALIZER_GENERAL(
    ServerLogRedirection,
    ("GlobalLogManager", "EndStartupOptionHandling", "ForkServer", "MungeUmask"),
    ("default"))
(InitializerContext*) {
    using logger::LogManager;
    using logger::MessageEventEphemeral;
    using logger::MessageEventDetailsEncoder;
    using logger::MessageEventWithContextEncoder;
    using logger::MessageLogDomain;
    using logger::RotatableFileAppender;
    using logger::StatusWithRotatableFileWriter;

    // Hook up this global into our logging encoder
    MessageEventDetailsEncoder::setMaxLogSizeKBSource(gMaxLogSizeKB);

    if (serverGlobalParams.logWithSyslog) {
#ifdef _WIN32
        return Status(ErrorCodes::InternalError,
                      "Syslog requested in Windows build; command line processor logic error");
#else
        using logger::SyslogAppender;

        StringBuilder sb;
        sb << serverGlobalParams.binaryName << "." << serverGlobalParams.port;
        openlog(strdup(sb.str().c_str()), LOG_PID | LOG_CONS, serverGlobalParams.syslogFacility);
        LogManager* manager = logger::globalLogManager();
        manager->getGlobalDomain()->clearAppenders();
        manager->getGlobalDomain()->attachAppender(
            std::make_unique<SyslogAppender<MessageEventEphemeral>>(
                std::make_unique<logger::MessageEventDetailsEncoder>()));
        manager->getNamedDomain("javascriptOutput")
            ->attachAppender(std::make_unique<SyslogAppender<MessageEventEphemeral>>(
                std::make_unique<logger::MessageEventDetailsEncoder>()));
#endif  // defined(_WIN32)
    } else if (!serverGlobalParams.logpath.empty()) {
        fassert(16448, !serverGlobalParams.logWithSyslog);
        std::string absoluteLogpath =
            boost::filesystem::absolute(serverGlobalParams.logpath, serverGlobalParams.cwd)
                .string();

        bool exists;

        try {
            exists = boost::filesystem::exists(absoluteLogpath);
        } catch (boost::filesystem::filesystem_error& e) {
            return Status(ErrorCodes::FileNotOpen,
                          str::stream() << "Failed probe for \"" << absoluteLogpath << "\": "
                                        << e.code().message());
        }

        if (exists) {
            if (boost::filesystem::is_directory(absoluteLogpath)) {
                return Status(ErrorCodes::FileNotOpen,
                              str::stream() << "logpath \"" << absoluteLogpath
                                            << "\" should name a file, not a directory.");
            }

            if (!serverGlobalParams.logAppend && boost::filesystem::is_regular(absoluteLogpath)) {
                std::string renameTarget = absoluteLogpath + "." + terseCurrentTime(false);
                boost::system::error_code ec;
                boost::filesystem::rename(absoluteLogpath, renameTarget, ec);
                if (!ec) {
                    log() << "log file \"" << absoluteLogpath << "\" exists; moved to \""
                          << renameTarget << "\".";
                } else {
                    return Status(ErrorCodes::FileRenameFailed,
                                  str::stream()
                                      << "Could not rename preexisting log file \""
                                      << absoluteLogpath
                                      << "\" to \""
                                      << renameTarget
                                      << "\"; run with --logappend or manually remove file: "
                                      << ec.message());
                }
            }
        }

        StatusWithRotatableFileWriter writer = logger::globalRotatableFileManager()->openFile(
            absoluteLogpath, serverGlobalParams.logAppend);
        if (!writer.isOK()) {
            return writer.getStatus();
        }

        LogManager* manager = logger::globalLogManager();
        manager->getGlobalDomain()->clearAppenders();
        manager->getGlobalDomain()->attachAppender(
            std::make_unique<RotatableFileAppender<MessageEventEphemeral>>(
                std::make_unique<MessageEventDetailsEncoder>(), writer.getValue()));
        manager->getNamedDomain("javascriptOutput")
            ->attachAppender(std::make_unique<RotatableFileAppender<MessageEventEphemeral>>(
                std::make_unique<MessageEventDetailsEncoder>(), writer.getValue()));

        if (serverGlobalParams.logAppend && exists) {
            log() << "***** SERVER RESTARTED *****";
            Status status = logger::RotatableFileWriter::Use(writer.getValue()).status();
            if (!status.isOK())
                return status;
        }
    } else {
        logger::globalLogManager()
            ->getNamedDomain("javascriptOutput")
            ->attachAppender(std::make_unique<logger::ConsoleAppender<MessageEventEphemeral>>(
                std::make_unique<MessageEventDetailsEncoder>()));
    }

    logger::globalLogDomain()->attachAppender(
        std::make_unique<RamLogAppender>(RamLog::get("global")));

    return Status::OK();
}

/**
 * atexit handler to terminate the process before static destructors run.
 *
 * Monger server processes cannot safely call ::exit() or std::exit(), but
 * some third-party libraries may call one of those functions.  In that
 * case, to avoid static-destructor problems in the server, this exits the
 * process immediately with code EXIT_FAILURE.
 *
 * TODO: Remove once exit() executes safely in monger server processes.
 */
static void shortCircuitExit() {
    quickExit(EXIT_FAILURE);
}

MONGO_INITIALIZER(RegisterShortCircuitExitHandler)(InitializerContext*) {
    if (std::atexit(&shortCircuitExit) != 0)
        return Status(ErrorCodes::InternalError, "Failed setting short-circuit exit handler.");
    return Status::OK();
}

// On non-windows platforms, drop rwx for group and other unless the
// user has opted into using the system umask. To do so, we first read
// out the current umask (by temporarily setting it to
// no-permissions), and then or the returned umask with the
// restrictions we want to apply and set it back. The overall effect
// is to set the bits for 'other' and 'group', but leave umask bits
// bits for 'user' unaltered.
namespace {

MONGO_INITIALIZER_WITH_PREREQUISITES(MungeUmask, ("EndStartupOptionHandling"))
(InitializerContext*) {
#ifndef _WIN32
    if (!gHonorSystemUmask) {
        umask(umask(S_IRWXU | S_IRWXG | S_IRWXO) | S_IRWXG | S_IRWXO);
    }
#endif

    return Status::OK();
}
}  // namespace

bool initializeServerGlobalState(ServiceContext* service, PidFileWrite pidWrite) {
#ifndef _WIN32
    if (!serverGlobalParams.noUnixSocket && !fs::is_directory(serverGlobalParams.socket)) {
        cout << serverGlobalParams.socket << " must be a directory" << endl;
        return false;
    }
#endif

    if (!serverGlobalParams.pidFile.empty() && pidWrite == PidFileWrite::kWrite) {
        if (!writePidFile(serverGlobalParams.pidFile)) {
            // error message logged in writePidFile
            return false;
        }
    }

    return true;
}

}  // namespace monger
