#!/usr/bin/env bash

cd $(dirname ${0})

case `uname -s` in
    FreeBSD)
        pthreads="-lpthread"
        ;;
    *)
        pthreads="-pthread"
        ;;
esac

cc="clang"
cflags="$CFLAGS -I../include $pthreads -DTEST_CODE -Wno-format -g"
srcs="misc.c queue.c qalarm.c"
(cd src; $cc $cflags -o ../qalarm $srcs)
