DISTRO := $(shell sed -n -E 's/^NAME="(.*)"/\1/p' /etc/*release)
CPP := $(shell if command -v g++-9 >/dev/null 2>&1; then echo 'g++-9'; \
            elif command -v g++-8 >/dev/null 2>&1; then echo 'g++-8'; \
            else echo 'g++'; fi)
CPPVER := $(shell $(CPP) --version | sed -n -E '1s/[^0-9]*([0-9]+).*/\1/p')
STD := $(shell if [ $(CPPVER) -ge 9 ]; then echo 'c++20'; else echo 'c++2a'; fi)
CPPFLAGS := -Wall -Wextra -std=$(STD) -g -Og -Wno-attributes
#CPPFLAGS := -Wall -Wextra -std=$(STD) -O2


all: g++8 libevent raise_limit simple_attack ls_proxy

simple_attack: src/simple_attack.cpp
	$(CPP) $(CPPFLAGS) -o $@ $^

ls_proxy: src/ls_proxy.cpp src/buffer.cpp src/client.cpp src/server.cpp src/connection.cpp
	$(CPP) $(CPPFLAGS) -o $@ $^ src/llhttp/libllhttp.so -levent

g++8:
	# check if g++ 8 or above has been installed
	if [ $(CPPVER) -lt 8 ]; then \
		case "$(DISTRO)" in \
		*Ubuntu*|*Debian*) \
			sudo apt-get update && sudo apt-get install -y g++-8;; \
		*Fedora*) \
			sudo dnf install -y gcc-c++;; \
		*CentOS*) \
			sudo yum install -y centos-release-scl devtoolset-8-gcc-c++;; \
		*) \
			 echo "Please install g++-8 or later manually on your platform.";; \
		esac \
	fi

libevent:
	# check if libevent has been installed
	if [ -z "`ldconfig -p | grep libevent`" ]; then \
		case "$(DISTRO)" in \
		*Ubuntu*|*Debian*) \
			sudo apt-get update && sudo apt-get install -y libevent-dev;; \
		*Fedora*) \
			sudo dnf install -y libevent-devel.x86_64;; \
		*CentOS*) \
			sudo yum install -y libevent-devel.x86_64;; \
		*) \
			echo "Please install libevent-dev manually on your platform.";; \
		esac \
	fi

raise_limit:
	./utils/raise_nofile_limit.sh

clean:
	rm -f simple_attack ls_proxy

.PHONY: all clean raise_limit gnu++2a libevent
