md5_out=$(md5sum "$1")
hash=${md5_out:0:32}

printf 'const uint8_t ROLO_SCHEMA_HASH[16] = { ' >rolo_schema_hash.h
for (( i=0; i<${#hash}; i+=2 )); do
	printf "0x${hash:i:2}, " >>rolo_schema_hash.h
done
echo '};' >>rolo_schema_hash.h

