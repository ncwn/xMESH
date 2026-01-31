# Upgrade all can5 in a subnet

```bash
$ ./discover_ip.sh 192.168.200 tmp/ip_addr_list.txt
```
The first parameter is the first 3 octet of the subnet. In this case its 192.168.200. Change it to your subnet.
The second parameter is the output file with list of ips.

ip_addr.txt will contain the list of ip addresses in the subnet.

```bash
$ ./discover_esp.sh tmp/ip_addr_list.txt tmp/can5_list.txt
```

can5_list.txt will contain the list of canarin5.

Check the number of can5 discovered.
```bash
$ cat tmp/can5_list.txt | wc -l
```



```
$ ./upgrade_esp.sh tmp/can5_list.txt  tmp/out.txt
```

Use the list generated to do mass OTA upgrade.


Wait for some time.

Run discovery again to show the new firmware version.
```bash
$ ./discover_esp.sh tmp/ip_addr_list.txt tmp/can5_list.txt | grep CFG_APP_VERSION
```


Check the number of can5 discovered after upgrade.
```bash
$ cat tmp/can5_list.txt | wc -l
```




