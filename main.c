#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>
#include <spawn.h>
#include <TargetConditionals.h>

#include <sys/sysctl.h>

#define PRIVATE 1
#include <sys/kern_memorystatus.h>

void usage(const char* overb0ard) {
    fprintf(stderr,
    "usage: %s [-l limit] [-p priority] "
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR && !TARGET_OS_MACCATALYST
            "[-P probability] [-f freezability]"
#endif
            " [-m management state] [-I] <Process name or PID>\n"
    "-l, --limit <limit>\t\tSet fatal process memory limit in MiB\n"
    "-M, --high-water-mark <limit>\tSet process memory high water mark\n"
    "-p, --priority <priority>\tSet process priority\n"
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR && !TARGET_OS_MACCATALYST
    "-P, --probability <0|1>\t\tSet process use probability (0 = unlikely, 1 = likely)\n"
    "-f, --freezability <true|false>\tSet whether process is freezable\n"
#endif
    "-m, --managed <true|false>\tSet whether process is managed\n"
    "-I, --process-info\t\tGet process memory information\n"
    , overb0ard);
}

int main(int argc, char* argv[]) {
    struct option opts[] = {
        {"limit", required_argument, NULL, 'l'},
        {"high-water-mark", required_argument, NULL, 'M'},
        {"priority", required_argument, NULL, 'p'},
        {"probability", required_argument, NULL, 'P'},
        {"freezability", required_argument, NULL, 'f'},
        {"managed", required_argument, NULL, 'm'},
        {"process-info", no_argument, NULL, 'I'},
        {NULL, 0, NULL, 0}
    };

    int ch;
    bool info = false;
    const char *prioritystr = NULL, *limitstr = NULL, *probabilitystr = NULL, 
    *freezabilitystr = NULL, *managedstr = NULL, *watermarkstr = NULL;
    while ((ch = getopt_long(argc, (char * const *)argv, "l:p:P:f:Im:M:", opts, NULL)) != -1) {
        switch (ch) {
            case 'l':
                limitstr = optarg;
                break;
            case 'p':
                prioritystr = optarg;
                break;
            case 'P':
                probabilitystr = optarg;
                break;
            case 'f':
                freezabilitystr = optarg;
                break;
            case 'm':
                managedstr = optarg;
                break;
            case 'M':
                watermarkstr = optarg;
                break;
            case 'I':
                info = true;
                break;
        }
    }

    const char *overb0ard = argv[0];
    argc -= optind;
    argv += optind;

    if (argc == 0) {
        fprintf(stderr, "%s: No process specified\n", overb0ard);
        usage(overb0ard);
        return 1;
    }

    // Parse pid or get from process name
    char *endptr;
    const char *process = argv[0];
    pid_t pid = (pid_t)strtoimax(process, &endptr, 0);
    if (process == endptr || *endptr != '\0') {
        int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};

        size_t size;
        if (sysctl(mib, 4, NULL, &size, NULL, 0) == -1) {
            fprintf(stderr, "%s: error: %s\n", overb0ard, strerror(errno));
            return 1;
        }

        struct kinfo_proc *processes = malloc(size);
        if (!processes) {
            fprintf(stderr, "%s: error: %s\n", overb0ard, strerror(ENOMEM));
            return -1;
        }

        if (sysctl(mib, 4, processes, &size, NULL, 0) == -1) {
            fprintf(stderr, "%s: error: %s\n", overb0ard, strerror(errno));
            free(processes);
            return 1;
        }

        for (unsigned long i = 0; i < size / sizeof(struct kinfo_proc); i++) {
            if (strcmp(processes[i].kp_proc.p_comm, process) == 0) {
                pid = processes[i].kp_proc.p_pid;
                break;
            }
        }
        
        free(processes);

        if (pid == 0) {
            fprintf(stderr, "%s: error: %s\n", overb0ard, strerror(ESRCH));
            return 1;
        }
    }

    if (managedstr) {
        if (__builtin_available(macOS 10.14, iOS 12.0, tvOS 12.0, watchOS 5.0, bridgeOS 3.0, *)) {
            int managed = -1;
            if (strcasecmp(managedstr, "true") == 0) managed = 1;
            else if (strcasecmp(managedstr, "false") == 0) managed = 0;
            else {
                managed = (int)strtoimax(managedstr, &endptr, 0);
            }
            if (managed != 1 && managed != 0) {
                fprintf(stderr, "%s is not a valid management state\n", freezabilitystr);
                return 1;
            }

            if (memorystatus_control(MEMORYSTATUS_CMD_SET_PROCESS_IS_MANAGED, pid, (uint32_t)managed, NULL, 0) == -1) {
                fprintf(stderr, "memorystatus_control(MEMORYSTATUS_CMD_SET_PROCESS_IS_MANAGED) error: %d: %s\n", errno, strerror(errno));
                return 1;
            }
            printf("Process %s is now %s.\n", process, managed ? "managed" : "unmanaged");
        } else {
            fprintf(stderr, "Setting management state is not supported on this OS\n");
        }
    }
    
    if (limitstr) {
        uint32_t limit = (uint32_t)strtoimax(limitstr, &endptr, 0);
        bool set = false;
        if (limitstr == endptr || *endptr != '\0') {
            fprintf(stderr, "%s is not a valid limit for the process %s.\n", limitstr, process);
            return 1;
        }
#if TARGET_OS_OSX || TARGET_OS_SIMULATOR || TARGET_OS_MACCATALYST
        if (__builtin_available(macOS 10.11, *)) {
            memorystatus_memlimit_properties_t props;
            memset(&props, '\0', sizeof(props));
            props.memlimit_active = limit;
            props.memlimit_active_attr = MEMORYSTATUS_MEMLIMIT_ATTR_FATAL;
            props.memlimit_inactive = limit;
            props.memlimit_active_attr = MEMORYSTATUS_MEMLIMIT_ATTR_FATAL;
            if (memorystatus_control(MEMORYSTATUS_CMD_SET_MEMLIMIT_PROPERTIES, pid, 0, &props, sizeof(props)) == -1) {
                fprintf(stderr, "memorystatus_control(MEMORYSTATUS_CMD_SET_MEMLIMIT_PROPERTIES) error: %d: %s\n", errno, strerror(errno));
                return 1;
            }
        }
#else
        if (__builtin_available(iOS 8.0, tvOS 9.0, watchOS 1.0, bridgeOS 1.0, *)) {
            if (memorystatus_control(MEMORYSTATUS_CMD_SET_JETSAM_TASK_LIMIT, pid, limit, NULL, 0) == -1) {
                fprintf(stderr, "memorystatus_control(MEMORYSTATUS_CMD_SET_JETSAM_TASK_LIMIT) error: %d: %s\n", errno, strerror(errno));
                return 1;
            }
            set = true;
        }
#endif
        else {
            fprintf(stderr, "Setting task memory limit is not supported on this OS\n");
        }

        if (set) printf("The limit of %s was set to %s MiB sucessfully.\n", process, limitstr);
    }
    
    if (watermarkstr) {
        uint32_t watermark = (uint32_t)strtoimax(watermarkstr, &endptr, 0);
        bool set = false;
        if (watermarkstr == endptr || *endptr != '\0') {
            fprintf(stderr, "%s is not a valid high water mark for the process %s.\n", watermarkstr, process);
            return 1;
        }

#if TARGET_OS_OSX || TARGET_OS_SIMULATOR || TARGET_OS_MACCATALYST
        if (__builtin_available(macOS 10.11, *)) {
            memorystatus_memlimit_properties_t props;
            memset(&props, '\0', sizeof(props));
            props.memlimit_active = watermark;
            props.memlimit_active_attr = 0;
            props.memlimit_inactive = watermark;
            props.memlimit_active_attr = 0;
            if (memorystatus_control(MEMORYSTATUS_CMD_SET_MEMLIMIT_PROPERTIES, pid, 0, &props, sizeof(props)) == -1) {
                fprintf(stderr, "memorystatus_control(MEMORYSTATUS_CMD_SET_MEMLIMIT_PROPERTIES) error: %d: %s\n", errno, strerror(errno));
                return 1;
            }
            set = 1;
        }
#else
        if (__builtin_available(iOS 7.0, tvOS 9.0, watchOS 1.0, bridgeOS 1.0, *)) {
            if (memorystatus_control(MEMORYSTATUS_CMD_SET_JETSAM_HIGH_WATER_MARK, pid, watermark, NULL, 0) == -1) {
                fprintf(stderr, "memorystatus_control(MEMORYSTATUS_CMD_SET_JETSAM_HIGH_WATER_MARK) error: %d: %s\n", errno, strerror(errno));
                return 1;
            }
            set = true;
        }
#endif
        else {
            fprintf(stderr, "Setting jetsam task high water mark is not supported on this OS\n");
        }

        if (set) printf("The high water mark of %s was set to %s MiB sucessfully.\n", process, watermarkstr);
    }

    if (prioritystr) {
        int priority = (int)strtoimax(prioritystr, &endptr, 0);
        bool set = false;
        if (prioritystr == endptr || *endptr != '\0') {
            fprintf(stderr, "%s is not a valid priority.\n", prioritystr);
            return 1;
        }

        if (__builtin_available(macOS 10.10, iOS 8.0, tvOS 9.0, watchOS 1.0, bridgeOS 1.0, *)) {
            memorystatus_properties_entry_v1_t properties;
            memset(&properties, 0, sizeof(memorystatus_properties_entry_v1_t));
            properties.pid = pid;
            properties.priority = priority;
            properties.version = MEMORYSTATUS_MPE_VERSION_1;
            
            if (memorystatus_control(MEMORYSTATUS_CMD_GRP_SET_PROPERTIES, 0, MEMORYSTATUS_FLAGS_GRP_SET_PRIORITY, &properties, sizeof(memorystatus_properties_entry_v1_t)) == -1) {
                fprintf(stderr, "memorystatus_control(MEMORYSTATUS_CMD_GRP_SET_PROPERTIES) error: %d: %s\n", errno, strerror(errno));
                return 1;
            }
            set = true;
        } else if (__builtin_available(macOS 10.9, iOS 7.0, tvOS 9.0, watchOS 1.0, bridgeOS 1.0, *)) {
            memorystatus_priority_properties_t prop;
            memset(&prop, 0, sizeof(memorystatus_priority_properties_t));
            prop.user_data = 0;
            prop.priority = priority;
            if (memorystatus_control(MEMORYSTATUS_CMD_SET_PRIORITY_PROPERTIES, pid, 0, &prop, sizeof(memorystatus_priority_properties_t)) == -1) {
                fprintf(stderr, "memorystatus_control(MEMORYSTATUS_CMD_SET_PRIORITY_PROPERTIES) error: %d: %s\n", errno, strerror(errno));
                return 1;
            }
            set = true;
        } else {
            fprintf(stderr, "Setting priority is not supported on this OS\n");
        }
        if (set) printf("The priority of %s was set to %s successfully.\n", process, prioritystr);
    }

    if (probabilitystr) {
        if (__builtin_available(macOS 10.15, iOS 13.0, tvOS 13.0, watchOS 6.0, bridgeOS 4.0, *)) {
            int probability = (int)strtoimax(probabilitystr, &endptr, 0);
            if (probabilitystr == endptr || *endptr != '\0' || (probability != 0 && probability != 1)) {
                fprintf(stderr, "%s is not a valid probability.\n", probabilitystr);
                return 1;
            }
            
            memorystatus_properties_entry_v1_t properties;
            memset(&properties, 0, sizeof(memorystatus_properties_entry_v1_t));
            properties.pid = pid;
            properties.use_probability = probability;
            properties.version = MEMORYSTATUS_MPE_VERSION_1;
            
            if (memorystatus_control(MEMORYSTATUS_CMD_GRP_SET_PROPERTIES, 0, MEMORYSTATUS_FLAGS_GRP_SET_PROBABILITY, &properties, sizeof(memorystatus_properties_entry_v1_t)) == -1) {
                fprintf(stderr, "memorystatus_control(MEMORYSTATUS_CMD_GRP_SET_PROPERTIES) error: %d: %s\n", errno, strerror(errno));
                return 1;
            }
            printf("The usage probability of %s was set to %s successfully.\n", process, probability ? "likely" : "unlikely");
        } else {
            fprintf(stderr, "Setting usage probabilties is not supported on this OS\n");
        }
    }

    if (freezabilitystr) {
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR && !TARGET_OS_MACCATALYST
        if (__builtin_available(iOS 12.0, tvOS 12.0, watchOS 5.0, bridgeOS 3.0, *)) {
            int freezability = -1;
            if (strcasecmp(freezabilitystr, "true") == 0) freezability = 1;
            else if (strcasecmp(freezabilitystr, "false") == 0) freezability = 0;
            else {
                freezability = (int)strtoimax(freezabilitystr, &endptr, 0);
            }
            if (freezability != 1 && freezability != 0) {
                fprintf(stderr, "%s is not a valid freezability\n", freezabilitystr);
                return 1;
            }
            
            if (__builtin_available(macOS 10.15, iOS 13.0, tvOS 13.0, watchOS 6.0, bridgeOS 4.0, *)) {}
            else if (pid != getpid()) {
                fprintf(stderr, "Warning: setting freezability is only allowed on self on your OS\n");
            }

            if (memorystatus_control(MEMORYSTATUS_CMD_SET_PROCESS_IS_FREEZABLE, pid, (uint32_t)freezability, NULL, 0) == -1) {
                fprintf(stderr, "memorystatus_control(MEMORYSTATUS_CMD_SET_PROCESS_IS_FREEZABLE) error: %d: %s\n", errno, strerror(errno));
                return 1;
            }
            printf("Process %s is now %s.\n", process, freezability ? "freezable" : "unfreezable");
        } else
#endif
        {
            fprintf(stderr, "Setting freezability is not supported on this OS\n");
        }
    }

    if (info) {
        int managed = 0;
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR && !TARGET_OS_MACCATALYST
        int freezable = 0, swappable = 0, frozen = 0;
#endif
        uint64_t excess_footprint = 0;
        memorystatus_memlimit_properties2_t memlimit_prop;

        if (__builtin_available(macOS 10.14, iOS 12.0, tvOS 12.0, watchOS 5.0, bridgeOS 3.0, *)) {
            if ((managed = memorystatus_control(MEMORYSTATUS_CMD_GET_PROCESS_IS_MANAGED, pid, 0, NULL, 0)) == -1) {
                fprintf(stderr, "memorystatus_control(MEMORYSTATUS_CMD_GET_PROCESS_IS_MANAGED) error: %d: %s\n", errno, strerror(errno));
                return 1;
            }
        }

#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR && !TARGET_OS_MACCATALYST
        if (__builtin_available(macOS 12.0, iOS 15.0, tvOS 15.0, watchOS 8.0, bridgeOS 6.0, *)) {
            if (pid == getpid()) {
                if ((freezable = memorystatus_control(MEMORYSTATUS_CMD_GET_PROCESS_IS_FREEZABLE, pid, 0, NULL, 0)) == -1) {
                    fprintf(stderr, "memorystatus_control(MEMORYSTATUS_CMD_GET_PROCESS_IS_FREEZABLE) error: %d: %s\n", errno, strerror(errno));
                    return 1;
                }
                
                if ((frozen = memorystatus_control(MEMORYSTATUS_CMD_GET_PROCESS_IS_FROZEN, pid, frozen, NULL, 0)) == -1) {
                    fprintf(stderr, "memorystatus_control(MEMORYSTATUS_CMD_GET_PROCESS_IS_FROZEN) error: %d: %s\n", errno, strerror(errno));
                    return 1;
                }
            }
        }

        if (__builtin_available(iOS 16.0, tvOS 16.0, watchOS 9.0, bridgeOS 7.0, *)) {
            if ((swappable = memorystatus_control(MEMORYSTATUS_CMD_GET_PROCESS_COALITION_IS_SWAPPABLE, pid, swappable, NULL, 0)) == -1) {
                fprintf(stderr, "memorystatus_control(MEMORYSTATUS_CMD_GET_PROCESS_COALITION_IS_SWAPPABLE) error: %d: %s\n", errno, strerror(errno));
                return 1;
            }
        }
#endif
        

        if (__builtin_available(macOS 10.15, iOS 13.0, tvOS 13.0, watchOS 6.0, bridgeOS 4.0, *)) {
            if (memorystatus_control(MEMORYSTATUS_CMD_GET_MEMLIMIT_PROPERTIES, pid, 0, &memlimit_prop, sizeof(memlimit_prop)) == -1) {
                fprintf(stderr, "memorystatus_control(MEMORYSTATUS_CMD_GET_MEMLIMIT_PROPERTIES) error: %d: %s\n", errno, strerror(errno));
                return 1;
            }
        } else if (__builtin_available(macOS 10.11, iOS 9.0, tvOS 9.0, watchOS 2.0, bridgeOS 1.0, *)) {
            if (memorystatus_control(MEMORYSTATUS_CMD_GET_MEMLIMIT_PROPERTIES, pid, 0, &memlimit_prop.v1, sizeof(memlimit_prop.v1)) == -1) {
                fprintf(stderr, "memorystatus_control(MEMORYSTATUS_CMD_GET_MEMLIMIT_PROPERTIES) error: %d: %s\n", errno, strerror(errno));
                return 1;
            }
            if (memorystatus_control(MEMORYSTATUS_CMD_GET_MEMLIMIT_EXCESS, pid, 0, &excess_footprint, sizeof(excess_footprint)) == -1) {
                fprintf(stderr, "memorystatus_control(MEMORYSTATUS_CMD_GET_MEMLIMIT_EXCESS) error: %d: %s\n", errno, strerror(errno));
                return 1;
            }
        }
        
        if (__builtin_available(macOS 10.11, iOS 9.0, tvOS 9.0, watchOS 2.0, bridgeOS 1.0, *)) {
            printf(
                   "Active memory limit          : %" PRId32 " MiB (%s)\n"
                   "Inactive memory limit        : %" PRId32 " MiB (%s)\n"
                   "Physical footprint excess    : %" PRIu64 " B \n"
                   ,
                   memlimit_prop.v1.memlimit_active,
                   (memlimit_prop.v1.memlimit_active_attr & MEMORYSTATUS_MEMLIMIT_ATTR_FATAL) ? "fatal" : "non-fatal",
                   memlimit_prop.v1.memlimit_inactive,
                   (memlimit_prop.v1.memlimit_inactive_attr & MEMORYSTATUS_MEMLIMIT_ATTR_FATAL) ? "fatal" : "non-fatal",
                   excess_footprint
                   );
        }
        
        if (__builtin_available(macOS 10.15, iOS 13.0, tvOS 13.0, watchOS 6.0, bridgeOS 4.0, *)) {
            printf(
                   "Memory limit increase        : %" PRIu32 " MiB\n"
                   "Memory limit increase bytes  : %" PRIu32 "\n"
                   ,
                   memlimit_prop.memlimit_increase,
                   memlimit_prop.memlimit_increase_bytes
                   );
        }
        
        
        if (__builtin_available(macOS 10.14, iOS 12.0, tvOS 12.0, watchOS 5.0, bridgeOS 3.0, *)) {
            printf("Managed                      : %s\n", managed ? "true" : "false");
        }
        
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR && !TARGET_OS_MACCATALYST
        if (__builtin_available(macOS 12.0, iOS 15.0, tvOS 15.0, watchOS 8.0, bridgeOS 6.0, *)) {
            if (pid == getpid()) {
                printf("Frozen                       : %s\n" , frozen ? "true" : "false");
                printf("Freezable                    : %s\n", freezable ? "true" : "false");
            }
        }

        if (__builtin_available(iOS 16.0, tvOS 16.0, watchOS 9.0, bridgeOS 7.0, *)) {
            printf("Swappable                    : %s\n" , swappable ? "true" : "false");
        }
#endif
        
    }

    return 0;
}
