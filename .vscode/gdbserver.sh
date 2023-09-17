#!/bin/sh

username="root" # 用户名
password="jlink" # 密码
ip="192.168.1.123"
port_ssh="22"
port_gdbserver="2331"
path_src="./build/jlink" # 本机程序路径
path_dest="/opt/" # 目标程序路径
path_bin="/opt/jlink/bin/jlink" # 目标程序路径
path_lib="/opt/jlink/lib" # 目标链接库路径
kill_process="watch gdbserver jlink" # 需要关闭的进程

# 关闭进程
echo "kill process..."
sshpass -p ${password} ssh ${username}@${ip} -p ${port_ssh} -o StrictHostKeyChecking=no \
killall ${kill_process} 2>/dev/null; sleep 1; killall -9 ${kill_process} 2>/dev/null

# 拷贝可执行文件
echo "copy file to dest..."
# sshpass -p ${password} scp -P ${port_ssh} -r ${path_src} ${username}@${ip}:${path_dest}
sshpass -p ${password} rsync -av --delete -e "ssh -p ${port_ssh} -o StrictHostKeyChecking=no" -r ${path_src} ${username}@${ip}:${path_dest}  || exit

# 启动 gdbserver
echo "start gdbserver..."
sshpass -p ${password} ssh ${username}@${ip} -p ${port_ssh} \
"export LD_LIBRARY_PATH=${path_lib}; \
gdbserver :${port_gdbserver} ${path_bin}"  || exit
