#!/bin/bash
cd_dir_1="../etc/";
cd_dir_2="/etc/";
name="file_watch";
cd_dir_3="/etc/file_watch/"
cd_dir_4="../src/client"
cd_dir_5="../hook/hook.so"
cd_dir_7="/etc/file_watch/init.conf"
eval    "cd $cd_dir_2"
eval    "sudo mkdir $name"
eval    "sudo chmod 755 $name"
eval    "cd -"
eval    "cd $cd_dir_1"
eval    "sudo cp init.conf $cd_dir_3"
eval    "sudo chmod 755 $cd_dir_7"
eval    "cd $cd_dir_4"
eval    "export LD_PRELOAD=$cd_dir_5"
eval    "make && ./inotify"
eval    "unset LD_PRELOAD"
