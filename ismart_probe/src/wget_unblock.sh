while true
do
        num=`ps | grep "wget \-t 1" | wc -l`
        if [ $num -ne 0 ];then
                echo $num
                sleep 4
                killall wget
        fi
        sleep 1
done
