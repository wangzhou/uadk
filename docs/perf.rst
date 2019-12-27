
App: test_bind_api
The test app generate pseudo-random number for test so no input file required.
Output will be double checked with inflate, which is ignored if "-o perf"

para:
-o perf: ignore output check, for better performance
	 non-sva mode simulate memcpy output buffer
-c: number of caches to run together, for batch processing.


1. sva mode
sudo rmmod hisi_zip; sudo rmmod hisi_qm; sudo rmmod uacce;
sudo insmod uacce.ko; sudo insmod hisi_qm.ko; sudo insmod hisi_zip.ko;

//add memset
sudo ./test_bind_api -b 8192 -s 81920000 -o perf -c 50
Compress bz=8192, speed=7089.383 MB/s
sudo ./test_bind_api -b 8192 -s 81920000 -o perf
Compress bz=8192, speed=580.303 MB/s

Conslusion:
a. Add memset (hack in the code) to trigger page fault early in cpu instead of in the smmu
   Can improve performance a lot
b. -c 50 can improve performance a lot, batch process 50 commands.
c. -q, multi-queue has no impact to performance


2. nosva:
sudo rmmod hisi_zip; sudo rmmod hisi_qm; sudo rmmod uacce;
sudo insmod uacce.ko uacce_nosva=1; sudo insmod hisi_qm.ko; sudo insmod hisi_zip.ko;
sudo ./test_bind_api -b 8192 -s 81920000 -o perf
Compress bz=8192, speed=2203.808 MB/s

Conclusion:
a, Already add memcpy when -o perf to simulate real case
If no memcpy, speed = 5G/s
b, memset, -c, -q has no impact to performance


3. test.sh

#!/bin/bash

block=8192
size=81920000

for i in {1..10}
do
	let "size*=$i"
	echo $i $size
	sudo ./test_bind_api -b $block -s $size -o perf -c 50
done

Conslusion:
a. only support 5w packages at most, since no enough memory for malloc
b. sva mode:
When pacakge larger than 4w, performance downgrade from 6G to 300M since page fault happen
The reason may caused by migration, known issue

log:
./test.sh
1 81920000
Compress bz=8192, speed=7033.219 MB/s
2 163840000
Compress bz=8192, speed=6593.940 MB/s
3 491520000
Compress bz=8192, speed=6595.981 MB/s
4 1966080000
Compress bz=8192, speed=324.536 MB/s
5 9830400000
Compress bz=8192, speed=532.281 MB/s
6 58982400000
7 412876800000
8 3303014400000
9 29727129600000
10 297271296000000
