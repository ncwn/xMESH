pushd build
rm -rf can5_dist can5_dist.zip || true

mkdir -p can5_dist

cp flash_project_args can5_dist/
cp can5-app.bin can5_dist/
cp ota_data_initial.bin can5_dist/
mkdir -p can5_dist/bootloader && cp bootloader/bootloader.bin "$_"
mkdir -p can5_dist/partition_table && cp partition_table/partition-table.bin "$_"
zip -r can5_dist.zip can5_dist
popd
