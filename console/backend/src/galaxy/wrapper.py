# -*- coding:utf-8 -*-
# Copyright (c) 2015, Galaxy Authors. All Rights Reserved
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Author: wangtaize@baidu.com
# Date: 2015-03-30
import logging
from common import shell

LOG = logging.getLogger("console")

class Galaxy(object):
    def __init__(self, master_addr,bin_path):
        self.master_addr = master_addr
        self.shell_helper = shell.ShellHelper()
        self.bin_path = bin_path
    def create_task(self,url,cmd_line,replicate_count):
        add_task_command = [self.bin_path,self.master_addr,'add',url,"'%s'"%str(cmd_line),str(replicate_count)]
        code,stdout,stderr = self.shell_helper.run_with_retuncode(add_task_command)
        if code != 0 :
            return False,None
        return True,stdout
    def get_task_status(self,job_id):
        list_task_command = [self.bin_path,self.master_addr,'list',str(job_id)]
        code,stdout,stderr = self.shell_helper.run_with_retuncode(list_task_command)
        if code != 0:
            return False,[]
        lines = stdout.splitlines()
        tasklist = []
        for line in lines:
           if line.startswith("="):
               continue
           parts = line.split('\t')
           tasklist.append({"id":parts[0],"name":parts[1],"state":parts[2]})
        return True, tasklist
    def list_node(self):
        list_node_command = [self.bin_path,self.master_addr,'listnode']
        code,stdout,stderr = self.shell_helper.run_with_retuncode(list_node_command)
        if code != 0:
            return False,[]
        lines = stdout.splitlines()
        machine_list = []
        for line in lines:
            if line.startswith('='):
                continue
            parts = line.split('\t')
            if len(parts) != 5:
                continue
            machine = {}
            machine['id'] = parts[0]
            machine['host'] = parts[1]
            machine['task_num'] = parts[2].split(':')[-1]
            machine['cpu_num'] = parts[3].split(':')[-1]
            machine['mem'] = parts[4].split(':')[-1].replace('GB','')
            machine_list.append(machine)
        return True,machine_list
    def list_jobs(self):
        list_node_command = [self.bin_path,self.master_addr,'listjob']
        code,stdout,stderr = self.shell_helper.run_with_retuncode(list_node_command)
        if code != 0 :
            return False ,[]
        lines  = stdout.splitlines()
        jobs = []
        for line in lines:
            if line.startswith('='):
                continue
            parts = line.split('\t')
            if len(parts)!= 4:
                continue
            job = {}
            job['id'] = parts[0]
            job['name'] = parts[1]
            job['task_running_num'] = parts[2]
            job['replicate_num'] = parts[3]
            jobs.append(job)
        return True,jobs
