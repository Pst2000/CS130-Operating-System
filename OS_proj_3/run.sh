cd userprog/
make clean
make
cd build/
make check;
# pintos -v --gdb --qemu  --filesys-size=2 -p tests/userprog/args-none -a args-none -- -q  -f run args-none;
# pintos -v -k -T 60 --qemu  --filesys-size=2 -p tests/userprog/args-single -a args-single -- -q  -f run 'args-single onearg';

# pintos -v --gdb --qemu  --filesys-size=2 -p tests/userprog/args-none -a args-none -- -q  -f run args-none;

# pintos -v -k -T 60 --qemu  --filesys-size=2 -p tests/userprog/create-normal -a create-normal -- -q  -f run create-normal
# pintos -v -k -T 60 --qemu  --filesys-size=2 -p tests/userprog/create-null -a create-null -- -q  -f run create-null

# pintos -v -k -T 60 --qemu  --filesys-size=2 -p tests/userprog/create-bad-ptr -a create-bad-ptr -- -q  -f run create-bad-ptr

# question here:
# pintos -v -k -T 60 --qemu  --filesys-size=2 -p tests/userprog/sc-boundary-3 -a sc-boundary-3 -- -q  -f run sc-boundary-3

# pintos -v -k -T 60 --qemu  --filesys-size=2 -p tests/userprog/open-twice -a open-twice -- -q  -f run open-twice
# pintos -v -k -T 60 --qemu  --filesys-size=2 -p tests/userprog/read-normal -a read-normal -p ../../tests/userprog/sample.txt -a sample.txt -- -q  -f run read-normal
# pintos -v -k -T 60 --qemu  --filesys-size=2 -p tests/userprog/read-stdout -a read-stdout -- -q  -f run read-stdout
# pintos -v -k -T 60 --qemu  --filesys-size=2 -p tests/userprog/write-zero -a write-zero -p ../../tests/userprog/sample.txt -a sample.txt -- -q  -f run write-zero
# pintos -v -k -T 60 --qemu  --filesys-size=2 -p tests/userprog/write-boundary -a write-boundary -p ../../tests/userprog/sample.txt -a sample.txt -- -q  -f run write-boundary

# pintos -v -k -T 60 --qemu  --filesys-size=2 -p tests/userprog/close-stdin -a close-stdin -- -q  -f run close-stdin

# pintos -v -k -T 60 --qemu  --filesys-size=2 -p tests/userprog/exec-once -a exec-once -p tests/userprog/child-simple -a child-simple -- -q  -f run exec-once
# pintos -v -k -T 60 --qemu  --filesys-size=2 -p tests/userprog/exec-missing -a exec-missing -- -q  -f run exec-missin
# pintos -v -k -T 60 --qemu  --filesys-size=2 -p tests/userprog/exec-multiple -a exec-multiple -p tests/userprog/child-simple -a child-simple -- -q  -f run exec-multiple
# pintos -v -k -T 360 --qemu  --filesys-size=2 -p tests/userprog/no-vm/multi-oom -a multi-oom -- -q  -f run multi-oom

# pintos -v -k -T 60 --qemu  --filesys-size=2 -p tests/filesys/base/syn-write -a syn-write -p tests/filesys/base/child-syn-wrt -a child-syn-wrt -- -q  -f run syn-write
# pintos -v -k -T 60 --qemu  --filesys-size=2 -p tests/userprog/bad-read -a bad-read -- -q  -f run bad-read 
# pintos -v -k -T 60 --qemu  --filesys-size=2 -p tests/userprog/bad-jump -a bad-jump -- -q  -f run bad-jump