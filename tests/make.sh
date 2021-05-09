#!/bin/bash

target=${1-"all"}
CC="gcc -std=gnu99 -D_DEFAULT_SOURCE -D_GNU_SOURCE -g -Wall -D_DEBUG -I../include -I../utils "

target=$(basename ${target})
target=${target/.[ch]/}

if ! pkg-config --modversion uuid >/dev/null 2>& 1 ; then
	sudo apt-get -y install uuid-dev
fi

# generate a random rsa key if not exists
SECKEY_FILE="seckey"
if [ ! -e "${SECKEY_FILE}.pem" ]; then
	echo "generate new rsa key pairs"
	# generate rsa privkey (use ssh-keygen or openssl tools)
	# ssh-keygen -t rsa -b 2048 -f ${SECKEY_FILE}.pem
	openssl genrsa -out ${SECKEY_FILE}.pem 2048
fi

if [ ! -e "${SECKEY_FILE}_pub.pem" ] ; then
	# output rsa pubkey (MUST be PKCS8 format) for libjwt-1.10.1
	# ssh-keygen -f ${SECKEY_FILE}.pem -e -m "PKCS8"
	openssl rsa -in ${SECKEY_FILE}.pem -pubout -out ${SECKEY_FILE}_pub.pem
fi

case "$target" in 
	test-jwt)
		echo "build ${target} ..."
		${CC} -o tests/${target} \
			tests/test-jwt.c \
			-lm -lpthread -ljwt -luuid
		;;
	*)
		echo "build nothing."
		;;
esac

