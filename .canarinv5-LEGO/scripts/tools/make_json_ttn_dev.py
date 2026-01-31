import csv
import json

def make_single_dict(row):
    base = {}
    for key in row.keys():
        tmp = base
        slst = key.split('.')
        for val in slst:
            if val not in tmp:
                if val == slst[-1]:
                    if row[key] in ['TRUE', 'true', 'True', 'FALSE', 'False', 'false']:
                        tmp[val] = row[key].lower() == 'true'
                    else:
                        tmp[val] = row[key]
                else:
                    tmp[val] = {}
            
            tmp = tmp[val]

    return base



all = []
with open('ndata.csv', newline='') as csvfile:
    reader = csv.DictReader(csvfile)
    for row in reader:
       all.append(make_single_dict(row))


print(json.dumps(all, indent=2))
