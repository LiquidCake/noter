# noter
simple Linux client/server app to instantly create notes (text of binary) right through stdin  
note is persisted inside DB or sent via email (text only) - channel to send notes is set in config file

#### notes may be sent e.g. like these:
```
$ echo feed cat 3 times per day at least | noter  
$ cat mycat-manual.txt | noter  
$ cat cat.png | noter  
$ ps -aux | noter
```

max size of single note: 1000mb  
(dont try to send big notes via email though)  
data is sent in plaintext (no SSL/TLS supported)  

### Structure:
/noter - client app consists of `noter` binary and `noterd` daemon that sends data to server asynchronously  
/noter-srv - socket server app that receives data from `noterd` and persists it inside DB as blob (or sends via email)

### Installation:
##### 1. Install deb packages:
noter/package/noter_0.1-1_amd64.deb (on client machine)  
noter-srv/package/noter-srv_0.1-1_amd64.deb (on client or dedicated server machine)

##### 2. Check config files
/etc/noter/config.cfg  
- set IP of noter-srv (default - localhost)  
- set channel to send notes: db or email (default - db)  

/etc/noter-srv/config.cfg  
- set IP and credentials of noter MySQL DB instance (default - localhost)  
and/or  
- set SMTP server IP/credentials

### Dependencies:
to install packages:  
noter - libssl  
noter-srv - libssl, libcurl, libmysqlcppconn (e.g. libmysqlcppconn9_8.0.29-1ubuntu20.04_amd64.deb from https://dev.mysql.com/downloads/connector/cpp/)
