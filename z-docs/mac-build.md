pip install powerline-status --user
cd ~/Desktop/OpenSource
git clone https://github.com/altercation/solarized
cd solarized/iterm2-colors-solarized/
open .
cd ~/Desktop/OpenSource
git clone https://github.com/fcamblor/oh-my-zsh-agnoster-fcamblor.git
cd oh-my-zsh-agnoster-fcamblor/
./install
vi ~/.zshrc
将ZSH_THEME后面的字段改为agnoster。
cd ~/.oh-my-zsh/custom/plugins/
git clone https://github.com/zsh-users/zsh-syntax-highlighting.git
vi ~/.zshrc
找到plugins，此时plugins中应该已经有了git，我们需要把高亮插件也加上：
请务必保证插件顺序，zsh-syntax-highlighting必须在最后一个。
然后在文件的最后一行添加：source ~/.oh-my-zsh/custom/plugins/zsh-syntax-highlighting/zsh-syntax-highlighting.zsh
source ~/.zshrc

```zsh
cd /Users/xuyang/projects/TXSQL # OK
# 1 仅生成构建文件（不自动 make）
./build.sh -t release -B 0 -b "$(pwd)" --clang
# 2 手动编译（按 CPU 核心数并行）
make -C bld-release -j"$(sysctl -n hw.ncpu)"
# 4 查看可执行文件
cd bld-release/runtime_output_directory/mysqld
ls
```

```zsh
# 路径变量
export TXSQL_HOME="/Users/xuyang/projects/TXSQL"
export TXSQL_BIN="$TXSQL_HOME/bld-release/runtime_output_directory/mysqld"
export TXSQL_DATA="$TXSQL_HOME/devdata"
export TXSQL_SOCK="/tmp/30002_txsql.sock"

# 1) 初始化数据目录（无密码）
mkdir -p "$TXSQL_DATA"
"$TXSQL_BIN" --initialize-insecure --datadir="$TXSQL_DATA"

# 2) 启动
"$TXSQL_BIN" \
  --datadir="$TXSQL_DATA" \
  --port=30002 \
  --socket="$TXSQL_SOCK" \
  --log-error="$TXSQL_DATA/error.log" \
  --server-id=1 \
  --skip_ssl \
  --bind-address=127.0.0.1 &

# 3) 验证进程
ps aux | grep mysqld | grep "$TXSQL_DATA"
# 4) MySQL命令行客户端连接
mysql -S /tmp/30002_txsql.sock -uroot --skip-password -e "SELECT VERSION();"
```