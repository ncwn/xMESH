# Change this directory according to the system
PYTHONDEVELDIR=~/miniconda3/envs/lorarelay/include/python3.8/
mkdir -p can5_loramsg
PBUILDDIR=can5_loramsg
CDIR=../../components/can5_net/can5_protocols/can5_lorarelay

mkdir -p $PBUILDDIR
rm $PBUILDDIR/can5_loramsg.c $PBUILDDIR/can5_loramsg.h || true
cp $CDIR/can5_loramsg.c $PBUILDDIR/
cp $CDIR/can5_loramsg.h $PBUILDDIR/
pushd $PBUILDDIR
swig -python can5_loramsg.i
gcc -fPIC -c can5_loramsg.c can5_loramsg_wrap.c -Iinclude -I$PYTHONDEVELDIR
ld -shared can5_loramsg.o can5_loramsg_wrap.o -o _can5_loramsg.so
touch __init__.py
popd
