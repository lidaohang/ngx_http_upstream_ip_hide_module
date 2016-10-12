### 功能 
查询请求URL的Upstream上游服务请求的server


### 安装方式(默认二进制已经安装好)
```
patch -p1 < ngx_http_upstream_ip_hide_module/tengine-2.1.0-ngx_http_special_response.patch

./configure --prefix=/usr/local/nginx  --add-module=ngx_http_upstream_ip_hide_module

make
make install

```

### 使用方式
1. 配置server_info on;
```
http {
    ...
    server_info on;
    ...
    
    server {
         listen  80;
         
         location /test {
            proxy_pass http://backup_server;
         }
    }   
}
```

2. 配置ip_hide指令
```
upstream backup_server {
     server 127.0.0.1:8000;
     server 127.0.0.1:8001;
     ip_hide;
}
```
