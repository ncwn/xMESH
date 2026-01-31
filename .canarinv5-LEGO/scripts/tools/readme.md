# Tools

## sd_provision.py

Provision a freshly formatted FAT32 sd card. To provision a freshly formatted sd card, first create
* Provision file,
* crontab file.


Provision file should only contain unique config keys which will in turn override the tempalate defined in sd_provision.py file itself.
Crontab file should have the corn jobs to be installed in the can5 node.

Sample files are provided as `sample_files/sample_provision.json` and `sample_files/sample_crontab`. Edit these sample files to your liking. Execute the script:

```shell
$ ./sd_provision.py -pfile sample_files/sample_provision.json -cfile sample_files/sample_crontab sd_card_path
```

```sd_card_path``` should be the path to the root folder of the sd card like ```/mnt/sd``` or ```E:``` depending on the O/S.

