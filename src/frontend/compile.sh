set -x

g++ -shared -fPIC -DHAVE_CONFIG_H -I. -I../.. -I./../util -I./../decoder -I./../display -I./../input -I./../encoder -I./../net -DX264_API_IMPORTS -I/usr/local/include -I./../../../ephe-store/include/function_interface -std=c++14 -pthread -pedantic -Wall -Wextra -Weffc++ -DNDEBUG -g -O2 -MT xc-dump-func.o  -MD -MP -MF .deps/xc-dump-func.Tpo -c -o xc-dump-func.o xc-dump-func.cc

g++ -shared -fPIC -o xc-dump.so xc-dump-func.o ../input/libalfalfainput.a ../decoder/libalfalfadecoder.a ../util/libalfalfautil.a -L~/excamera_project/x264 -lx264 -ljpeg

kubectl cp xc-dump.so function-nodes-9wlrp:/dev/shm -c function-1

kubectl cp xc-dump.so function-nodes-9wlrp:/dev/shm -c function-2

g++ -shared -fPIC -DHAVE_CONFIG_H -I. -I../.. -I./../util -I./../decoder -I./../display -I./../input -I./../encoder -I./../net -DX264_API_IMPORTS -I/usr/local/include -I./../../../ephe-store/include/function_interface -std=c++14 -pthread -pedantic -Wall -Wextra -Weffc++ -DNDEBUG -g -O2 -MT xc-enc-func.o  -MD -MP -MF .deps/xc-enc-func.Tpo -c -o xc-enc-func.o xc-enc-func.cc

g++ -shared -fPIC -o xc-enc.so xc-enc-func.o ../encoder/libalfalfaencoder.a ../input/libalfalfainput.a ../decoder/libalfalfadecoder.a ../util/libalfalfautil.a -L~/excamera_project/x264 -lx264  -ljpeg



kubectl cp xc-enc.so function-nodes-9wlrp:/dev/shm -c function-1

kubectl cp xc-enc.so function-nodes-9wlrp:/dev/shm -c function-2

