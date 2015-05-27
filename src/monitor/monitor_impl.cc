/***************************************************************************
 * 
 * Copyright (c) 2015 Baidu.com, Inc. All Rights Reserved
 * $Id$ 
 * 
 **************************************************************************/
 
 /**
 * @file monitor_impl.cc
 * @author zhoushiyong(zhoushiyong@baidu.com)
 * @date 2015/05/25 21:17:32
 * @version $Revision$ 
 * @brief 
 *  
 **/

#include <fstream>
#include <sstream>
#include <time.h>
#include "common/asm_atomic.h"
#include "common/logging.h"
#include "monitor_impl.h"

namespace galaxy {
MonitorImpl::MonitorImpl() {
    msg_forbit_time_ = 60;       
}
MonitorImpl::~MonitorImpl() {
    std::map<std::string, Watch*>::iterator watch_it;
    for (watch_it == watch_map_.begin(); watch_it != watch_map_.end(); watch_it++) {
        delete watch_it->second;
    }
    
    std::map<std::string, Trigger*>::iterator trigger_it;
    for (trigger_it == trigger_map_.begin(); trigger_it != trigger_map_.end();
            trigger_it++) {
        delete trigger_it->second;
    }

    std::map<std::string, Action*>::iterator action_it;
    for (action_it == action_map_.begin(); action_it != action_map_.end(); 
            action_it++) {
        delete action_it->second;
    }

    std::vector<Rule*>::iterator rule_it;
    for (rule_it == rule_list_.begin(); rule_it != rule_list_.end();
            rule_it++) {
        delete *rule_it;
    }
    return;
}

void MonitorImpl::Split(std::string& src, std::string& delim, std::vector<std::string>* ret)
{
    size_t last = 0;  
    size_t index=src.find_first_of(delim, last);  
    while (index!=std::string::npos) {  
        ret->push_back(src.substr(last,index-last));
        last=index+1;  
        index=src.find_first_of(delim,last);  
    }  
    if (index-last>0) {  
        ret->push_back(src.substr(last,index-last));  
    }
    return;
}
bool MonitorImpl::ParseConfig(const std::string conf_path)
{
    std::ifstream fin(conf_path.c_str());
    if (!fin) {
        LOG(WARNING, "open conf_path err %s", conf_path.c_str());
        return false;
    }
    std::string line;
    char value[1024];
    while (getline(fin, line)) {
        if (sscanf(line.c_str(), "<input>: %s", value)) {
            log_path = std::string(value);
        }
        if (sscanf(line.c_str(), "<watch>: %s", value)) {
            std::vector<std::string> args;
            std::string input(value);
            std::string delim("|");
            Split(input, delim, &args);
            Watch *watch_ptr = new Watch();
            watch_ptr->item_name.assign(args[1]);
            watch_ptr->regex.assign(args[2]);
            watch_ptr->reg.assign(args[2]);
            watch_ptr->count = 0;
            watch_map_[args[0]] = watch_ptr;
        } else if (sscanf(line.c_str(), "<trigger>: %s", value)) {
            std::vector<std::string> args;
            std::string input(value);
            std::string delim("|");
            Split(input, delim, &args);
            Trigger* trigger_ptr = new Trigger();
            trigger_ptr->item_name.assign(args[0]);
            trigger_ptr->threadhold = atoi(args[2].c_str());
            trigger_ptr->relate.assign(args[1]);
            trigger_ptr->range = atoi(args[3].c_str());
            trigger_ptr->timestamp = time(NULL);
            trigger_map_[args[0]] = trigger_ptr;
        } else if (sscanf(line.c_str(), "<action>: %s", value)) {
            std::vector<std::string> args;
            std::string input(value);
            std::string delim("|");
            Split(input, delim, &args);
            Action* action_ptr = new Action();
            action_ptr->title.assign(args[2]);
            action_ptr->content.assign(args[3]);
            action_ptr->timestamp = time(NULL);
            delim.assign(":");
            Split(args[1], delim, &(action_ptr->to_list));
            action_map_[args[0]] = action_ptr;
        } else if (sscanf(line.c_str(), "<rule>: %s", value)) {
            std::vector<std::string> args;
            std::string input(value);
            std::string delim("|");
            Split(input, delim, &args);
            Rule* rule_ptr = new Rule();
            rule_ptr->watch = watch_map_.find(args[0])->second;
            rule_ptr->trigger = trigger_map_.find(args[1])->second;
            rule_ptr->action = action_map_.find(args[2])->second;
            rule_list_.push_back(rule_ptr);
        }
    }
    return true;
}

void MonitorImpl::Run()
{
    std::ifstream fin(log_path.c_str());
    std::string line;
    running_ = true;
    size_t seek;
    while (running_) {
        if (fin.peek() == EOF) {  
            fin.clear();  
            fin.seekg(seek, std::ios::beg);  
            sleep(3);  
            continue;  
        }
        getline(fin, line);
        seek = fin.tellg();
        ExecRule(std::string(line));
    }
    fin.close();
    return;
}
bool MonitorImpl::ExecRule(std::string src)
{
    for (std::vector<Rule*>::iterator it = rule_list_.begin();
            it != rule_list_.end(); it++) {
        if (!Matching(src, (*it)->watch)) {
            continue;
        }
        if (!Judging(&((*it)->watch->count), (*it)->trigger)) {
            continue;
        }
        if (!Treating((*it)->action)) {
            continue;
        }
    }
    return true;
}

bool MonitorImpl::Matching(std::string src, Watch* watch) 
{
    assert(watch != NULL);
    boost::cmatch mat;
    if (boost::regex_search(src.c_str(), mat, watch->reg)) {
        common::atomic_inc(&watch->count);
        return true;
    }
    return false;
}

bool MonitorImpl::Judging( int* cnt, Trigger* trigger)
{
    assert(trigger != NULL);
    bool ret = false;
    if (trigger->relate == "<") {
        ret = (*cnt < trigger->threadhold);
    } else if (trigger->relate == std::string("=")) {
        ret = (*cnt == trigger->threadhold);
    } else if (trigger->relate == std::string(">")) {
        ret = (*cnt > trigger->threadhold);
    } else {
        LOG(WARNING, "unsupported relate %s", trigger->relate.c_str());       
    }
    if (time(NULL) - trigger->timestamp >= trigger->range) {
        common::atomic_swap(cnt, 0);
        trigger->timestamp = time(NULL);
        return ret;
    }
    return false;
}

bool MonitorImpl::Treating(Action* act) {
    assert(act != NULL);
    if ((time(NULL) - act->timestamp) < msg_forbit_time_) {
        return true;
    }
    for (std::vector<std::string>::iterator it = act->to_list.begin();
            it != act->to_list.end(); it++) { 
        std::string cmd = std::string("/home/galaxy/agent/galaxy/tools/gsmsend ")
            + std::string("-s emp01.baidu.com:15001 -semp02.baidu.com:15001 ") 
            + *it + "@" + act->title + ":" + act->content;
        if (0 != system(cmd.c_str())) {
            LOG(WARNING, "gsmsend msg err %s", cmd.c_str());
        }
        act->timestamp = time(NULL);
    }
    return true;
}
 
}


/* vim: set ts=4 sw=4 sts=4 tw=100 */
