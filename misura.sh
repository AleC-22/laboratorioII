#!/bin/bash
shopt -s lastpipe

client_totali=0
client_corretti=0
client_id_client=""
secret_client=""
client_id_server=""
secret_server=""
secret_minimo=0
secret_massimo=0

grep "SECRET" $1 | while read -r riga_client ; do
    client_totali=$((client_totali+1))
    secret_client=$(echo $riga_client | cut -d" " -f4)
    client_id_client=$(echo $riga_client | cut -d" " -f2)
    grep -E "\s+[a-z0-9]+\s+\|+\s" $2 | while read -r riga_server ; do
        client_id_server=$(echo $riga_server | cut -d" " -f1)
        secret_server=$(echo $riga_server | cut -d" " -f3)
        if [ "$client_id_client" == "$client_id_server" ] ; then
            secret_minimo=$((secret_client-25))
            secret_massimo=$((secret_client+25))
            if [ "$secret_server" -le "$secret_massimo" ] && [ "$secret_server" -ge "$secret_minimo" ] ; then
                client_corretti=$((client_corretti+1))
                echo "Client: $client_id_client corretto"
            fi
        fi
    done
done
echo "Client corretti: $client_corretti su $client_totali"
