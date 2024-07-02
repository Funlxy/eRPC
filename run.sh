processes=${1} #processes
for (( i=0; i<${processes}; ++i ))
do 
client_port=$(( 31851+${i} ))
sudo ./build/hello_client -client_port=${client_port} &
done