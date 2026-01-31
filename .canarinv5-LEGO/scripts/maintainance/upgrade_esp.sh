filename=$1
out_file=$2

function execute {
    ipaddr=$(echo $1 | cut -d ':' -f1)
    name=$(echo $1 | cut -d ':' -f2)
    cmd=$(curl "http://$ipaddr/api/cmds" \
      -s \
      -H 'Accept: */*' \
      -H 'Accept-Language: en-US,en;q=0.9,ne-IN;q=0.8,ne;q=0.7' \
      -H 'Authorization: Basic Y2FuYXJpbmVyOmludGVybGFi' \
      -H 'Connection: keep-alive' \
      -H 'Content-Type: application/json' \
      -H 'DNT: 1' \
      -H 'Origin: http://192.168.200.188' \
      -H 'Referer: http://192.168.200.188/device' \
      -H 'User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/112.0.0.0 Safari/537.36' \
      --data-raw '{"command":"upgrade_firmware"}' \
      --compressed \
      --insecure)
    if [ $? -eq 0 ]; then
      echo "$ipaddr:$name upgrade command sent!"
      echo "$ipaddr:$name" >> $out_file
    fi
}

rm $out_file

echo

while read p; do
    execute $p &
done < $filename

wait < <(jobs -p)
