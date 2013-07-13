#!/bin/sh
if [ -x /usr/sbin/apache2ctl ];
then
    CMD="/usr/sbin/apache2ctl"
else if [ -x /usr/sbin/apachectl ];
then 
    CMD="/usr/sbin/apachectl"
else
    CMD="apache2ctl"
fi
fi

$CMD -d `pwd` -f `pwd`/test/httpd.conf -X -k start
