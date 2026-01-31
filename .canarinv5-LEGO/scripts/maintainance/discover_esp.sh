filename=$1
out_file=$2

function execute {
    ipaddr=$1
    node=''
    node=$(curl "http://$ipaddr/api/general"  -s  -H 'Accept: */*'   -H 'Accept-Language: en-US,en;q=0.9,ne-IN;q=0.8,ne;q=0.7'   -H 'Authorization: Basic Y2FuYXJpbmVyOmludGVybGFi'   -H 'Connection: keep-alive'   -H 'DNT: 1'   -H 'Referer: http://192.168.200.188/device'   -H 'User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/112.0.0.0 Safari/537.36'   --compressed   --insecure)
    if [ $? -eq 0 ]; then
      name=$(echo $node | grep CFG_DEVICE_NAME | cut -d '"' -f 4)
      echo "$ipaddr:$node"
      echo "$ipaddr:$name" >> $out_file
    fi
}


rm $out_file


while read p; do
    execute $p
done < $filename
