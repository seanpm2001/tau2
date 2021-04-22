/***************************************************************************
 * *   This plugin will prevent measurement of pthreads spawned during
 * *   specified timed regions.
 * *************************************************************************/

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <fstream>

#include <string>
#include <set>
#include <stack>
#include <iterator>
#include <algorithm>

#include <Profile/Profiler.h>
#include <Profile/TauAPI.h>
#include <Profile/TauPlugin.h>

static bool enabled{false};

namespace tau_plugin {

class plugin_options {
    private:
        plugin_options(void) {}
    public:
        bool env_use_selection;
        std::set<std::string> excluded_timers;
        std::set<std::string> excluded_timers_with_wildcards;
        static plugin_options& thePluginOptions() {
            static plugin_options tpo;
            return tpo;
        }
};

inline plugin_options& thePluginOptions() {
    return plugin_options::thePluginOptions();
}

void Tau_stopper_parse_environment_variables(void);
void Tau_stopper_parse_selection_file(const char * filename);
bool Tau_stopper_contains(std::set<std::string>& myset,
        const char * key, bool if_empty);

void Tau_stopper_parse_environment_variables(void) {
    char * tmp = NULL;
    tmp = getenv("TAU_PTHREAD_STOPPER_FILE");
    if (tmp != NULL) {
      Tau_stopper_parse_selection_file(tmp);
    }
}

void Tau_stopper_parse_selection_file(const char * filename) {
    std::ifstream file(filename);
    std::string str;
    bool excluding_timers = false;
    thePluginOptions().env_use_selection = true;
    while (std::getline(file, str)) {
        // trim right whitespace
        str.erase(str.find_last_not_of(" \n\r\t")+1);
        // trim left whitespace
        str.erase(0, str.find_first_not_of(" \n\r\t"));
        // skip blank lines
        if (str.size() == 0) {
            continue;
        }
        // skip comments
        if (str.find("#", 0) == 0) {
            continue;
        }
        if (str.compare("BEGIN_TIMERS") == 0) {
            excluding_timers = true;
        } else if (str.compare("END_TIMERS") == 0) {
            excluding_timers = false;
        } else {
            if (excluding_timers) {
                if (str.find("#") == std::string::npos && str.find("?") == std::string::npos) {
                    thePluginOptions().excluded_timers.insert(str);
                } else {
                    thePluginOptions().excluded_timers_with_wildcards.insert(str);
                }
            }
        }
    }
}

bool skip_timer(const char * key);

std::stack<std::string>& getStack() {
    static std::stack<std::string> theStack;
    return theStack;
}

/* This happens on Tau_start() */
int Tau_plugin_stopper_function_entry(Tau_plugin_event_function_entry_data_t* data) {
    if (!enabled) return 0;
    /* First, check to see if we are including/excluding this timer */
    if (skip_timer(data->timer_name)) {
        // set the flag for ignoring threads
        Tau_disable_pthread_tracking();
        getStack().push(data->timer_name);
    }
    return 0;
}

/* This happens on Tau_stop() */
int Tau_plugin_stopper_function_exit(Tau_plugin_event_function_exit_data_t* data) {
    if (!enabled) return 0;
    /* First, check to see if we are including/excluding this timer */
    if (skip_timer(data->timer_name)) {
        getStack().pop();
        if (getStack().size() == 0) {
            // restore the flag for ignoring threads
            Tau_enable_pthread_tracking();
        }
    }
    return 0;
}

/*This is the init function that gets invoked by the plugin mechanism inside TAU.
 * Every plugin MUST implement this function to register callbacks for various events
 * that the plugin is interested in listening to*/
extern "C" int Tau_plugin_init_func(int argc, char **argv, int id) {
    Tau_plugin_callbacks_t cb;
    TAU_VERBOSE("TAU PLUGIN pthread stopper Init\n"); fflush(stdout);
    tau_plugin::Tau_stopper_parse_environment_variables();
    /* Create the callback object */
    TAU_UTIL_INIT_TAU_PLUGIN_CALLBACKS(&cb);

    cb.FunctionEntry = Tau_plugin_stopper_function_entry;
    cb.FunctionExit = Tau_plugin_stopper_function_exit;

    /* Register the callback object */
    TAU_UTIL_PLUGIN_REGISTER_CALLBACKS(&cb, id);
    enabled = true;

    return 0;
}

// C++ program to implement wildcard
// pattern matching algorithm
// from: https://www.geeksforgeeks.org/wildcard-pattern-matching/
#if defined(__APPLE__) && defined(__clang__)
// do nothing
#else
#include <bits/stdc++.h>
#endif
using namespace std;

// Function that matches input str with
// given wildcard pattern
bool strmatch(const char str[], const char pattern[],
              int n, int m)
{
    // empty pattern can only match with
    // empty string
    if (m == 0)
        return (n == 0);

    // lookup table for storing results of
    // subproblems
    bool lookup[n + 1][m + 1]; // = {false};
	// PGI compiler doesn't like initialization during declaration...
    for (int i = 0; i <= n; i++) {
        for (int j = 0; j <= m; j++) {
            lookup[i][j] = false;
		}
	}

    // initailze lookup table to false
    //memset(lookup, false, sizeof(lookup));

    // empty pattern can match with empty string
    lookup[0][0] = true;

    // Only '#' can match with empty string
    for (int j = 1; j <= m; j++)
        if (pattern[j - 1] == '#')
            lookup[0][j] = lookup[0][j - 1];

    // fill the table in bottom-up fashion
    for (int i = 1; i <= n; i++)
    {
        for (int j = 1; j <= m; j++)
        {
            // Two cases if we see a '#'
            // a) We ignore ‘#’ character and move
            //    to next  character in the pattern,
            //     i.e., ‘#’ indicates an empty sequence.
            // b) '#' character matches with ith
            //     character in input
            if (pattern[j - 1] == '#')
                lookup[i][j] = lookup[i][j - 1] ||
                               lookup[i - 1][j];

            // Current characters are considered as
            // matching in two cases
            // (a) current character of pattern is '?'
            // (b) characters actually match
            else if (pattern[j - 1] == '?' ||
                    str[i - 1] == pattern[j - 1])
                lookup[i][j] = lookup[i - 1][j - 1];

            // If characters don't match
            else lookup[i][j] = false;
        }
    }

    return lookup[n][m];
}

} // end namespace

using namespace tau_plugin;

bool skip_timer(const char * key) {
    // are we filtering at all?
    if (!thePluginOptions().env_use_selection) {
        return false;
    }
    // check to see if this label is excluded
    if (Tau_stopper_contains(thePluginOptions().excluded_timers, key, false)) {
        return true;
    } else {
        // check to see if it's in the excluded wildcards
        for (std::set<std::string>::iterator
                it=thePluginOptions().excluded_timers_with_wildcards.begin();
             it!=thePluginOptions().excluded_timers_with_wildcards.end(); ++it) {
            if (strmatch(key, it->c_str(), strlen(key), it->length())) {
                // make the lookup faster next time
                thePluginOptions().excluded_timers.insert(key);
                return true;
            }
        }
    }
    // by default, don't skip it.
    return false;
}

