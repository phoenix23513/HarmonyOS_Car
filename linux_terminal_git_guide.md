# Linux 终端常用操作与 Git 实战指南

> 适用于 Linux、Ubuntu 和 WSL 初学者。本文主要讲解交互式终端、Linux 日常命令以及 Git/GitHub 工作流。示例默认在 Bash 中执行。

## 目录

- [1. 终端、Shell 与命令](#1-终端shell-与命令)
- [2. 高效操作命令行](#2-高效操作命令行)
- [3. 路径、文件与目录](#3-路径文件与目录)
- [4. 查看、搜索和处理文本](#4-查看搜索和处理文本)
- [5. 管道、重定向与命令组合](#5-管道重定向与命令组合)
- [6. 权限、用户与 sudo](#6-权限用户与-sudo)
- [7. 进程、任务、服务与日志](#7-进程任务服务与日志)
- [8. 系统、磁盘与资源](#8-系统磁盘与资源)
- [9. 软件安装、压缩与解压](#9-软件安装压缩与解压)
- [10. 网络、SSH 与文件传输](#10-网络ssh-与文件传输)
- [11. 环境变量与程序执行](#11-环境变量与程序执行)
- [12. Git 的核心模型](#12-git-的核心模型)
- [13. Git 初始化、添加与提交](#13-git-初始化添加与提交)
- [14. Git 历史、差异与文件追踪](#14-git-历史差异与文件追踪)
- [15. Git 分支、合并与冲突](#15-git-分支合并与冲突)
- [16. Git 撤销、恢复与暂存](#16-git-撤销恢复与暂存)
- [17. Git 远程仓库与 GitHub](#17-git-远程仓库与-github)
- [18. .gitignore 与 .gitattributes](#18-gitignore-与-gitattributes)
- [19. 完整实战工作流](#19-完整实战工作流)
- [20. 常见问题与排查](#20-常见问题与排查)
- [21. 命令速查表](#21-命令速查表)
- [22. 官方参考资料](#22-官方参考资料)

---

## 1. 终端、Shell 与命令

### 1.1 它们分别是什么

- **终端（Terminal）**：显示文字并接收键盘输入的窗口。
- **Shell**：读取命令、处理语法并启动程序的命令解释器。
- **Bash**：Linux 上常见的 Shell 之一。
- **命令**：Shell 内置功能、别名、函数或可执行程序。

查看当前 Shell：

```bash
echo "$SHELL"
ps -p $$ -o comm=
```

第一条通常显示登录 Shell，第二条更接近当前实际运行的 Shell。

### 1.2 命令的基本结构

```bash
ls -lah /var/log
```

| 部分 | 示例 | 含义 |
| --- | --- | --- |
| 命令名 | `ls` | 要运行的命令 |
| 选项 | `-lah` | 改变命令行为 |
| 参数 | `/var/log` | 命令要处理的对象 |

短选项常以单个减号开头，多个短选项常可合并：

```bash
ls -l -a -h
ls -lah
```

长选项通常以两个减号开头：

```bash
ls --all --human-readable
```

### 1.3 命令是什么类型

```bash
type cd
type ls
type -a python
command -v git
```

- `type` 能识别别名、Shell 内置命令、函数和外部程序。
- `command -v` 适合脚本中检查命令是否存在。
- `which` 主要在 `PATH` 中查找外部程序，对 Shell 内置命令不够可靠。

### 1.4 如何获取帮助

```bash
type read
help read
ls --help
man ls
info coreutils
```

建议顺序：

1. 先用 `type` 确认命令类型。
2. Bash 内置命令使用 `help`。
3. 外部程序先看 `--help`。
4. 需要完整说明时使用 `man`。
5. GNU 工具的更完整文档可使用 `info`。

`man` 阅读器中常用按键：

| 按键 | 作用 |
| --- | --- |
| `Space` / `PageDown` | 向下翻页 |
| `b` / `PageUp` | 向上翻页 |
| `/text` | 向下搜索 |
| `n` / `N` | 下一个/上一个匹配 |
| `q` | 退出 |

---

## 2. 高效操作命令行

下面的操作多数由 Bash 的 GNU Readline 功能提供。不同 Shell、终端或自定义键位可能略有差异。

### 2.1 Tab 自动补全

输入一部分内容后按 `Tab`，Shell 会尝试补全命令、文件名或目录名。

```bash
cd /etc/sys<Tab>
git sta<Tab>
```

常见行为：

- 只有一个匹配时，直接补全。
- 有多个匹配时，先补到它们共有的部分。
- 再按一次 `Tab` 通常会列出候选项。

推荐习惯：输入长路径时多用 Tab，可同时提高速度并减少拼写错误。

### 2.2 历史命令

| 操作 | 作用 |
| --- | --- |
| `↑` | 取回上一条历史命令 |
| `↓` | 向更新的历史命令移动 |
| `history` | 显示历史列表 |
| `history 20` | 显示最近 20 条 |
| `Ctrl+R` | 反向搜索历史 |

`Ctrl+R` 使用示例：

1. 按 `Ctrl+R`。
2. 输入命令中的部分文字，例如 `ssh`。
3. 继续按 `Ctrl+R` 查找更早的匹配。
4. 按左右方向键进入编辑，或按 Enter 直接执行。
5. 按 `Ctrl+G` 或 `Ctrl+C` 取消搜索。

历史展开语法需要谨慎使用：

```bash
!!        # 上一条命令
!$        # 上一条命令的最后一个参数
```

执行前可以先显示展开结果：

```bash
history -p '!!'
```

### 2.3 命令行编辑快捷键

| 快捷键 | 作用 |
| --- | --- |
| `Ctrl+A` | 移到行首 |
| `Ctrl+E` | 移到行尾 |
| `Alt+B` | 向左移动一个单词 |
| `Alt+F` | 向右移动一个单词 |
| `Ctrl+U` | 删除从光标到行首的内容 |
| `Ctrl+K` | 删除从光标到行尾的内容 |
| `Ctrl+W` | 删除光标前的一个单词 |
| `Alt+D` | 删除光标后的一个单词 |
| `Ctrl+Y` | 粘贴刚被 Readline 删除的内容 |
| `Ctrl+_` | 撤销上一次命令行编辑 |
| `Alt+.` | 插入上一条命令的最后参数 |

### 2.4 控制前台命令

| 快捷键 | 作用 |
| --- | --- |
| `Ctrl+C` | 向前台程序发送中断请求 |
| `Ctrl+Z` | 暂停前台程序，不是终止 |
| `Ctrl+D` | 空行时表示输入结束，通常可退出 Shell |
| `Ctrl+L` | 清理屏幕，效果类似 `clear` |
| `Ctrl+S` | 某些终端中暂停屏幕输出 |
| `Ctrl+Q` | 恢复被 `Ctrl+S` 暂停的输出 |

`Ctrl+C` 不是终端中的“复制”。大多数 Linux 图形终端使用：

- `Ctrl+Shift+C`：复制。
- `Ctrl+Shift+V`：粘贴。

### 2.5 多行命令

行末的反斜杠表示命令尚未结束：

```bash
gcc main.c \
    uart.c \
    -Iinclude \
    -o app
```

Shell 出现 `>` 等待符时，通常是引号、括号或行末反斜杠还没有闭合。如果不想继续，按 `Ctrl+C` 取消。

---

## 3. 路径、文件与目录

### 3.1 重要路径符号

| 写法 | 含义 |
| --- | --- |
| `/` | 根目录 |
| `.` | 当前目录 |
| `..` | 上一级目录 |
| `~` | 当前用户的主目录 |
| `-` | `cd` 记录的上一个目录 |

绝对路径从 `/` 开始：

```text
/home/alice/project/main.c
```

相对路径从当前目录计算：

```text
src/main.c
../include/config.h
```

### 3.2 查看和切换目录

```bash
pwd
ls
ls -lah
ls -lt
cd /var/log
cd ..
cd ~
cd -
```

`ls -lah` 是常用组合：

- `-l`：详细列表。
- `-a`：包含以 `.` 开头的隐藏项。
- `-h`：使文件大小更易读。

### 3.3 创建文件和目录

```bash
mkdir demo
mkdir -p project/src/include
touch notes.txt
```

`touch` 在文件不存在时创建空文件；文件存在时主要更新时间戳。

### 3.4 复制、移动和重命名

```bash
cp config.example config
cp -r src src-backup
cp -a project project-backup
mv old-name.txt new-name.txt
mv report.txt documents/
```

- `cp -r` 递归复制目录。
- `cp -a` 尽量保留权限、时间戳和符号链接等属性。
- `mv` 同时用于移动和重命名。

覆盖前询问：

```bash
cp -i source.txt target.txt
mv -i old.txt target.txt
```

### 3.5 删除

```bash
rm file.txt
rm -i file.txt
rmdir empty-directory
rm -r directory
```

> **危险：** `rm` 通常不经过回收站。`rm -rf` 会递归强制删除，在使用变量、通配符或管理员权限时尤其危险。

先显示目标，再删除：

```bash
find build -maxdepth 1 -type f -name '*.o' -print
find build -maxdepth 1 -type f -name '*.o' -delete
```

处理以减号开头的文件名时，使用 `--` 结束选项：

```bash
rm -- -strange-name
```

### 3.6 通配符

| 模式 | 含义 | 示例 |
| --- | --- | --- |
| `*` | 任意长度文字 | `*.c` |
| `?` | 恰好一个字符 | `file?.txt` |
| `[abc]` | 列表中的一个字符 | `[ab].c` |
| `[0-9]` | 范围内的一个字符 | `log[0-9].txt` |

Shell 会在运行命令前展开通配符。对批量删除先用 `printf` 检查：

```bash
printf '%s\n' *.tmp
rm -- *.tmp
```

### 3.7 空格和特殊字符

引用含空格的路径：

```bash
cd "$HOME/my project"
cp "my notes.txt" backup/
```

变量中的路径几乎总应使用双引号：

```bash
src="$HOME/my project"
cp -a "$src" /tmp/backup/
```

---

## 4. 查看、搜索和处理文本

### 4.1 判断文件类型和属性

```bash
file firmware.bin
stat main.c
wc -l main.c
```

- `file`：根据内容特征判断文件类型。
- `stat`：查看大小、权限、时间戳等详细信息。
- `wc -l`：统计行数。

### 4.2 查看文件内容

```bash
cat short.txt
less large.log
head -n 20 app.log
tail -n 50 app.log
tail -f app.log
```

- `cat` 适合较短文件。
- `less` 适合分页查看大文件，按 `q` 退出。
- `head` 查看开头，`tail` 查看末尾。
- `tail -f` 持续跟踪日志，按 `Ctrl+C` 停止。

### 4.3 搜索文本

```bash
grep 'error' app.log
grep -i 'error' app.log
grep -n 'main' src/main.c
grep -RIn --exclude-dir=.git 'password' .
```

| 选项 | 作用 |
| --- | --- |
| `-i` | 忽略大小写 |
| `-n` | 显示行号 |
| `-R` | 递归搜索目录 |
| `-v` | 反选不匹配的行 |
| `-E` | 使用扩展正则表达式 |

如果已安装 ripgrep，`rg` 在代码库中通常更快，且默认尊重 `.gitignore`：

```bash
rg 'USART_RX_STA'
rg -n -g '*.c' 'main\('
rg --files
```

### 4.4 查找文件

```bash
find . -type f -name '*.c'
find . -type d -name build
find . -type f -size +100M
find . -type f -mtime -7
```

- `-type f`：普通文件。
- `-type d`：目录。
- `-size +100M`：大于 100 MiB。
- `-mtime -7`：最近七天修改过。

包含空格的文件名应使用 NUL 分隔或 `-exec`：

```bash
find . -type f -name '*.tmp' -exec rm -i -- {} \;
```

### 4.5 排序、去重和字段处理

```bash
sort names.txt
sort names.txt | uniq
sort access.log | uniq -c | sort -nr
cut -d: -f1 /etc/passwd
tr '[:lower:]' '[:upper:]' < names.txt
```

`uniq` 只比较相邻行，因此去重前通常先 `sort`。

### 4.6 文本编辑器

```bash
nano notes.txt
vim notes.txt
```

Nano 更适合初学者，底部的 `^` 表示 Ctrl，例如 `^O` 为保存，`^X` 为退出。

Vim 最小操作：

1. 按 `i` 进入插入模式。
2. 按 `Esc` 回到普通模式。
3. 输入 `:wq` 保存退出。
4. 输入 `:q!` 放弃修改退出。

---

## 5. 管道、重定向与命令组合

### 5.1 标准流

| 编号 | 名称 | 默认位置 |
| --- | --- | --- |
| `0` | 标准输入 stdin | 键盘 |
| `1` | 标准输出 stdout | 终端 |
| `2` | 标准错误 stderr | 终端 |

### 5.2 重定向

```bash
echo 'first' > notes.txt
echo 'second' >> notes.txt
sort < names.txt
gcc main.c -o app 2> build-errors.log
gcc main.c -o app > build.log 2>&1
```

- `>` 创建或覆盖文件。
- `>>` 追加到文件末尾。
- `2>` 只重定向错误输出。
- `>file 2>&1` 将标准输出和错误都写入同一文件。

> **注意：** `>` 会先清空目标文件。重要文件应先备份或使用 `>>`。

### 5.3 管道

管道将左边命令的标准输出交给右边命令：

```bash
ps aux | grep '[p]ython'
find . -type f | wc -l
du -h . | sort -h | tail
```

管道默认不传递标准错误。需要时：

```bash
some_command 2>&1 | less
```

### 5.4 同时显示和保存

```bash
make 2>&1 | tee build.log
make 2>&1 | tee -a build.log
```

`tee` 把输入同时写到屏幕和文件，`-a` 表示追加。

### 5.5 按成功或失败决定下一步

```bash
mkdir build && cd build
gcc main.c -o app && ./app
test -f config.ini || echo 'config.ini is missing' >&2
```

- `A && B`：A 成功才执行 B。
- `A || B`：A 失败才执行 B。
- `A ; B`：无论 A 是否成功都执行 B。

---

## 6. 权限、用户与 sudo

### 6.1 理解权限

```bash
ls -l script.sh
```

示例输出：

```text
-rwxr-xr-- 1 alice developers 240 Aug 25 10:00 script.sh
```

权限分为三组：

- `u`：所有者（user）。
- `g`：所属组（group）。
- `o`：其他用户（others）。

| 字母 | 文件含义 | 目录含义 |
| --- | --- | --- |
| `r` | 读取内容 | 列出目录项 |
| `w` | 修改内容 | 创建或删除目录项 |
| `x` | 执行文件 | 进入目录并访问其中项 |

### 6.2 修改权限

```bash
chmod u+x script.sh
chmod g-w config.ini
chmod u=rw,go=r README.md
chmod 755 script.sh
chmod 644 config.ini
```

数字权限中：`r=4`、`w=2`、`x=1`。

- `755` 表示所有者 `rwx`，其他人 `r-x`。
- `644` 表示所有者 `rw-`，其他人 `r--`。

> 不要为了“快速解决权限问题”使用 `chmod -R 777`。这会给所有人过度权限，并可能带来安全风险。

### 6.3 所有者和用户组

```bash
whoami
id
groups
sudo chown alice:developers config.ini
sudo chown -R alice:developers project/
```

`chown -R` 会递归改变整棵目录树，应先确认目标路径。

### 6.4 sudo

`sudo` 使用被授权用户的管理员权限执行一条命令：

```bash
sudo apt update
sudo systemctl restart ssh
```

使用原则：

- 只在需要系统权限时使用。
- 不要对来源不明的命令或脚本使用 `sudo`。
- 避免用 `sudo` 运行普通编辑器对项目源码进行日常编辑，否则文件可能变成 root 所有。

---

## 7. 进程、任务、服务与日志

### 7.1 查看进程

```bash
ps
ps aux
ps -ef
pgrep -a python
top
```

- `ps aux` 或 `ps -ef` 显示较完整的进程列表。
- `pgrep -a name` 按名称查找并显示命令行。
- `top` 动态显示 CPU、内存和进程。

如果已安装 `htop`，它提供更友好的交互界面：

```bash
sudo apt install htop
htop
```

### 7.2 结束进程

```bash
kill 1234
kill -TERM 1234
kill -KILL 1234
pkill -TERM process-name
```

推荐顺序：

1. 先使用默认 `SIGTERM`，允许程序清理资源。
2. 等待后确认进程是否结束。
3. 只有程序不响应时才考虑 `SIGKILL`。

`kill -9` 无法被程序捕获，可能留下未保存数据或不完整文件。

### 7.3 前台和后台任务

```bash
long_command &
jobs
fg %1
bg %1
```

- 命令末尾的 `&` 使命令在后台运行。
- `Ctrl+Z` 暂停前台任务后，可用 `bg` 让它在后台继续。
- `fg` 把任务拉回前台。

需要退出终端后继续运行：

```bash
nohup long_command > app.log 2>&1 &
```

对长时间交互任务，可以考虑 `tmux` 或 `screen`。

### 7.4 systemd 服务

```bash
systemctl status ssh
sudo systemctl start ssh
sudo systemctl stop ssh
sudo systemctl restart ssh
sudo systemctl enable ssh
sudo systemctl disable ssh
```

- `start/stop/restart` 改变当前运行状态。
- `enable/disable` 改变是否开机启动，不等于立即启动或停止。

WSL、容器或某些精简系统可能不使用 systemd，此时 `systemctl` 不一定可用。

### 7.5 系统日志

```bash
journalctl -b
journalctl -u ssh
journalctl -u ssh -n 100
journalctl -u ssh -f
sudo dmesg | less
```

- `-b`：当次启动的日志。
- `-u`：按 systemd 单元过滤。
- `-n 100`：最近 100 条。
- `-f`：持续跟踪新日志。
- `dmesg`：查看内核消息，常用于硬件和驱动排查。

---

## 8. 系统、磁盘与资源

### 8.1 系统信息

```bash
uname -a
hostname
hostnamectl
cat /etc/os-release
date
uptime
```

### 8.2 磁盘和目录大小

```bash
df -h
df -h /home
du -sh project
du -h --max-depth=1 . | sort -h
lsblk
```

- `df` 查看文件系统整体使用情况。
- `du` 统计具体目录或文件占用空间。
- `lsblk` 显示块设备、分区和挂载点。

排查当前目录下最大的项：

```bash
du -ah . | sort -h | tail -n 20
```

### 8.3 内存和 CPU

```bash
free -h
lscpu
top
```

Linux 会把空闲内存用作文件缓存，因此不应只看 `free` 输出中的 `free` 列；`available` 更能反映还可供应用使用的内存。

---

## 9. 软件安装、压缩与解压

### 9.1 Ubuntu/Debian 软件包

```bash
sudo apt update
apt search ripgrep
apt show ripgrep
sudo apt install ripgrep
sudo apt remove ripgrep
sudo apt autoremove
```

- `apt update` 更新软件包索引，不是直接升级所有软件。
- `apt install` 安装软件包。
- `apt remove` 卸载软件包，通常保留系统级配置。
- `apt purge` 还会删除软件包管理的系统配置。

`apt` 适合交互使用；稳定的自动化脚本中通常使用 `apt-get`。

### 9.2 tar 归档

```bash
tar -cf project.tar project/
tar -tf project.tar
tar -xf project.tar
tar -czf project.tar.gz project/
tar -xzf project.tar.gz
tar -cJf project.tar.xz project/
tar -xJf project.tar.xz
```

| 选项 | 含义 |
| --- | --- |
| `-c` | 创建归档 |
| `-x` | 解开归档 |
| `-t` | 列出内容 |
| `-f` | 后面跟归档文件名 |
| `-z` | gzip 压缩 |
| `-J` | xz 压缩 |

解压来源不明的归档前先列出内容：

```bash
tar -tf download.tar.gz | less
```

### 9.3 zip

```bash
zip -r project.zip project/
unzip -l project.zip
unzip project.zip -d output/
```

---

## 10. 网络、SSH 与文件传输

### 10.1 基本网络检查

```bash
ip address
ip route
ping -c 4 github.com
getent hosts github.com
ss -lntp
```

- `ip address`：查看网卡和 IP 地址。
- `ip route`：查看路由表和默认网关。
- `ping`：检查 ICMP 可达性；某些服务器会屏蔽 ICMP，因此 ping 失败不必然代表网站不可用。
- `ss -lntp`：查看正在监听的 TCP 端口及相关进程。

### 10.2 curl 和 wget

```bash
curl -I https://example.com
curl -L -o file.zip https://example.com/file.zip
wget https://example.com/file.zip
```

- `curl -I` 只查看 HTTP 响应头。
- `curl -L` 跟随重定向。
- `curl -o` 指定保存文件名。

不要盲目执行类似下面的命令：

```bash
curl https://unknown.example/install.sh | sudo bash
```

正确做法是先下载、阅读、确认来源和内容，再决定是否执行。

### 10.3 SSH 远程登录

```bash
ssh user@server.example.com
ssh -p 2222 user@server.example.com
ssh -v user@server.example.com
```

- `-p`：指定服务器 SSH 端口。
- `-v`：输出调试信息，用于排查连接和认证问题。

初次连接会询问是否信任主机密钥。对重要服务器应通过可靠渠道核对指纹，而不是习惯性输入 `yes`。

### 10.4 SCP 文件复制

```bash
scp firmware.bin user@server:/srv/firmware/
scp user@server:/var/log/app.log ./
scp -r output/ user@server:/srv/project/
```

冒号前是远程主机，冒号后是远程路径。

### 10.5 rsync 同步

```bash
rsync -avh --progress project/ user@server:/srv/project/
rsync -avh --dry-run project/ user@server:/srv/project/
```

`--dry-run` 只预览操作，建议在批量同步或删除前使用。

注意源目录末尾的斜杠：

- `project/` 表示复制 `project` 里的内容。
- `project` 表示复制 `project` 目录本身。

---

## 11. 环境变量与程序执行

### 11.1 查看环境变量

```bash
env
printenv PATH
echo "$HOME"
echo "$PATH"
```

常见环境变量：

| 变量 | 含义 |
| --- | --- |
| `HOME` | 当前用户主目录 |
| `USER` | 当前用户名 |
| `PWD` | 当前工作目录 |
| `PATH` | Shell 查找外部命令的目录列表 |

### 11.2 临时设置

```bash
export APP_ENV=development
export PATH="$PATH:$HOME/bin"
```

不要丢失原有 `PATH`：

```bash
export PATH="$HOME/bin"   # 通常是错误的
```

上面的错误写法可能导致 `ls`、`git` 等命令无法找到。

### 11.3 加载 Shell 配置

```bash
source ~/.bashrc
. ~/.bashrc
```

`source` 在当前 Shell 中运行文件，因此能修改当前 Shell 的环境。不要 `source` 来源不明的脚本。

### 11.4 执行当前目录的程序

```bash
gcc hello.c -o hello
./hello
```

`./hello` 中的 `./` 表示当前目录。出于安全原因，当前目录通常不在 `PATH` 中。

Shell 脚本的两种执行方式：

```bash
bash script.sh
chmod +x script.sh
./script.sh
```

---

## 12. Git 的核心模型

### 12.1 Git 与 GitHub

- **Git** 是分布式版本控制系统，可以完全在本地使用。
- **GitHub** 是托管 Git 仓库和提供协作功能的网络平台。
- `git commit` 创建本地历史；`git push` 才会把本地提交发送到远程仓库。

### 12.2 四个重要位置

```text
工作区             暂存区             本地仓库             远程仓库
files              index              commits                GitHub
  |                   |                   |                      |
  | git add           | git commit        | git push             |
  +------------------>|------------------>|--------------------->|
  |                   |                   |                      |
  |<------ git restore|                   |<---- git fetch ------|
```

- **工作区**：你正在编辑的文件。
- **暂存区**：下一次提交要包含的快照内容。
- **本地仓库**：`.git` 中保存的提交历史。
- **远程仓库**：GitHub 等服务器上的仓库。

### 12.3 文件状态

```text
Untracked  ->  Staged  ->  Committed
    ^             |
    |             v
Modified  <-------+
```

- `Untracked`：Git 还没有跟踪的文件。
- `Modified`：已跟踪文件被修改。
- `Staged`：已加入暂存区。
- `Committed`：已记录到本地提交历史。

---

## 13. Git 初始化、添加与提交

### 13.1 检查 Git

```bash
git --version
git help -a
git help status
```

### 13.2 设置身份

```bash
git config --global user.name "Your Name"
git config --global user.email "you@example.com"
git config --global init.defaultBranch main
```

查看配置及来源：

```bash
git config --list --show-origin
```

`user.name` 和 `user.email` 会写入今后的提交作者信息。应使用希望在项目历史中展示的值。

### 13.3 创建或克隆仓库

将现有目录变成 Git 仓库：

```bash
cd project
git init -b main
```

克隆已存在的远程仓库：

```bash
git clone https://github.com/OWNER/REPOSITORY.git
cd REPOSITORY
```

`git init` 和 `git clone` 不是同一个步骤的前后关系。克隆得到的目录已经是 Git 仓库，无需再执行 `git init`。

### 13.4 查看状态

```bash
git status
git status --short
```

`--short` 的常见状态字母：

| 状态 | 含义 |
| --- | --- |
| `??` | 未跟踪 |
| `A ` | 已暂存的新文件 |
| `M ` | 暂存区中有修改 |
| ` M` | 工作区中有未暂存修改 |
| `MM` | 既有已暂存修改，又有更新的未暂存修改 |
| `D ` / ` D` | 已暂存/未暂存删除 |

### 13.5 添加到暂存区

```bash
git add README.md
git add src/main.c include/main.h
git add src/
git add .
git add -p
```

- `git add .` 添加当前目录下的变化，包括删除。
- `git add -p` 逐块选择要暂存的修改，适合把不同逻辑拆成不同提交。

暂存不等于提交，也不等于上传。

### 13.6 提交

```bash
git diff --cached
git commit -m "Add UART receive handling"
```

提交前推荐顺序：

```bash
git status
git diff
git diff --cached
git commit -m "Describe the completed change"
```

好的提交应当：

- 只包含一个相对完整的逻辑变更。
- 不包含密码、密钥、临时文件或意外的大文件。
- 提交信息说明“做了什么”，而不是 `update` 或 `changes` 这类模糊描述。

---

## 14. Git 历史、差异与文件追踪

### 14.1 查看差异

```bash
git diff
git diff --cached
git diff HEAD
git diff main..feature
git diff --stat
git diff -- README.md
```

| 命令 | 比较内容 |
| --- | --- |
| `git diff` | 工作区与暂存区 |
| `git diff --cached` | 暂存区与当前提交 |
| `git diff HEAD` | 工作区及暂存区与当前提交 |

### 14.2 查看历史

```bash
git log
git log --oneline
git log --oneline --graph --decorate --all
git log --stat
git log -- README.md
```

推荐配置一个只读别名：

```bash
git config --global alias.lg "log --oneline --graph --decorate --all"
git lg
```

### 14.3 查看提交

```bash
git show HEAD
git show abc1234
git show HEAD:README.md
```

`HEAD` 通常表示当前分支的最新提交；`HEAD~1` 表示它的第一个父提交。

### 14.4 查看文件每行来源

```bash
git blame src/main.c
git blame -L 20,40 src/main.c
```

`blame` 用于定位历史背景，不应用作对个人的指责工具。

### 14.5 由 Git 管理移动和删除

```bash
git mv old-name.c new-name.c
git rm obsolete.c
git rm --cached local-config.ini
```

Git 实际上会根据内容相似度识别重命名。直接使用 `mv` 后再 `git add -A` 也可以，`git mv` 只是方便写法。

---

## 15. Git 分支、合并与冲突

### 15.1 分支基本操作

```bash
git branch
git branch --all
git switch -c feature/uart-stop
git switch main
git branch -m old-name new-name
git branch -d feature/uart-stop
```

- `git switch -c name` 创建并切换分支。
- `git branch -d` 只删除已安全合并的分支。
- `git branch -D` 强制删除，可能使未合并工作难以找回。

分支是指向提交的轻量指针，创建分支不会复制整个项目目录。

### 15.2 合并分支

```bash
git switch main
git merge feature/uart-stop
```

含义是：把 `feature/uart-stop` 的历史整合进当前所在的 `main`。合并前应先确认当前分支。

### 15.3 解决冲突

冲突文件中可能出现：

```text
<<<<<<< HEAD
current branch content
=======
incoming branch content
>>>>>>> feature/uart-stop
```

基本处理流程：

```bash
git status
# 编辑冲突文件，删除标记并保留正确内容
git add conflicted-file.c
git commit
```

放弃本次合并：

```bash
git merge --abort
```

### 15.4 rebase 的基本概念

```bash
git switch feature/uart-stop
git rebase main
```

`rebase` 把当前分支的提交重放到新的基础上，可以获得更线性的历史，但会改写提交 ID。

> 不要随意 rebase 已经推送且与他人共享的提交。对公开历史的改写需要团队协调。

### 15.5 标签

```bash
git tag
git tag -a v1.0.0 -m "Release v1.0.0"
git show v1.0.0
git push origin v1.0.0
git push origin --tags
```

普通 `git push` 默认不会推送所有标签。

---

## 16. Git 撤销、恢复与暂存

在撤销前先执行：

```bash
git status
git diff
git log --oneline -n 5
```

要先回答两个问题：要撤销的是工作区、暂存区还是提交？这个提交是否已经推送给他人？

### 16.1 撤销未暂存修改

```bash
git restore README.md
git restore src/
```

> `git restore` 会用暂存区内容覆盖工作区。未保存在其他位置的修改可能无法找回。

只查看要恢复的内容：

```bash
git diff -- README.md
```

### 16.2 取消暂存

```bash
git restore --staged README.md
```

这会把文件移出暂存区，但保留工作区的修改。

### 16.3 修改最近一次提交

```bash
git add forgotten-file.c
git commit --amend
```

只修改提交信息：

```bash
git commit --amend -m "Correct commit message"
```

`--amend` 会创建新的提交 ID。如果原提交已经共享，修改前应评估对其他人的影响。

### 16.4 reset 三种模式

```bash
git reset --soft HEAD~1
git reset --mixed HEAD~1
git reset --hard HEAD~1
```

| 模式 | 移动分支指针 | 重置暂存区 | 重置工作区 |
| --- | --- | --- | --- |
| `--soft` | 是 | 否 | 否 |
| `--mixed` | 是 | 是 | 否 |
| `--hard` | 是 | 是 | 是 |

> **危险：** `git reset --hard` 会丢弃已跟踪文件的未提交修改。不要在不理解目标提交和当前工作区的情况下执行。

### 16.5 安全撤销已公开的提交

```bash
git revert abc1234
```

`revert` 新建一个反向提交，不改写已有历史，因此通常适合撤销已推送的提交。

### 16.6 reflog 找回提交

```bash
git reflog
git show HEAD@{2}
git switch -c recovery HEAD@{2}
```

`reflog` 记录本地引用的移动，常可用来找回误 reset、rebase 或删除分支前的提交。它是本地日志，不会作为普通历史推送到远程。

### 16.7 stash 临时保存工作

```bash
git stash push -m "WIP UART debug"
git stash list
git stash show -p stash@{0}
git stash apply stash@{0}
git stash pop
git stash drop stash@{0}
```

默认 stash 通常不包含未跟踪文件。需要包含时：

```bash
git stash push -u -m "WIP with untracked files"
```

---

## 17. Git 远程仓库与 GitHub

### 17.1 查看和配置远程

```bash
git remote -v
git remote add origin https://github.com/OWNER/REPOSITORY.git
git remote get-url origin
git remote set-url origin git@github.com:OWNER/REPOSITORY.git
git remote remove origin
```

`origin` 只是默认常用的远程名称，不是 Git 的强制关键字。

### 17.2 fetch、pull 和 push

```bash
git fetch origin
git pull
git push
```

| 命令 | 主要作用 |
| --- | --- |
| `git fetch` | 下载远程更新，不直接改变当前工作区 |
| `git pull` | 先 fetch，再将远程更新整合到当前分支 |
| `git push` | 把本地提交更新到远程 |

对变化敏感的项目，可先 `git fetch`，检查后再合并：

```bash
git fetch origin
git log --oneline --graph --decorate HEAD..origin/main
git merge origin/main
```

### 17.3 首次推送并设置上游

```bash
git push -u origin main
```

`-u` 将当前本地分支与远程分支建立跟踪关系。以后通常只需：

```bash
git pull
git push
```

### 17.4 将现有本地项目上传到 GitHub

1. 在 GitHub 创建新仓库。
2. 如果本地已有提交，创建远程仓库时不要预先生成 README、`.gitignore` 或 License，以避免创建两条独立历史。
3. 复制 GitHub 显示的 HTTPS 或 SSH 地址。
4. 配置远程并推送。

```bash
cd project
git status
git branch -M main
git remote add origin https://github.com/OWNER/REPOSITORY.git
git remote -v
git push -u origin main
```

### 17.5 HTTPS 和 SSH 认证

GitHub 命令行通信常用：

- HTTPS + 凭据管理器或个人访问令牌。
- SSH + 本地私钥和上传到 GitHub 的公钥。

GitHub 不再接受账户密码作为 Git HTTPS 操作的认证凭据。

SSH 的基本流程：

```bash
ssh-keygen -t ed25519 -C "you@example.com"
eval "$(ssh-agent -s)"
ssh-add ~/.ssh/id_ed25519
cat ~/.ssh/id_ed25519.pub
```

只把 `.pub` 公钥内容添加到 GitHub。私钥 `~/.ssh/id_ed25519` 不能上传、提交或发送给他人。

测试 GitHub SSH 连接：

```bash
ssh -T git@github.com
```

### 17.6 远程分支

```bash
git branch -r
git branch -a
git switch --track origin/feature/demo
git push -u origin feature/demo
git push origin --delete feature/demo
git fetch --prune
```

`git fetch --prune` 清理已在远程删除的远程跟踪分支，不会直接删除对应的本地分支。

---

## 18. .gitignore 与 .gitattributes

### 18.1 .gitignore

`.gitignore` 告诉 Git 哪些未跟踪文件不应进入版本库。

```gitignore
# 根目录下的本地工具
/software_tools/

# 任意层级的编译目录
build/
OBJ/
Listings/

# 编译中间文件
*.o
*.d
*.tmp

# 保留一个示例配置
*.local
!config.example.local
```

检查忽略规则：

```bash
git status --ignored
git check-ignore -v path/to/file
```

`git check-ignore -v` 会显示命中的规则及来源文件。

### 18.2 为什么已跟踪文件仍然出现

`.gitignore` 不会停止 Git 跟踪已经进入索引或历史的文件。只停止跟踪但保留本地文件：

```bash
git rm --cached local-config.ini
git commit -m "Stop tracking local configuration"
```

对目录：

```bash
git rm -r --cached build/
```

这会在下一次提交中从仓库删除该路径，因此先使用 `git status` 检查暂存结果。

### 18.3 .gitattributes 和换行符

`.gitattributes` 用于定义路径属性，包括文本规范化、换行符和二进制文件。

```gitattributes
* text=auto eol=lf

*.bat text eol=crlf

*.pdf binary
*.docx binary
*.png binary
*.zip binary
```

- 代码和 Shell 脚本在仓库与工作区使用 LF。
- Windows 批处理文件使用 CRLF。
- PDF、Word、PNG 和 ZIP 按二进制文件处理。

新增或修改属性后规范化已跟踪文件：

```bash
git add .gitattributes
git add --renormalize .
git status
git diff --cached
```

### 18.4 大文件与 Git LFS

Git 历史会保留每一次提交的对象。安装包、虚拟机磁盘、编译产物和数据集会迅速膨胀仓库。

提交前查找大文件：

```bash
find . -type f -size +50M -not -path './.git/*' -print
```

如果大型二进制文件确实必须版本化，可评估 Git LFS：

```bash
git lfs install
git lfs track '*.bin'
git add .gitattributes
```

Git LFS 不会自动迁移早已提交的大文件；历史迁移会改写提交，需要单独计划。

---

## 19. 完整实战工作流

### 19.1 将普通项目变成 Git 仓库

```bash
cd ~/projects/unmanned-car
git init -b main
```

先创建 `.gitignore` 和 `.gitattributes`，然后检查：

```bash
git status --short
find . -type f -size +50M -not -path './.git/*' -print
git status --ignored
```

首次提交：

```bash
git add .
git status
git diff --cached --stat
git commit -m "Initial commit"
```

### 19.2 连接 GitHub

在 GitHub 创建同名空仓库，然后：

```bash
git remote add origin https://github.com/OWNER/unmanned-car.git
git remote -v
git push -u origin main
```

### 19.3 日常修改

```bash
git status
git diff
git add src/main.c include/main.h
git diff --cached
git commit -m "Handle UART stop command"
git push
```

### 19.4 分支开发

```bash
git switch main
git pull --ff-only
git switch -c feature/stop-command

# 编辑和测试
git status
git add src/main.c
git commit -m "Add STOP command handling"
git push -u origin feature/stop-command
```

审查和合并后更新本地分支：

```bash
git switch main
git pull --ff-only
git branch -d feature/stop-command
git fetch --prune
```

`git pull --ff-only` 只允许快进更新，不会意外创建合并提交；如果本地和远程已分叉，它会停止并要求你明确处理。

### 19.5 提交前检查清单

1. `git status` 中是否只有预期文件？
2. `git diff` 是否包含调试代码、密码或无关修改？
3. `git diff --cached` 是否正是此次提交内容？
4. 编译、测试或基本运行检查是否通过？
5. 提交信息是否具体？

---

## 20. 常见问题与排查

### 20.1 command not found

```bash
type command-name
command -v command-name
echo "$PATH"
```

可能原因：软件未安装、命令名拼错、程序不在 `PATH` 中，或需要使用 `./program`。

### 20.2 Permission denied

```bash
ls -l path
namei -l path
id
```

检查文件权限、所有者，以及每一层父目录是否可进入。不要直接使用 `chmod 777` 掩盖问题。

### 20.3 终端突然不输出

可能误按了 `Ctrl+S`。按 `Ctrl+Q` 恢复。

### 20.4 文件名包含空格

```bash
cd "My Project"
rm -- "strange file.txt"
```

对变量始终使用双引号：

```bash
rm -- "$file"
```

### 20.5 Git 说 nothing to commit

```bash
git status
git diff
git diff --cached
```

可能情况：

- 文件没有变化。
- 文件被 `.gitignore` 忽略。
- 当前不在预期仓库。
- 修改在其他分支或其他工作目录。

查看当前仓库根目录：

```bash
git rev-parse --show-toplevel
```

### 20.6 文件被忽略

```bash
git check-ignore -v path/to/file
```

如果文件已经跟踪，`.gitignore` 不会使它自动消失。

### 20.7 remote origin already exists

```bash
git remote -v
git remote set-url origin https://github.com/OWNER/REPOSITORY.git
```

不需要重复 `git remote add origin`，而应检查并修改已存在的地址。

### 20.8 push 被拒绝

先检查本地和远程历史：

```bash
git fetch origin
git status
git log --oneline --graph --decorate --all -n 30
```

如果远程有本地没有的提交，先整合：

```bash
git pull --rebase
git push
```

`pull --rebase` 会重放本地未推送提交。如果不理解 rebase，先使用 `git status` 和 `git log` 确认历史，不要直接强制推送。

### 20.9 身份未配置

Git 提示 `Please tell me who you are` 时：

```bash
git config --global user.name "Your Name"
git config --global user.email "you@example.com"
```

### 20.10 LF will be replaced by CRLF

这是换行符转换提示，不代表 `git add` 失败。使用项目级 `.gitattributes` 明确文本和二进制规则，然后：

```bash
git add .gitattributes
git add --renormalize .
git status
```

### 20.11 误删分支或 reset 错了

先不要继续做大量改动，查看：

```bash
git reflog
```

找到正确提交后，先创建恢复分支：

```bash
git switch -c recovery <commit-id>
```

### 20.12 敏感信息已提交

1. 立即在对应服务中撤销或轮换密钥。
2. 不要以为再提交一次“删除密钥”就从历史中消失。
3. 按托管平台的官方敏感数据清理流程处理历史。
4. 通知已可能获取该密钥的协作者。

---

## 21. 命令速查表

### 21.1 交互式终端

| 操作 | 作用 |
| --- | --- |
| `Tab` | 自动补全 |
| `↑` / `↓` | 上一条/下一条历史 |
| `Ctrl+R` | 搜索历史 |
| `Ctrl+A` / `Ctrl+E` | 行首/行尾 |
| `Ctrl+U` / `Ctrl+K` | 删除光标前/后内容 |
| `Ctrl+W` | 删除前一个单词 |
| `Ctrl+C` | 中断前台命令 |
| `Ctrl+Z` | 暂停前台命令 |
| `Ctrl+L` | 清屏 |
| `Ctrl+Q` | 恢复被 `Ctrl+S` 暂停的输出 |

### 21.2 Linux 常用命令

| 命令 | 作用 |
| --- | --- |
| `pwd` | 显示当前目录 |
| `ls -lah` | 详细列出目录内容 |
| `cd path` | 切换目录 |
| `mkdir -p path` | 递归创建目录 |
| `cp -a src dst` | 复制并尽量保留属性 |
| `mv old new` | 移动或重命名 |
| `rm -i file` | 询问后删除 |
| `less file` | 分页查看 |
| `tail -f log` | 跟踪日志 |
| `grep -RIn text .` | 递归搜索文本 |
| `find . -name pattern` | 查找路径 |
| `chmod u+x file` | 添加所有者执行权限 |
| `ps aux` | 查看进程 |
| `kill PID` | 请求进程结束 |
| `df -h` | 查看文件系统空间 |
| `du -sh path` | 查看路径占用空间 |
| `free -h` | 查看内存 |
| `ip address` | 查看网络地址 |
| `ss -lntp` | 查看 TCP 监听端口 |
| `ssh user@host` | 登录远程主机 |
| `scp src user@host:path` | 向远程复制文件 |

### 21.3 Git 常用命令

| 命令 | 作用 |
| --- | --- |
| `git init -b main` | 创建本地仓库 |
| `git clone URL` | 克隆仓库 |
| `git status` | 查看状态 |
| `git diff` | 查看未暂存差异 |
| `git diff --cached` | 查看已暂存差异 |
| `git add path` | 添加到暂存区 |
| `git commit -m message` | 创建本地提交 |
| `git log --oneline --graph --all` | 简洁查看历史 |
| `git switch -c branch` | 创建并切换分支 |
| `git switch branch` | 切换分支 |
| `git merge branch` | 合并分支到当前分支 |
| `git restore path` | 撤销未暂存修改 |
| `git restore --staged path` | 取消暂存 |
| `git stash` | 临时保存修改 |
| `git remote -v` | 查看远程地址 |
| `git fetch` | 下载远程更新 |
| `git pull` | 下载并整合远程更新 |
| `git push` | 推送本地提交 |
| `git reflog` | 查看本地引用移动记录 |

---

## 22. 官方参考资料

本文的命令行为和工作流主要参考以下官方文档：

- [GNU Bash Reference Manual](https://www.gnu.org/software/bash/manual/bash.html)
- [GNU Readline Library Manual](https://tiswww.case.edu/php/chet/readline/readline.html)
- [GNU Coreutils Manual](https://www.gnu.org/software/coreutils/manual/coreutils.html)
- [Ubuntu Manpage Repository](https://manpages.ubuntu.com/)
- [systemctl Manual](https://www.freedesktop.org/software/systemd/man/latest/systemctl.html)
- [journalctl Manual](https://www.freedesktop.org/software/systemd/man/latest/journalctl.html)
- [OpenSSH Manual Pages](https://www.openssh.com/manual.html)
- [Git Reference](https://git-scm.com/docs)
- [Pro Git Book](https://git-scm.com/book/en/v2)
- [GitHub: Adding locally hosted code to GitHub](https://docs.github.com/en/migrations/importing-source-code/using-the-command-line-to-import-source-code/adding-locally-hosted-code-to-github)
- [GitHub: Managing remote repositories](https://docs.github.com/en/get-started/git-basics/managing-remote-repositories)
- [GitHub: Connecting to GitHub with SSH](https://docs.github.com/en/authentication/connecting-to-github-with-ssh)

---

## 最后的建议

不要靠一次背下所有命令来学习 Linux 和 Git。更有效的方法是：

1. 建立一个临时练习目录。
2. 使用 Tab 补全减少路径拼写。
3. 对命令使用 `--help` 和 `man`。
4. 删除、覆盖、批量移动和 Git 重置前先执行只读检查。
5. 频繁运行 `git status`，在每个阶段理解当前状态。
6. 用小而清晰的提交记录每一步有意义的进展。

当你忘记命令时，记住如何查找正确用法，比记住每个选项更重要。
