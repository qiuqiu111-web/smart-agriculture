# 编译 / 运行 / 安装 / 自启动 指南

## 1. 安装依赖

```bash
sudo apt update
sudo apt install -y build-essential cmake
```

## 2. 编译 & 安装

```bash
cd ~/smart_agriculture               # 项目根目录
mkdir -p build && cd build
cmake ..
make -j$(nproc)
sudo make install                    # → /usr/local/bin/smart_agri
                                     # → /usr/local/etc/lubancat_agri/config.json
```

## 3. 手动运行（验证）

```bash
# 用安装路径的配置文件运行
smart_agri /usr/local/etc/smart_agri/config.json

# CSV 默认输出到 ./data/ 目录
ls data/
```

## 4. 上电自启动 (systemd)

```bash
# 创建服务文件
sudo tee /etc/systemd/system/smart_agri.service << 'EOF'
[Unit]
Description=LubanCat Smart Agriculture
After=multi-user.target

[Service]
Type=simple
ExecStart=/usr/local/bin/smart_agri /usr/local/etc/lubancat_agri/config.json
WorkingDirectory=/home/cat/smart_agriculture
Restart=always
RestartSec=10
User=cat

[Install]
WantedBy=multi-user.target
EOF

# 启用并启动
sudo systemctl daemon-reload
sudo systemctl enable smart_agri
sudo systemctl start smart_agri

# 查看状态 / 日志
systemctl status smart_agri
journalctl -u smart_agri -f        # -f 实时跟踪日志
```

## 5. 常用管理命令

```bash
sudo systemctl stop smart_agri     # 停止
sudo systemctl restart smart_agri  # 重启
sudo systemctl disable smart_agri  # 取消自启动

# 查看 CSV 输出
tail -f /home/cat/smart_agriculture/data/*.csv
```
