1. put binaries:
noter-srv -> noter-srv_0.1-1_amd64/opt/noter-srv

2. build package
dpkg-deb --build --root-owner-group noter-srv_0.1-1_amd64

3. install package
sudo dpkg -i noter-srv_0.1-1_amd64.deb