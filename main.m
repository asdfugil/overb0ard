//
//  main.c
//  overb0ard
//
//  Created by Conrad Kramer on 3/17/15.
//  Copyright (c) 2015 Kramer Software Productions, LLC. All rights reserved.
//  Revived by Adam Tunnic on 5/17/21.
//

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>
#include <spawn.h>

#include <sys/sysctl.h>
#include "sys/kern_memorystatus.h"

#import <Foundation/Foundation.h>

#include "NSTask.h"

#define FLAG_PLATFORMIZE (1 << 1)

extern char **environ;

void patchyPatchyEggMan() {
    // Make sure setuid(0) and platformization works on Chimera or Electra.
    void *handle = dlopen("/usr/lib/libjailbreak.dylib", RTLD_LAZY);
    if (!handle) {
        return;
    }
    
    dlerror();
    typedef void (*fix_entitle_prt_t)(pid_t pid, uint32_t what);
    fix_entitle_prt_t enetitle_ptr = (fix_entitle_prt_t)dlsym(handle, "jb_oneshot_entitle_now");
    const char *dlsym_error = dlerror();
    if (dlsym_error) {
        return;
    }
    enetitle_ptr(getpid(), FLAG_PLATFORMIZE);
    
    dlerror();
    typedef void (*fix_setuid_prt_t)(pid_t pid);
    fix_setuid_prt_t setuid_ptr = (fix_setuid_prt_t)dlsym(handle,"jb_oneshot_fix_setuid_now");
    dlsym_error = dlerror();
    if (dlsym_error) {
        return;
    }
    
    setuid_ptr(getpid());
    setuid(0);
    setgid(0);
    setuid(0);
    setgid(0);
}

NSString * taskArg(const char *argument) {
    NSString *finalArg;
    if(!argument) {
        finalArg = @" ";
    } else {
        finalArg = [NSString stringWithUTF8String:argument];
    }

    return finalArg;
}

