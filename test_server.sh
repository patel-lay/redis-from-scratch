#!/bin/bash

# Set the number of clients to run
num_clients=500

# Set the command to run for each client (replace with your actual client command)
client_command="./client set test"

# Loop to start multiple client processes in the background
for i in $(seq 1 $num_clients); do
  #echo "$client_command${i} ${i}"
  $client_command${i} ${i}
 #$command 
done

client_command_response="./client get test" 
for i in $(seq 1 $num_clients); do
 # echo "$client_command_response${i}"
  $client_command_response${i}
done

# Wait for all background processes to complete
wait

echo "All clients finished."