# parameters 'branch' 'binary'
branch=$1
filename=$2

pass=$PASSWORD
username=$USER
version=$(cat ./version.txt|tr -d '\n'|tr -d '\r')
server=lora.hazemon.in.th
access=$username@$server

bin_path=/home/$USER/nginx/esp-ota/$branch

sshpass -p "$pass" scp "$filename" "$access":"$bin_path"/can5-app-"$version".bin
sshpass -p "$pass" ssh "$access" "rm $bin_path/can5-app.bin || true; ln $bin_path/can5-app-$version.bin $bin_path/can5-app.bin; echo $version > $bin_path/latest.txt"