int main(int argc, const char * argv[]) {
    BOOL isMobile;
    // Make it easier for devs to use overb0ard in their code.
    if (getuid() != 0) {
        isMobile = YES;
    } else {
        // Prevent security issues... hopefully
    }
    setuid(0);
    if (getuid() != 0) {
        patchyPatchyEggMan();
        printf("Warning, UID is not 0.");
    }
    static const char *usage = "\noverb0ard, made by Doregon with love <3\nusage: %s <option> <memory> <processName>\n[-l] process - Set process limit for current run\n[-s] process - Set process limit for all runs\n[-p] process - Set the priority of the process for current run\n[-o] reverse.dns.bundle - Open an application with the set limit\n[-o] process - Open a process with the set limit\n\nSee the GitHub repository for more information.\n\n";

    static struct option opts[] = {
        {"limit", optional_argument, NULL, 'l'},
        {"priority", optional_argument, NULL, 'p'},
        {"set", optional_argument, NULL, 's'},
        {"open", optional_argument, NULL, 'o'},
        {NULL, 0, NULL, 0}
    };

    int ch;
    const char *prioritystr = NULL, *limitstr = NULL, *setstr = NULL, *openstr = NULL;
    while ((ch = getopt_long(argc, (char * const *)argv, "l:p:s:o:", opts, NULL)) != -1) {
        switch (ch) {
            case 'l':
                limitstr = optarg;
                break;
            case 'p':
                prioritystr = optarg;
                break;
            case 's':
                setstr = optarg;
                break;
            case 'o':
                openstr = optarg;
                break;
        }
    }

    const char *overb0ard = argv[0];
    argc -= optind;
    argv += optind;

    if (argc == 0 || (prioritystr == NULL && limitstr == NULL && setstr == NULL && openstr == NULL)) {
        fprintf(stderr, usage, overb0ard);
        return 1;
    }

    char *endptr;
    const char *process = argv[0];
    pid_t pid = strtoimax(process, &endptr, 0);
    if (process == endptr || *endptr != '\0') {
        int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};

        size_t size;
        if (sysctl(mib, 4, NULL, &size, NULL, 0) == -1) {
            if (openstr == NULL && setstr == NULL) {
                fprintf(stderr, "%s: error: %s\n", overb0ard, strerror(errno));
                return 1;
            }
        }

        struct kinfo_proc *processes = malloc(size);
        if (sysctl(mib, 4, processes, &size, NULL, 0) == -1) {
            if (openstr == NULL && setstr == NULL) {
                fprintf(stderr, "%s: error: %s\n", overb0ard, strerror(errno));
                return 1;
            }
        }

        for (unsigned long i = 0; i < size / sizeof(struct kinfo_proc); i++) {
            if (strcmp(processes[i].kp_proc.p_comm, process) == 0) {
                pid = processes[i].kp_proc.p_pid;
                break;
            }
        }

        if (pid == 0) {
            if (openstr == NULL && setstr == NULL) {
                fprintf(stderr, "%s: error: %s\n", overb0ard, strerror(ESRCH));
                return 1;
            }
        }
    }

    if (limitstr) {
        uint32_t limit = strtoimax(limitstr, &endptr, 0);
        if (limitstr == endptr || *endptr != '\0') {
            fprintf(stderr, "%s is not a valid limit for the process %s.\n", limitstr, process);
            return 1;
        }

        if (memorystatus_control(MEMORYSTATUS_CMD_SET_JETSAM_TASK_LIMIT, pid, limit, NULL, 0) == -1) {
            fprintf(stderr, "%s exited with an error: %s\n", overb0ard, strerror(errno));
            return 1;
        }
        printf("The limit of %s was set to %s megabytes sucessfully.\n", process, limitstr);
    }

    if (prioritystr) {
        uint32_t priority = strtoimax(prioritystr, &endptr, 0);
        if (prioritystr == endptr || *endptr != '\0') {
            fprintf(stderr, "%s is not a valid priority.\n", prioritystr);
            return 1;
        }

        memorystatus_priority_entry_t properties;
        memset(&properties, 0, sizeof(memorystatus_priority_entry_t));
        properties.pid = pid;
        properties.priority = priority;

        if (memorystatus_control(MEMORYSTATUS_CMD_GRP_SET_PROPERTIES, 0, 0, &properties, sizeof(memorystatus_priority_entry_t)) == -1) {
            fprintf(stderr, "%s exited with an error:  %s\n", overb0ard, strerror(errno));
            return 1;
        }
        printf("The priority of %s was set to %s successfully.\n", process, prioritystr);
    }
    
    if (setstr) {
    //  This is where the persistent stuff will go.
        printf("The \"set\" option is not yet available.\nIt will be released in version 1.2\n");
        return 1;
    }

    if (openstr) {
        uint32_t open = strtoimax(openstr, &endptr, 0);
        //printf("%s", process);
        //char *ret;
        //ret = strstr(process, ".");
        //if (ret) {
        // TODO: launching an application from command line with custom memory limit
        //} else {
        int argCount = 1;
        NSString *launchArgs_pre = @"";
        launchArgs_pre = [launchArgs_pre stringByAppendingString:[NSString stringWithFormat:@"%@", [NSString stringWithUTF8String:process]]];
        NSMutableArray *arguments = [[NSMutableArray alloc] init];
        [arguments addObject:@"-c"];
        do {
            if(!(argv[argCount] == 0)) {
                NSString *tempStr = [NSString stringWithFormat:@" %@", [NSString stringWithUTF8String:argv[argCount]]];
                launchArgs_pre = [launchArgs_pre stringByAppendingString:tempStr];
                argCount++;
            } else {
                break;
            }
        } while (argCount < 1000);
        NSTask *task = [[NSTask alloc] init];
        [task setStandardInput:[NSPipe pipe]];
        [task setLaunchPath:@"/bin/bash"];
        const char *launchArgs = [launchArgs_pre cStringUsingEncoding:NSUTF8StringEncoding];
        [arguments addObject:[NSString stringWithFormat:@"%@", [NSString stringWithUTF8String:launchArgs]]];
        [task setArguments:arguments];

        if (openstr == endptr || *endptr != '\0') {
            fprintf(stderr, "%s is not a valid limit for the process %s.\n", openstr, process);
            return 1;
        }

        [task launch];

        printf("Waiting 5 seconds to set memory limit.\n");
        sleep(5);

        if (memorystatus_control(MEMORYSTATUS_CMD_SET_JETSAM_TASK_LIMIT, pid, open, NULL, 0) == -1) {
            fprintf(stderr, "%s was unable to start the program and exited with an error:  %s\n", overb0ard, strerror(errno));
            return 1;
        } else if (memorystatus_control(MEMORYSTATUS_CMD_SET_JETSAM_TASK_LIMIT, pid, open, NULL, 0) == 0) {
        }

        [task waitUntilExit];
        }
        //}
    return 0;
}
