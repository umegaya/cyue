#!bin/sh

mkdir .ssh
chmod 700 .ssh
cat /mnt/iyatomi/.ssh/id_rsa.pub >> .ssh/authorized_keys
chmod 600 .ssh/authorized_keys

