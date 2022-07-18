# SIMPLEX

A toy federated storage engine for author's personal study.

```
git clone -b mariadb-10.6.7 --depth=1 git@github.com:MariaDB/server.git mariadb-10.6.7
cd mariadb-10.6.7/storage && git clone git@github.com:nayuta-yanagisawa/simplex.git
```
```
mkdir ../bld && cd $_
cmake .. -DPLUGIN_{ARCHIVE,TOKUDB,MROONGA,OQGRAPH,ROCKSDB,CONNECT,COLUMNSTORE,SPIDER}=NO
cmake --build . -j $(grep ^cpu\\scores /proc/cpuinfo | uniq |  awk '{print $4}')
```
```
./mysql-test/mtr --suite simplex
```
