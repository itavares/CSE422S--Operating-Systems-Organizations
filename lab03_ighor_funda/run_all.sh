 #!/bin/bash   

ARRAY=(64 128 256 512)
declare -i iter

for size in "${ARRAY[@]}"  
do
    iter=1
    while [ "$iter" -le 10 ]
    do
        sudo ./dense_mm $size
        sleep 1
        iter=`expr $iter + 1`
    done
    echo done $size
done







