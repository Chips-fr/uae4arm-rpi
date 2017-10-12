#!/bin/sh -e
echo "Boostrapping capsimg..."
cd CAPSImg
rm -Rf autom4te.cache
echo "Running autoheader"
autoheader
echo "Running autoconf"
autoconf

cd ..
cp configure.in configure
chmod a+x configure

echo "Bootstrap done, you can now run ./configure"
