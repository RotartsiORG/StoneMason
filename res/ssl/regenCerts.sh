#!/bin/bash
# Cert generation script. Modified version of https://github.com/nplab/DTLS-Examples/blob/master/cert.sh

cd res/ssl || exit

rm -rf legit
rm -rf dubious

mkdir dubious
mkdir legit

cd legit || exit

touch ca-db-index
echo 01 > ca-db-serial

# Certificate Authority
openssl req -nodes -x509 -newkey rsa:2048 -days 365 -keyout ca-priv-key.pem -out ca-pub-cert.pem

# Server Certificate
openssl req -nodes -new -newkey rsa:2048 -keyout serv-priv-key.pem -out serv.csr

# Sign Server Certificate
openssl ca -config ../ca.conf -days 365 -in serv.csr -out serv-pub-cert.pem

# Client Certificate
openssl req -nodes -new -newkey rsa:2048 -keyout cli-priv-key.pem -out cli.csr

# Sign Client Certificate
openssl ca -config ../ca.conf -days 365 -in cli.csr -out cli-pub-cert.pem


cd ../dubious || exit

touch ca-db-index
echo 01 > ca-db-serial

# Certificate Authority
openssl req -nodes -x509 -newkey rsa:2048 -days 365 -keyout ca-priv-key.pem -out ca-pub-cert.pem

# Server Certificate
openssl req -nodes -new -newkey rsa:2048 -keyout serv-priv-key.pem -out serv.csr

# Sign Server Certificate
openssl ca -config ../ca.conf -days 365 -in serv.csr -out serv-pub-cert.pem

# Client Certificate
openssl req -nodes -new -newkey rsa:2048 -keyout cli-priv-key.pem -out cli.csr

# Sign Client Certificate
openssl ca -config ../ca.conf -days 365 -in cli.csr -out cli-pub-cert.pem

cd ../../.. || exit
