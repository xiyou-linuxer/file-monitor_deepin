#!/bin/bash
cd_dir_1="../../etc/";
cd_dir_2="/etc/";
name = "file_watch";
cd_dir_3 = "/etc/file_watch/"
cd_dir_4 = "src/client"
eval    "cd $cd_dir_2"
eval    "mkdir $name"
eval    "cd -"
eval    "cd $cd_dir_1"
eval    "cp init.conf $cd_dir_3"
eval    "cd $cd_dir_4"
eval    "make && ./inotify"