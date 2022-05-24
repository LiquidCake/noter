1. put binaries:
noter -> noter_0.1-1_amd64/usr/bin
noterd -> noter_0.1-1_amd64/usr/sbin

2. build package
dpkg-deb --build --root-owner-group noter_0.1-1_amd64

3. install package
sudo dpkg -i noter_0.1-1_amd64.deb