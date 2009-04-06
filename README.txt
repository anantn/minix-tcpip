This the the code which was written for Computer Network Practical coursework
More details about this course-work can be found at location
http://www.cs.vu.nl/~cn/

The problem statement can be also found in the "practicum_problem_statement.pdf".
The documentation about the approach and design are documented in
./cn/doc/readme.pdf

The entire code is written for Minix3 operating system, so it does not compile
directly on linux.
We use special library ( called ip-linux) for compilation and running of this library in linux.

Because of this, to compile this code on linux, use "Makefile.linux" instead
of "Makefile" ("Makfile" is aimed for Minix by default)

For setting up.

cd ./cn/ip-linux
make

cd ../tcp
make -f Makefile.linux

cd ../http
make -f Makefile.linux

cd ../test -f Makefile.linux

For Running
To run the test programs in ./test directory,
you will need to pass environment variable "ETH" with value 1 for server and 2
for client

You can run listening program as follows,
$ ETH=1 ./tcp_listen

and you can run client side as follows
$ ETH=2 ./tcp_connect

