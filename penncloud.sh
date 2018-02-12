# master
./masterNode -v backend.txt &

# backend for drive (only one replica for speed)
./keyValueStore -v backend.txt 0 &
./keyValueStore -v backend.txt 3 &
./keyValueStore -v backend.txt 6 &

# backend for mail (only one replica for speed)
./keyValueStore -v backend.txt 9 &
./keyValueStore -v backend.txt 12 &

# backend fo user (only one replica for speed)
./keyValueStore -v backend.txt 15 &

# load balancers
./loadbalancer -p 8000 frontend.txt &
./loadbalancer -p 9000 frontend.txt &
./loadbalancer -p 10000 frontend.txt &

# frontend for drive
./storage -v -p 8001 backend.txt &
./storage -v -p 8002 backend.txt &
./storage -v -p 8003 backend.txt &

# frontend for mail
./mail -v -p 9001 backend.txt &
./mail -v -p 9002 backend.txt &
./mail -v -p 9003 backend.txt &

# frontend for user
./httpServer -v -p 10001 backend.txt &
./httpServer -v -p 10002 backend.txt &
./httpServer -v -p 10003 backend.txt &

# admin console
./admin -v -p 8080 backend.txt &