## 开启语音识别前端

在本目录启动一个 HTTP服务器，提供静态文件访问，输入：
```shell
python -m http.server 8000
```

在浏览器中打开`http://localhost:8000/`进入如下界面：
![](figs/1.jpg)

点击提交结果后将向服务端发送序列`<eos>`

## 开启字符处理后端

安装通信库：
```shell
sudo apt update
sudo apt install libwebsockets-dev
```

启动服务端：
```shell
# 默认启动设置
make run

# 自定义token投送时间，单位s
make
./server --send_delay 0.5

# 自定义通信端口，谨慎使用，需要与前端框架同步
make
./server --port 1234
```
该服务端在收到文本后，将以固定频率向外打印/发送字符，见`server.c print_thread_func`
（目前仅支持中文字符的拆解发送）
若收到`<eos>`，视作特殊token，直接打印/发送

---

然后就可以插上麦克风体验了
