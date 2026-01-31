subnet=$1
out_file=$2

echo "Enter OpenWRT router root password:"

ssh root@$subnet.1 'cat /var/dhcp.leases' | awk '{print $3}' 2>&1 | tee $out_file