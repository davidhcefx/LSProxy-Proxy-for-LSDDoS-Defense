DISTRO := $(shell sed -n -E 's/^NAME="(.*)"/\1/p' /etc/*release)
SUDO := sudo
SRC := $(addprefix src/, ls_proxy.cpp buffer.cpp client.cpp server.cpp connection.cpp)
# delayed expansions
CPP = $(shell if command -v g++-10 >/dev/null 2>&1; then echo 'g++-10'; \
            elif command -v g++-9 >/dev/null 2>&1; then echo 'g++-9'; \
            else echo 'g++'; fi)
CPPVER = $(shell $(CPP) --version | sed -n -E '1s/[^0-9]*([0-9]+).*/\1/p')
STDVER = $(shell if [ "$(CPPVER)" -ge 10 ]; then echo 'c++20'; else echo 'c++2a'; fi)
# pass a DEBUG=1 flag for debugging
CPPFLAGS = -Wall -Wextra -std=$(STDVER) $(if $(DEBUG), -g -Og, -O2)


all: g++9 libevent check_limit shorten_timeout
	$(MAKE) ls_proxy

ls_proxy: $(SRC) src/*.h
	$(CPP) $(CPPFLAGS) -o $@ $(SRC) src/llhttp/libllhttp.so -levent

simple_attack: utils/simple_attack.cpp
	$(CPP) $(CPPFLAGS) -o $@ $^

speed_limit_proxy: $(addprefix utils/, speed_limit_proxy.cpp buffer.cpp buffer.h)
	$(CPP) $(CPPFLAGS) -o $@ $(filter %.cpp,$^) -levent

test:
	make -C test all

utils/buffer.h:
	# replace the original include with our file
	cp src/buffer.h $@; \
	sed -i 's/^#include "ls_proxy.h".*/#include "speed_limit_proxy.h"/' $@

g++9:
	# check if g++-9 or above has been installed
	if [ "$(CPPVER)" -lt 9 ]; then \
		case "$(DISTRO)" in \
		*Ubuntu*|*Debian*) \
			# install 'add-apt-repository' \
			$(SUDO) apt update && $(SUDO) apt-get install -y software-properties-common && \
			$(SUDO) add-apt-repository -y ppa:ubuntu-toolchain-r/test && $(SUDO) apt-get update && \
			($(SUDO) apt-get install -y g++-10 || $(SUDO) apt-get install -y g++-9) ;; \
		*Fedora*) \
			$(SUDO) dnf install -y gcc-c++ ;; \
		*CentOS*) \
			$(SUDO) yum install -y gcc-toolset-9-gcc-c++ && \
			update-alternatives --install /usr/bin/g++-9 g++-9 /opt/rh/gcc-toolset-9/root/usr/bin/g++ 10 ;; \
		*) \
			 echo "Please install g++-9 or later manually in order to support c++20." ;; \
		esac \
	fi

libevent:
	# check if libevent has been installed
	if $(CPP) -levent 2>&1 | grep -q 'cannot find -levent'; then \
		case "$(DISTRO)" in \
		*Ubuntu*|*Debian*) \
			$(SUDO) apt-get update && $(SUDO) apt-get install -y libevent-dev ;; \
		*Fedora*) \
			$(SUDO) dnf install -y libevent-devel.x86_64 ;; \
		*CentOS*) \
			$(SUDO) yum install -y libevent-devel.x86_64 ;; \
		*) \
			echo "Please install libevent-dev manually on your platform." ;; \
		esac \
	fi

check_limit:
	./utils/check_rlimit_nofile_raisable.sh

shorten_timeout:
	./utils/shorten_connection_timeout.sh

clean: clean-test
	rm -f ls_proxy simple_attack speed_limit_proxy utils/buffer.h core*

clean-test:
	make -C test clean

.PHONY: all test g++9 libevent check_limit shorten_timeout clean
